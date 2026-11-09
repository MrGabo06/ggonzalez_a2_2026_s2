// Backends SIMD con AVX-512 (variante de la propuesta 2 y su versión directa).
//
//  - "simd512":     convolución directa con AVX-512 (16 píxeles por iteración).
//  - "simd_sep512": convolución separable con AVX-512.
//
// Son la misma estructura que los backends AVX2 de simd_backend.cpp, solo cambia el
// ancho del vector (16 floats en vez de 8). Así la comparación AVX2 vs AVX-512
// aísla el efecto del ancho vectorial y no el de otro cambio de algoritmo.
//
// Este archivo se compila aparte, con -mavx512f (ver Makefile), para que ese flag no
// llegue al código AVX2. Solo se usan instrucciones de AVX512F.
//
// Los pesos son exactos en float, por eso la salida es idéntica al baseline
// (tolerancia 0) aunque el compilador fusione mul+add.
#include <immintrin.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "backends/backend.h"

namespace {

constexpr int kLanes = 16;  // píxeles por registro de 512 bits (16 floats)

// Cierre del cálculo para un píxel: valor absoluto, +0.5, saturar a 255 y truncar.
inline uint8_t FinishScalar(float acc) {
    const float v = std::fabs(acc);
    return static_cast<uint8_t>(std::min(v + 0.5f, 255.0f));
}

// Mismo cierre para 16 píxeles. Guarda 16 bytes en dst.
inline void FinishAndStore16(uint8_t* dst, __m512 acc) {
    acc = _mm512_abs_ps(acc);
    acc = _mm512_min_ps(_mm512_add_ps(acc, _mm512_set1_ps(0.5f)), _mm512_set1_ps(255.0f));
    const __m512i ints = _mm512_cvttps_epi32(acc);
    // 16 x int32 -> 16 x uint8 en una sola instrucción (los valores están en [0, 255])
    _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), _mm512_cvtusepi32_epi8(ints));
}

// Carga 16 píxeles uint8 consecutivos y los convierte a 16 floats.
inline __m512 LoadPixels16(const uint8_t* src) {
    const __m128i bytes = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
    return _mm512_cvtepi32_ps(_mm512_cvtepu8_epi32(bytes));
}

// Mismo cálculo del baseline para un solo píxel (con clamp en ambos ejes).
uint8_t ScalarPixel(const Image& in, const Filter& f, int x, int y) {
    const int r = f.size / 2;
    float acc = 0.0f;
    for (int ky = -r; ky <= r; ++ky) {
        const int sy = std::clamp(y + ky, 0, in.height - 1);
        for (int kx = -r; kx <= r; ++kx) {
            const int sx = std::clamp(x + kx, 0, in.width - 1);
            acc += f.weights[(ky + r) * f.size + (kx + r)] * in.at(sx, sy);
        }
    }
    return FinishScalar(acc);
}

// ---------------------------------------------------------------------------
// Convolución directa, 16 píxeles por iteración
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Convolución separable, 16 píxeles por iteración
// ---------------------------------------------------------------------------
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

std::unique_ptr<Backend> make_simd512_backend() { return std::make_unique<Simd512Backend>(); }
std::unique_ptr<Backend> make_simd_separable512_backend() {
    return std::make_unique<SimdSeparable512Backend>();
}
