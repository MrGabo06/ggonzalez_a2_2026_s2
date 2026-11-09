// Funciones auxiliares AVX2 (8 píxeles por registro de 256 bits) para los backends
// "simd" y "simd_sep". Solo se incluye desde archivos compilados con -mavx2.
#pragma once
#include <immintrin.h>

#include <cstdint>
#include <cstring>

namespace dlp_simd {

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

}  // namespace dlp_simd
