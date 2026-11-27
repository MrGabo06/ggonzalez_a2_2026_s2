// Backend "simd": convolución directa con AVX2, 8 píxeles por iteración.
#include <immintrin.h>

#include <algorithm>
#include <cstdint>

#include "backends/backend.h"
#include "backends/simd/simd_avx2.h"
#include "backends/simd/simd_common.h"

namespace {

using namespace dlp_simd;

constexpr int kLanes = 8;  // píxeles por registro de 256 bits (8 floats)

class SimdBackend : public Backend {
public:
    std::string name() const override { return "simd"; }

    // Evita una instrucción ilegal si la CPU no tiene AVX2.
    bool available() const override { return __builtin_cpu_supports("avx2"); }

    void run(const Image& in, const Filter& f, Image& out) override {
        const int r = f.size / 2;
        const int w = in.width;
        const int h = in.height;
        out = Image(w, h);

        // Cada peso se replica en los 8 carriles una sola vez, fuera de los ciclos.
        __m256 weight[25];
        for (int i = 0; i < f.size * f.size; ++i) weight[i] = _mm256_set1_ps(f.weights[i]);

        for (int y = 0; y < h; ++y) {
            // Filas vecinas con clamp en y, una por cada fila del filtro.
            const uint8_t* rows[5];
            for (int ky = -r; ky <= r; ++ky) {
                const int sy = std::clamp(y + ky, 0, h - 1);
                rows[ky + r] = in.data.data() + static_cast<size_t>(sy) * w;
            }
            uint8_t* out_row = out.data.data() + static_cast<size_t>(y) * w;

            int x = 0;

            // Borde izquierdo: los vecinos caen fuera de la imagen, código escalar.
            for (; x < r && x < w; ++x) out_row[x] = ScalarPixel(in, f, x, y);

            // Interior: el bloque [x, x+8) solo lee columnas x-r .. x+7+r, todas dentro.
            for (; x + kLanes + r <= w; x += kLanes) {
                __m256 acc = _mm256_setzero_ps();
                for (int ky = 0; ky < f.size; ++ky) {
                    for (int kx = -r; kx <= r; ++kx) {
                        const __m256 px = LoadPixels8(rows[ky] + x + kx);
                        // mul y add por separado (sin FMA explícito)
                        acc = _mm256_add_ps(acc, _mm256_mul_ps(weight[ky * f.size + kx + r], px));
                    }
                }
                FinishAndStore8(out_row + x, acc);
            }

            // Cola: lo que no completa un bloque de 8 (incluye el borde derecho).
            for (; x < w; ++x) out_row[x] = ScalarPixel(in, f, x, y);
        }
    }
};

}  // namespace

std::unique_ptr<Backend> make_simd_backend() { return std::make_unique<SimdBackend>(); }
