// Backends SIMD con AVX2.
//
//  - "simd":     propuesta 1, convolución directa (una pasada 2D).
//  - "simd_sep": propuesta 2, convolución separable (dos pasadas 1D).
//
// Descomposición: por dominio de datos. Cada píxel de salida es independiente,
// así que se vectorizan 8 píxeles contiguos de una fila (eje x) por iteración.
// Una fila de la imagen es contigua en memoria, por eso los 8 vecinos de cada tap
// se leen con una sola carga.
//
// Reglas de los backends (ver common/filter.h): borde con clamp, valor absoluto,
// +0.5, saturar a 255 y truncar. Para que la salida sea igual al baseline:
//  - Los píxeles del borde izquierdo y la cola de cada fila usan código escalar.
//  - Los bloques interiores (todos sus vecinos dentro de la imagen) usan AVX2.
//  - Los pesos son exactos en float, por eso el orden de las sumas y el FMA no
//    cambian el resultado (se valida con tolerancia 0).
#include <immintrin.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "backends/backend.h"

namespace {

constexpr int kLanes = 8;  // píxeles por registro de 256 bits (8 floats)

// Cierre del cálculo para un píxel: valor absoluto, +0.5, saturar a 255 y truncar.
inline uint8_t FinishScalar(float acc) {
    const float v = std::fabs(acc);
    return static_cast<uint8_t>(std::min(v + 0.5f, 255.0f));
}

// Mismo cierre para 8 píxeles. Guarda 8 bytes en dst.
inline void FinishAndStore8(uint8_t* dst, __m256 acc) {
    const __m256 sign_mask = _mm256_set1_ps(-0.0f);  // solo el bit de signo
    const __m256 half = _mm256_set1_ps(0.5f);
    const __m256 max_value = _mm256_set1_ps(255.0f);

    acc = _mm256_andnot_ps(sign_mask, acc);
    acc = _mm256_min_ps(_mm256_add_ps(acc, half), max_value);
    const __m256i ints = _mm256_cvttps_epi32(acc);

    // 8 x int32 -> 8 x uint8. Los valores están en [0, 255], así que la saturación
    // sin signo de packus no los cambia. packus actúa por mitades de 128 bits: la
    // mitad baja trae los píxeles 0-3 y la alta los 4-7, cada una en los 4 bytes bajos.
    const __m256i p16 = _mm256_packus_epi32(ints, ints);
    const __m256i p8 = _mm256_packus_epi16(p16, p16);
    const int32_t lo = _mm_cvtsi128_si32(_mm256_castsi256_si128(p8));
    const int32_t hi = _mm_cvtsi128_si32(_mm256_extracti128_si256(p8, 1));
    std::memcpy(dst, &lo, 4);
    std::memcpy(dst + 4, &hi, 4);
}

// Carga 8 píxeles uint8 consecutivos y los convierte a 8 floats.
inline __m256 LoadPixels8(const uint8_t* src) {
    const __m128i bytes = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(src));
    return _mm256_cvtepi32_ps(_mm256_cvtepu8_epi32(bytes));
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
// Propuesta 1: convolución directa
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Propuesta 2: convolución separable
// Pasada 1 (horizontal): imagen uint8 -> temp en float, con el kernel de filas.
// Pasada 2 (vertical): temp -> salida, con el kernel de columnas.
// Hace 2*size multiplicaciones por píxel en vez de size*size (10 en vez de 25
// para el Gaussiano 5x5), a cambio de recorrer la imagen dos veces y de guardar
// un buffer intermedio de w*h floats.
// ---------------------------------------------------------------------------
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

std::unique_ptr<Backend> make_simd_backend() { return std::make_unique<SimdBackend>(); }
std::unique_ptr<Backend> make_simd_separable_backend() {
    return std::make_unique<SimdSeparableBackend>();
}
