// Funciones escalares compartidas por todos los backends SIMD (AVX2 y AVX-512).
// No usa instrucciones vectoriales, así que se puede incluir desde cualquier archivo.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "common/filter.h"
#include "common/image.h"

namespace dlp_simd {

// Cierre del cálculo para un píxel: valor absoluto, +0.5, saturar a 255 y truncar.
inline uint8_t FinishScalar(float acc) {
    const float v = std::fabs(acc);
    return static_cast<uint8_t>(std::min(v + 0.5f, 255.0f));
}

// Mismo cálculo del baseline para un solo píxel (con clamp en ambos ejes).
inline uint8_t ScalarPixel(const Image& in, const Filter& f, int x, int y) {
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

}  // namespace dlp_simd
