#pragma once
#include <cstdio>
#include <vector>

#include "common/cli.hpp"
#include "common/mesh_io.hpp"

namespace common {

inline double checksum(const std::vector<double>& u) {
    double sum = 0.0;
    for (double v : u) sum += v;
    return sum;
}

// stdout carries exactly one CSV line per run so the bench scripts can append it
// directly: model,N,threads,steps,time_s,checksum
inline void report(const char* model, const Config& cfg, double c, double elapsed,
                   const std::vector<double>& u) {
    const double sum = checksum(u);
    if (!cfg.out_bin.empty()) save_mesh_bin(cfg.out_bin, u, cfg.N);
    if (!cfg.out_csv.empty()) save_mesh_csv(cfg.out_csv, u, cfg.N);

    std::printf("%s,%d,%d,%d,%.6f,%.6f\n", model, cfg.N, cfg.threads, cfg.steps,
                elapsed, sum);
    if (!cfg.quiet) {
        std::fprintf(stderr, "[%s] N=%d T=%d steps=%d c=%.4f -> %.6f s (checksum=%.6f)\n",
                     model, cfg.N, cfg.threads, cfg.steps, c, elapsed, sum);
    }
}

}  // namespace common
