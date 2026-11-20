// Backend "cuda" (propuesta 1): un hilo por píxel, todas las lecturas a memoria global.
#include <cuda_runtime.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

#include "backends/backend.h"
#include "backends/cuda/cuda_common.h"

namespace {

using namespace dlp_cuda;

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

}  // namespace

std::unique_ptr<Backend> make_cuda_backend() { return std::make_unique<CudaBackend>(); }
