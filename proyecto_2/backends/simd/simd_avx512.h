// Funciones auxiliares AVX-512 (16 píxeles por registro de 512 bits) para los backends
// "simd512" y "simd_sep512". Solo se incluye desde archivos compilados con -mavx512f.
// Solo se usan instrucciones de AVX512F.
#pragma once
#include <immintrin.h>

#include <cstdint>

namespace dlp_simd {

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

}  // namespace dlp_simd
