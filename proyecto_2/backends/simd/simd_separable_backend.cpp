// Backend "simd_sep": convolución separable con AVX2, 8 píxeles por iteración.
#include <immintrin.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "backends/backend.h"
#include "backends/simd/simd_avx2.h"
#include "backends/simd/simd_common.h"

namespace {

using namespace dlp_simd;

constexpr int kLanes = 8;  // píxeles por registro de 256 bits (8 floats)

// Propuesta 2: convolución separable
// Pasada 1 (horizontal): imagen uint8 -> temp en float, con el kernel de filas.
// Pasada 2 (vertical): temp -> salida, con el kernel de columnas.
// Hace 2*size multiplicaciones por píxel en vez de size*size (10 en vez de 25
// para el Gaussiano 5x5), a cambio de recorrer la imagen dos veces y de guardar
// un buffer intermedio de w*h floats.
class SimdSeparableBackend : public Backend {
public:
    std::string name() const override { return "simd_sep"; }

    bool available() const override { return __builtin_cpu_supports("avx2"); }

    void run(const Image& in, const Filter& f, Image& out) override {
        const int r = f.size / 2;
        const int w = in.width;
        const int h = in.height;
        out = Image(w, h);

        // El buffer intermedio se guarda en float para no redondear entre pasadas
        // (mismo criterio que el baseline separable).
        std::vector<float> temp(static_cast<size_t>(w) * h);

        __m256 row_w[5];
        __m256 col_w[5];
        for (int i = 0; i < f.size; ++i) {
            row_w[i] = _mm256_set1_ps(f.row_weights[i]);
            col_w[i] = _mm256_set1_ps(f.col_weights[i]);
        }

        // Pasada 1: horizontal. Cada fila es independiente.
        for (int y = 0; y < h; ++y) {
            const uint8_t* row = in.data.data() + static_cast<size_t>(y) * w;
            float* temp_row = temp.data() + static_cast<size_t>(y) * w;

            int x = 0;
            // Borde izquierdo y cola: escalar, con clamp en x.
            auto scalar_pixel = [&](int px) {
                float acc = 0.0f;
                for (int kx = -r; kx <= r; ++kx) {
                    const int sx = std::clamp(px + kx, 0, w - 1);
                    acc += f.row_weights[kx + r] * row[sx];
                }
                temp_row[px] = acc;
            };

            for (; x < r && x < w; ++x) scalar_pixel(x);
            for (; x + kLanes + r <= w; x += kLanes) {
                __m256 acc = _mm256_setzero_ps();
                for (int kx = -r; kx <= r; ++kx) {
                    acc = _mm256_add_ps(acc, _mm256_mul_ps(row_w[kx + r], LoadPixels8(row + x + kx)));
                }
                _mm256_storeu_ps(temp_row + x, acc);
            }
            for (; x < w; ++x) scalar_pixel(x);
        }

        // Pasada 2: vertical. Solo hay clamp en y, así que toda la fila se vectoriza
        // (no hay borde horizontal) y solo queda cola cuando w no es múltiplo de 8.
        for (int y = 0; y < h; ++y) {
            const float* rows[5];
            for (int ky = -r; ky <= r; ++ky) {
                const int sy = std::clamp(y + ky, 0, h - 1);
                rows[ky + r] = temp.data() + static_cast<size_t>(sy) * w;
            }
            uint8_t* out_row = out.data.data() + static_cast<size_t>(y) * w;

            int x = 0;
            for (; x + kLanes <= w; x += kLanes) {
                __m256 acc = _mm256_setzero_ps();
                for (int ky = 0; ky < f.size; ++ky) {
                    acc = _mm256_add_ps(acc, _mm256_mul_ps(col_w[ky], _mm256_loadu_ps(rows[ky] + x)));
                }
                FinishAndStore8(out_row + x, acc);
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

std::unique_ptr<Backend> make_simd_separable_backend() {
    return std::make_unique<SimdSeparableBackend>();
}
