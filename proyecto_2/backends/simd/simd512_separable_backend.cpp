// Backend "simd_sep512": convolución separable con AVX-512, 16 píxeles por iteración.
// Misma estructura que "simd_sep" (AVX2): solo cambia el ancho del vector. Se compila
// aparte, con -mavx512f (ver Makefile).
#include <immintrin.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "backends/backend.h"
#include "backends/simd/simd_avx512.h"
#include "backends/simd/simd_common.h"

namespace {

using namespace dlp_simd;

constexpr int kLanes = 16;  // píxeles por registro de 512 bits (16 floats)

class SimdSeparable512Backend : public Backend {
public:
    std::string name() const override { return "simd_sep512"; }

    bool available() const override { return __builtin_cpu_supports("avx512f"); }

    void run(const Image& in, const Filter& f, Image& out) override {
        const int r = f.size / 2;
        const int w = in.width;
        const int h = in.height;
        out = Image(w, h);

        // Buffer intermedio en float, sin redondear entre pasadas.
        std::vector<float> temp(static_cast<size_t>(w) * h);

        __m512 row_w[5];
        __m512 col_w[5];
        for (int i = 0; i < f.size; ++i) {
            row_w[i] = _mm512_set1_ps(f.row_weights[i]);
            col_w[i] = _mm512_set1_ps(f.col_weights[i]);
        }

        // Pasada 1: horizontal.
        for (int y = 0; y < h; ++y) {
            const uint8_t* row = in.data.data() + static_cast<size_t>(y) * w;
            float* temp_row = temp.data() + static_cast<size_t>(y) * w;

            auto scalar_pixel = [&](int px) {
                float acc = 0.0f;
                for (int kx = -r; kx <= r; ++kx) {
                    const int sx = std::clamp(px + kx, 0, w - 1);
                    acc += f.row_weights[kx + r] * row[sx];
                }
                temp_row[px] = acc;
            };

            int x = 0;
            for (; x < r && x < w; ++x) scalar_pixel(x);
            for (; x + kLanes + r <= w; x += kLanes) {
                __m512 acc = _mm512_setzero_ps();
                for (int kx = -r; kx <= r; ++kx) {
                    acc = _mm512_add_ps(acc, _mm512_mul_ps(row_w[kx + r], LoadPixels16(row + x + kx)));
                }
                _mm512_storeu_ps(temp_row + x, acc);
            }
            for (; x < w; ++x) scalar_pixel(x);
        }

        // Pasada 2: vertical. Solo hay clamp en y, toda la fila se vectoriza.
        for (int y = 0; y < h; ++y) {
            const float* rows[5];
            for (int ky = -r; ky <= r; ++ky) {
                const int sy = std::clamp(y + ky, 0, h - 1);
                rows[ky + r] = temp.data() + static_cast<size_t>(sy) * w;
            }
            uint8_t* out_row = out.data.data() + static_cast<size_t>(y) * w;

            int x = 0;
            for (; x + kLanes <= w; x += kLanes) {
                __m512 acc = _mm512_setzero_ps();
                for (int ky = 0; ky < f.size; ++ky) {
                    acc = _mm512_add_ps(acc, _mm512_mul_ps(col_w[ky], _mm512_loadu_ps(rows[ky] + x)));
                }
                FinishAndStore16(out_row + x, acc);
            }
            for (; x < w; ++x) {
                float acc = 0.0f;
                for (int ky = 0; ky < f.size; ++ky) acc += f.col_weights[ky] * rows[ky][x];
                out_row[x] = FinishScalar(acc);
            }
        }
    }
};

}  // namespace

std::unique_ptr<Backend> make_simd_separable512_backend() {
    return std::make_unique<SimdSeparable512Backend>();
}
