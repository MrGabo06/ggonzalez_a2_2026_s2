// Sequential separable convolution: two 1D passes instead of one 2D pass.
// Needs filter.col_weights and filter.row_weights (see common/filter.h).
#include <algorithm>
#include <cmath>
#include <vector>

#include "backends/backend.h"

namespace {

class SeqSeparableBackend : public Backend {
public:
    std::string name() const override { return "seq_sep"; }

    void run(const Image& in, const Filter& f, Image& out) override {
        const int r = f.size / 2;
        out = Image(in.width, in.height);

        // Result of the horizontal pass, kept as float so nothing is rounded yet
        std::vector<float> temp(static_cast<size_t>(in.width) * in.height);

        // Pass 1: horizontal, each pixel mixes the pixels of its own row
        for (int y = 0; y < in.height; ++y) {
            for (int x = 0; x < in.width; ++x) {
                float acc = 0.0f;
                for (int kx = -r; kx <= r; ++kx) {
                    // Border: replicate the nearest pixel (same rule as the baseline)
                    const int sx = std::clamp(x + kx, 0, in.width - 1);
                    acc += f.row_weights[kx + r] * in.at(sx, y);
                }
                temp[static_cast<size_t>(y) * in.width + x] = acc;
            }
        }

        // Pass 2: vertical, each pixel mixes the pass 1 values of its own column
        for (int y = 0; y < in.height; ++y) {
            for (int x = 0; x < in.width; ++x) {
                float acc = 0.0f;
                for (int ky = -r; ky <= r; ++ky) {
                    const int sy = std::clamp(y + ky, 0, in.height - 1);
                    acc += f.col_weights[ky + r] * temp[static_cast<size_t>(sy) * in.width + x];
                }
                // Sobel can be negative: absolute value, then saturate to [0, 255]
                const float v = std::fabs(acc);
                out.at(x, y) = static_cast<uint8_t>(std::min(v + 0.5f, 255.0f));
            }
        }
    }
};

}  // namespace

std::unique_ptr<Backend> make_seq_separable_backend() { return std::make_unique<SeqSeparableBackend>(); }