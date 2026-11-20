// Backend "simd512": convolución directa con AVX-512, 16 píxeles por iteración.
// Misma estructura que "simd" (AVX2): solo cambia el ancho del vector, así la comparación
// AVX2 contra AVX-512 aísla el efecto del ancho vectorial. Se compila aparte, con
// -mavx512f (ver Makefile), para que ese flag no llegue al código AVX2.
#include <immintrin.h>

#include <algorithm>
#include <cstdint>

#include "backends/backend.h"
#include "backends/simd/simd_avx512.h"
#include "backends/simd/simd_common.h"

namespace {

using namespace dlp_simd;

constexpr int kLanes = 16;  // píxeles por registro de 512 bits (16 floats)

class Simd512Backend : public Backend {
public:
    std::string name() const override { return "simd512"; }

    // Evita una instrucción ilegal si la CPU no tiene AVX-512.
    bool available() const override { return __builtin_cpu_supports("avx512f"); }

    void run(const Image& in, const Filter& f, Image& out) override {
        const int r = f.size / 2;
        const int w = in.width;
        const int h = in.height;
        out = Image(w, h);

        __m512 weight[25];
        for (int i = 0; i < f.size * f.size; ++i) weight[i] = _mm512_set1_ps(f.weights[i]);

        for (int y = 0; y < h; ++y) {
            const uint8_t* rows[5];
            for (int ky = -r; ky <= r; ++ky) {
                const int sy = std::clamp(y + ky, 0, h - 1);
                rows[ky + r] = in.data.data() + static_cast<size_t>(sy) * w;
            }
            uint8_t* out_row = out.data.data() + static_cast<size_t>(y) * w;

            int x = 0;

            // Borde izquierdo: escalar, con clamp.
            for (; x < r && x < w; ++x) out_row[x] = ScalarPixel(in, f, x, y);

            // Interior: el bloque [x, x+16) solo lee columnas x-r .. x+15+r, todas dentro.
            for (; x + kLanes + r <= w; x += kLanes) {
                __m512 acc = _mm512_setzero_ps();
                for (int ky = 0; ky < f.size; ++ky) {
                    for (int kx = -r; kx <= r; ++kx) {
                        const __m512 px = LoadPixels16(rows[ky] + x + kx);
                        acc = _mm512_add_ps(acc, _mm512_mul_ps(weight[ky * f.size + kx + r], px));
                    }
                }
                FinishAndStore16(out_row + x, acc);
            }

            // Cola: lo que no completa un bloque de 16 (incluye el borde derecho).
            for (; x < w; ++x) out_row[x] = ScalarPixel(in, f, x, y);
        }
    }
};

}  // namespace

std::unique_ptr<Backend> make_simd512_backend() { return std::make_unique<Simd512Backend>(); }
