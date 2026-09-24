#pragma once
#include <algorithm>
#include <cstddef>

// Stall model of the coarse-grained model, kept apart so its regimes are tested.
namespace common {

// Row-granular cache model. A time step touches every row of u and u_next, so
// either both meshes stay resident after the first (cold) step, or rows stream
// through and each thread may keep only its share of the cache prefetched.
struct CacheModel {
    bool working_set_fits;
    int window_rows;
};

inline CacheModel model_cache(std::size_t cache_bytes, int N, int threads) {
    const std::size_t row_of_each_mesh = 2 * sizeof(double) * static_cast<std::size_t>(N);
    const std::size_t rows_in_cache = cache_bytes / row_of_each_mesh;
    return {rows_in_cache >= static_cast<std::size_t>(N),
            std::max(1, static_cast<int>(rows_in_cache / threads))};
}

// Only the head of each stream is prefetched: once a core's ~16 outstanding-miss
// slots fill up, every further prefetch blocks the thread (prefetching whole
// windows ran ~40% slower than the baseline). The hardware streamer continues
// once the head is in flight.
inline void prefetch_window_heads(const double* u, double* u_next, int N, int r0) {
    constexpr std::size_t doubles_per_line = 64 / sizeof(double);
    constexpr std::size_t lines_per_stream = 4;
    const std::size_t n = static_cast<std::size_t>(N);
    const std::size_t above = static_cast<std::size_t>(std::max(r0 - 1, 0)) * n;
    const std::size_t below = static_cast<std::size_t>(std::min(r0 + 1, N - 1)) * n;
    const std::size_t own = static_cast<std::size_t>(r0) * n;
    const std::size_t lines = std::min(lines_per_stream, (n + doubles_per_line - 1) / doubles_per_line);
    for (std::size_t line = 0; line < lines; ++line) {
        const std::size_t offset = line * doubles_per_line;
        __builtin_prefetch(u + above + offset, 0, 3);
        __builtin_prefetch(u + own + offset, 0, 3);
        __builtin_prefetch(u + below + offset, 0, 3);
        __builtin_prefetch(u_next + own + offset, 1, 3);
    }
}

}  // namespace common
