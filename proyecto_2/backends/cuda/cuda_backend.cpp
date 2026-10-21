// Backend CUDA. Por ahora es un stub: la implementación llega en F3.
#include <stdexcept>

#include "backends/backend.h"

namespace {

class CudaBackend : public Backend {
public:
    std::string name() const override { return "cuda"; }

    void run(const Image&, const Filter&, Image&) override {
        throw std::runtime_error("Backend CUDA pendiente (F3)");
    }
};

}  // namespace

std::unique_ptr<Backend> make_cuda_backend() { return std::make_unique<CudaBackend>(); }