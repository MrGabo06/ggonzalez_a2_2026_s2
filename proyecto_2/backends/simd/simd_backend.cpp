// Backend SIMD (AVX2 / AVX-512). Por ahora es un stub: la implementación llega en F3.
#include <stdexcept>

#include "backends/backend.h"

namespace {

class SimdBackend : public Backend {
public:
    std::string name() const override { return "simd"; }

    void run(const Image&, const Filter&, Image&) override {
        throw std::runtime_error("Backend SIMD pendiente (F3)");
    }
};

}  // namespace

std::unique_ptr<Backend> make_simd_backend() { return std::make_unique<SimdBackend>(); }