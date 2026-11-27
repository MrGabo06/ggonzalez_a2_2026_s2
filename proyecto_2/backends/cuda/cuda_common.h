// Utilidades compartidas por los backends CUDA ("cuda" y "cuda_tiled").
#pragma once
#include <cuda_runtime.h>

#include <stdexcept>
#include <string>

// Convierte un error de CUDA en una excepción con el nombre de la llamada.
#define CUDA_CHECK(call)                                                           \
    do {                                                                           \
        const cudaError_t err_ = (call);                                           \
        if (err_ != cudaSuccess) {                                                 \
            throw std::runtime_error(std::string("CUDA: ") + cudaGetErrorString(err_) + \
                                     " en " #call);                                \
        }                                                                          \
    } while (0)

namespace dlp_cuda {

constexpr int kBlockX = 16;  // hilos por bloque en x
constexpr int kBlockY = 16;  // hilos por bloque en y

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

}  // namespace dlp_cuda
