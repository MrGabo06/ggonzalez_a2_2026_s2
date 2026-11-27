// Backend secuencial (baseline): convolución 2D directa, un píxel a la vez.
// Es la referencia contra la que se validan y comparan los demás backends.
#include <algorithm>
#include <cmath>

#include "backends/backend.h"

namespace {

class SeqBackend : public Backend {
public:
    std::string name() const override { return "seq"; }

    void run(const Image& in, const Filter& f, Image& out) override {
        const int r = f.size / 2;
        out = Image(in.width, in.height);

        for (int y = 0; y < in.height; ++y) {
            for (int x = 0; x < in.width; ++x) {
                float acc = 0.0f;
                for (int ky = -r; ky <= r; ++ky) {
                    // Borde: se replica el píxel más cercano (clamp)
                    const int sy = std::clamp(y + ky, 0, in.height - 1);
                    for (int kx = -r; kx <= r; ++kx) {
                        const int sx = std::clamp(x + kx, 0, in.width - 1);
                        acc += f.weights[(ky + r) * f.size + (kx + r)] * in.at(sx, sy);
                    }
                }
                // Sobel puede dar negativos: se usa el valor absoluto y se satura a [0, 255]
                const float v = std::fabs(acc);
                out.at(x, y) = static_cast<uint8_t>(std::min(v + 0.5f, 255.0f));
            }
        }
    }
};

}  // namespace

std::unique_ptr<Backend> make_seq_backend() { return std::make_unique<SeqBackend>(); }