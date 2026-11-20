// Backend "cuda_tiled" (propuesta 2): tiles en memoria compartida.
#include <cuda_runtime.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>

#include "backends/backend.h"
#include "backends/cuda/cuda_common.h"

namespace {

using namespace dlp_cuda;

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

std::unique_ptr<Backend> make_cuda_tiled_backend() {
    return std::make_unique<CudaTiledBackend>();
}
