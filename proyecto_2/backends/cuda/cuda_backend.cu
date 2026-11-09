#include <cuda_runtime.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>

#include "backends/backend.h"

namespace {

// Convierte un error de CUDA en una excepción con el nombre de la llamada.
#define CUDA_CHECK(call)                                                           \
    do {                                                                           \
        const cudaError_t err_ = (call);                                           \
        if (err_ != cudaSuccess) {                                                 \
            throw std::runtime_error(std::string("CUDA: ") + cudaGetErrorString(err_) + \
                                     " en " #call);                                \
        }                                                                          \
    } while (0)

constexpr int kBlockX = 16;  // hilos por bloque en x
constexpr int kBlockY = 16;  // hilos por bloque en y

// KERNEL-BEGIN
// Cada hilo calcula un píxel de salida. Todas las lecturas van a memoria global.
__global__ void ConvolveGlobal(const uint8_t* in, uint8_t* out, int w, int h,
                               const float* weights, int size) {
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;
    // La malla se redondea hacia arriba, así que sobran hilos fuera de la imagen.
    if (x >= w || y >= h) return;

    const int r = size / 2;
    float acc = 0.0f;
    for (int ky = -r; ky <= r; ++ky) {
        // Borde: se replica el píxel más cercano (clamp), igual que el baseline
        const int sy = min(max(y + ky, 0), h - 1);
        for (int kx = -r; kx <= r; ++kx) {
            const int sx = min(max(x + kx, 0), w - 1);
            acc += weights[(ky + r) * size + (kx + r)] *
                   in[static_cast<size_t>(sy) * w + sx];
        }
    }
    // Sobel puede dar negativos: valor absoluto, +0.5, saturar a 255 y truncar
    const float v = fabsf(acc);
    out[static_cast<size_t>(y) * w + x] = static_cast<uint8_t>(fminf(v + 0.5f, 255.0f));
}
// KERNEL-END

// ---------------------------------------------------------------------------
// Propuesta 2 (cuda_tiled): tiles en memoria compartida.
//
// En la propuesta 1 cada hilo lee sus 25 (o 9) vecinos de memoria global, y los hilos
// vecinos leen casi los mismos píxeles. Aquí cada bloque copia una sola vez a memoria
// compartida (rápida, dentro del chip) el tile que necesita: su bloque de píxeles más
// un margen de r píxeles por cada lado (el halo). Después todos los hilos calculan
// leyendo de ahí. El clamp del borde se aplica al copiar, así que el resultado es el
// mismo que el del baseline.
//
// El tamaño del bloque es configurable con la variable de entorno DLP_CUDA_BLOCK
// (por ejemplo DLP_CUDA_BLOCK=32x8); por defecto 16x16.
// ---------------------------------------------------------------------------

constexpr int kMaxWeights = 25;  // filtro más grande: 5x5

// Los pesos son pequeños y todos los hilos leen los mismos: memoria constante.
__constant__ float c_weights[kMaxWeights];

// TILED-BEGIN
// Cada bloque carga su tile (bloque + halo) a memoria compartida y luego cada hilo
// calcula un píxel leyendo solo de ahí.
__global__ void ConvolveTiled(const uint8_t* in, uint8_t* out, int w, int h, int size) {
    extern __shared__ uint8_t tile[];  // tamaño dinámico: tile_w * tile_h bytes

    const int r = size / 2;
    const int tile_w = blockDim.x + 2 * r;
    const int tile_h = blockDim.y + 2 * r;
    // Esquina superior izquierda del tile en la imagen (el halo queda fuera del bloque)
    const int x0 = blockIdx.x * blockDim.x - r;
    const int y0 = blockIdx.y * blockDim.y - r;

    // Carga cooperativa: los hilos reparten los píxeles del tile (hay más que hilos).
    const int tid = threadIdx.y * blockDim.x + threadIdx.x;
    const int threads = blockDim.x * blockDim.y;
    for (int i = tid; i < tile_w * tile_h; i += threads) {
        const int gx = min(max(x0 + i % tile_w, 0), w - 1);  // clamp en el borde
        const int gy = min(max(y0 + i / tile_w, 0), h - 1);
        tile[i] = in[static_cast<size_t>(gy) * w + gx];
    }
    // Todos los hilos del bloque deben terminar de cargar antes de que alguno calcule.
    // Por eso este sync va antes del return de los hilos fuera de la imagen.
    __syncthreads();

    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= w || y >= h) return;

    float acc = 0.0f;
    for (int ky = -r; ky <= r; ++ky) {
        // El píxel (x, y) está en la posición (threadIdx + r) dentro del tile
        const uint8_t* row = tile + (threadIdx.y + r + ky) * tile_w + (threadIdx.x + r);
        for (int kx = -r; kx <= r; ++kx) {
            acc += c_weights[(ky + r) * size + (kx + r)] * row[kx];
        }
    }
    const float v = fabsf(acc);
    out[static_cast<size_t>(y) * w + x] = static_cast<uint8_t>(fminf(v + 0.5f, 255.0f));
}
// TILED-END

// Mide con eventos de CUDA cuánto tarda el kernel en la GPU, sin las copias de memoria.
// Se crea en init() y no en el constructor: así, sin GPU, el backend responde "no
// disponible" en vez de fallar al crearse.
class KernelTimer {
public:
    KernelTimer() {
        CUDA_CHECK(cudaEventCreate(&start_));
        CUDA_CHECK(cudaEventCreate(&stop_));
    }
    ~KernelTimer() {
        cudaEventDestroy(start_);
        cudaEventDestroy(stop_);
    }
    KernelTimer(const KernelTimer&) = delete;
    KernelTimer& operator=(const KernelTimer&) = delete;

    void Start() { CUDA_CHECK(cudaEventRecord(start_)); }

    // Espera a que termine el kernel y devuelve su duración en ms
    double Stop() {
        CUDA_CHECK(cudaEventRecord(stop_));
        CUDA_CHECK(cudaEventSynchronize(stop_));
        float ms = 0.0f;
        CUDA_CHECK(cudaEventElapsedTime(&ms, start_, stop_));
        return ms;
    }

private:
    cudaEvent_t start_ = nullptr;
    cudaEvent_t stop_ = nullptr;
};


class CudaBackend : public Backend {
public:
    ~CudaBackend() override { Release(); }

    std::string name() const override { return "cuda"; }

    // false si no hay driver o no hay GPU NVIDIA visible.
    bool available() const override {
        int count = 0;
        return cudaGetDeviceCount(&count) == cudaSuccess && count > 0;
    }

    // Reserva la memoria de la GPU y sube los pesos del filtro (una sola vez).
    void init(const Image& input, const Filter& f) override {
        Release();
        timer_ = std::make_unique<KernelTimer>();
        width_ = input.width;
        height_ = input.height;
        pixels_ = static_cast<size_t>(width_) * height_;

        CUDA_CHECK(cudaMalloc(&d_in_, pixels_));
        CUDA_CHECK(cudaMalloc(&d_out_, pixels_));
        CUDA_CHECK(cudaMalloc(&d_weights_, f.weights.size() * sizeof(float)));
        CUDA_CHECK(cudaMemcpy(d_weights_, f.weights.data(), f.weights.size() * sizeof(float),
                              cudaMemcpyHostToDevice));
    }

    void run(const Image& in, const Filter& f, Image& out) override {
        if (!d_in_ || in.width != width_ || in.height != height_) {
            throw std::runtime_error("CUDA: run() necesita la misma imagen que init()");
        }
        out = Image(width_, height_);

        const dim3 block(kBlockX, kBlockY);
        const dim3 grid((width_ + kBlockX - 1) / kBlockX, (height_ + kBlockY - 1) / kBlockY);

        // Imagen a la GPU, kernel y resultado de vuelta (la copia final espera al kernel)
        CUDA_CHECK(cudaMemcpy(d_in_, in.data.data(), pixels_, cudaMemcpyHostToDevice));
        timer_->Start();
        ConvolveGlobal<<<grid, block>>>(d_in_, d_out_, width_, height_, d_weights_, f.size);
        CUDA_CHECK(cudaGetLastError());
        kernel_ms_ = timer_->Stop();
        CUDA_CHECK(cudaMemcpy(out.data.data(), d_out_, pixels_, cudaMemcpyDeviceToHost));
    }

    void cleanup() override { Release(); }

    // Solo el kernel, sin las copias de la imagen (ver KernelTimer)
    double kernel_ms() const override { return kernel_ms_; }

private:
    // Libera la memoria de la GPU. No lanza excepciones, también se usa en el destructor.
    void Release() {
        timer_.reset();
        cudaFree(d_in_);
        cudaFree(d_out_);
        cudaFree(d_weights_);
        d_in_ = nullptr;
        d_out_ = nullptr;
        d_weights_ = nullptr;
    }

    int width_ = 0;
    int height_ = 0;
    size_t pixels_ = 0;
    uint8_t* d_in_ = nullptr;
    std::unique_ptr<KernelTimer> timer_;
    double kernel_ms_ = -1.0;
    uint8_t* d_out_ = nullptr;
    float* d_weights_ = nullptr;
};

class CudaTiledBackend : public Backend {
public:
    CudaTiledBackend() { ReadBlockSize(); }
    ~CudaTiledBackend() override { Release(); }

    std::string name() const override { return "cuda_tiled"; }

    bool available() const override {
        int count = 0;
        return cudaGetDeviceCount(&count) == cudaSuccess && count > 0;
    }

    // Reserva la memoria de la GPU y sube los pesos a memoria constante (una vez).
    void init(const Image& input, const Filter& f) override {
        Release();
        timer_ = std::make_unique<KernelTimer>();
        if (f.size * f.size > kMaxWeights) {
            throw std::runtime_error("CUDA: el filtro es mas grande que 5x5");
        }
        width_ = input.width;
        height_ = input.height;
        pixels_ = static_cast<size_t>(width_) * height_;

        CUDA_CHECK(cudaMalloc(&d_in_, pixels_));
        CUDA_CHECK(cudaMalloc(&d_out_, pixels_));
        CUDA_CHECK(cudaMemcpyToSymbol(c_weights, f.weights.data(),
                                      f.weights.size() * sizeof(float)));
    }

    void run(const Image& in, const Filter& f, Image& out) override {
        if (!d_in_ || in.width != width_ || in.height != height_) {
            throw std::runtime_error("CUDA: run() necesita la misma imagen que init()");
        }
        out = Image(width_, height_);

        const int r = f.size / 2;
        const dim3 block(block_x_, block_y_);
        const dim3 grid((width_ + block_x_ - 1) / block_x_, (height_ + block_y_ - 1) / block_y_);
        // Memoria compartida por bloque: el tile con su halo, un byte por píxel
        const size_t shared_bytes = static_cast<size_t>(block_x_ + 2 * r) * (block_y_ + 2 * r);

        CUDA_CHECK(cudaMemcpy(d_in_, in.data.data(), pixels_, cudaMemcpyHostToDevice));
        timer_->Start();
        ConvolveTiled<<<grid, block, shared_bytes>>>(d_in_, d_out_, width_, height_, f.size);
        CUDA_CHECK(cudaGetLastError());
        kernel_ms_ = timer_->Stop();
        CUDA_CHECK(cudaMemcpy(out.data.data(), d_out_, pixels_, cudaMemcpyDeviceToHost));
    }

    void cleanup() override { Release(); }

    // Solo el kernel, sin las copias de la imagen (ver KernelTimer)
    double kernel_ms() const override { return kernel_ms_; }

private:
    // Lee DLP_CUDA_BLOCK=<x>x<y>. Si falta o no es válida, deja 16x16.
    void ReadBlockSize() {
        const char* text = std::getenv("DLP_CUDA_BLOCK");
        if (!text) return;
        int bx = 0, by = 0;
        char sep = 0;
        // Un bloque admite como máximo 1024 hilos
        if (std::sscanf(text, "%d%c%d", &bx, &sep, &by) == 3 && (sep == 'x' || sep == 'X') &&
            bx > 0 && by > 0 && bx * by <= 1024) {
            block_x_ = bx;
            block_y_ = by;
        }
    }

    void Release() {
        timer_.reset();
        cudaFree(d_in_);
        cudaFree(d_out_);
        d_in_ = nullptr;
        d_out_ = nullptr;
    }

    int block_x_ = kBlockX;
    int block_y_ = kBlockY;
    int width_ = 0;
    int height_ = 0;
    size_t pixels_ = 0;
    uint8_t* d_in_ = nullptr;
    std::unique_ptr<KernelTimer> timer_;
    double kernel_ms_ = -1.0;
    uint8_t* d_out_ = nullptr;
};

}  // namespace

std::unique_ptr<Backend> make_cuda_backend() { return std::make_unique<CudaBackend>(); }
std::unique_ptr<Backend> make_cuda_tiled_backend() { return std::make_unique<CudaTiledBackend>(); }
