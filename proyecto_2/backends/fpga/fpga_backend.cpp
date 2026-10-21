// Stub del backend FPGA. Se reemplaza en F5 cuando haya acceso a la tarjeta.
#include <stdexcept>

#include "backends/backend.h"

namespace {

class FpgaBackend : public Backend {
public:
    std::string name() const override { return "fpga"; }
    bool available() const override { return false; }

    void run(const Image&, const Filter&, Image&) override {
        throw std::runtime_error("Backend FPGA no disponible todavía");
    }
};

}  // namespace

std::unique_ptr<Backend> make_fpga_backend() { return std::make_unique<FpgaBackend>(); }