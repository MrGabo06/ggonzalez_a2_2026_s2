// Sequential baseline for the 2D heat-diffusion stencil (no threads).
// It is the denominator of every speedup and the correctness reference.
//
// Usage:
//   baseline_seq [--N 512] [--steps 1000] [--alpha 1.0] [--dt 0.2] [--dx 1.0]
//                [--out field.bin] [--csv field.csv] [--quiet]
//
// stdout: one parseable CSV line -> model,N,threads,steps,time_s,checksum
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "common/mesh_io.hpp"
#include "common/timing.hpp"
#include "stencil_core.hpp"

namespace {

struct Config {
    int N = 512;
    int steps = 1000;
    double alpha = 1.0;
    double dt = 0.2;
    double dx = 1.0;
    std::string out_bin;
    std::string out_csv;
    bool quiet = false;
};

Config parse_args(int argc, char** argv) {
    Config c;
    for (int i = 1; i < argc; ++i) {
        auto value_of = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "missing value for %s\n", name);
                std::exit(2);
            }
            return argv[++i];
        };
        if (!std::strcmp(argv[i], "--N")) c.N = std::atoi(value_of("--N"));
        else if (!std::strcmp(argv[i], "--steps")) c.steps = std::atoi(value_of("--steps"));
        else if (!std::strcmp(argv[i], "--alpha")) c.alpha = std::atof(value_of("--alpha"));
        else if (!std::strcmp(argv[i], "--dt")) c.dt = std::atof(value_of("--dt"));
        else if (!std::strcmp(argv[i], "--dx")) c.dx = std::atof(value_of("--dx"));
        else if (!std::strcmp(argv[i], "--out")) c.out_bin = value_of("--out");
        else if (!std::strcmp(argv[i], "--csv")) c.out_csv = value_of("--csv");
        else if (!std::strcmp(argv[i], "--quiet")) c.quiet = true;
        else {
            std::fprintf(stderr, "unknown argument: %s\n", argv[i]);
            std::exit(2);
        }
    }
    return c;
}

void init_hot_top_edge(std::vector<double>& u, int N) {
    std::fill(u.begin(), u.end(), 0.0);
    for (int j = 0; j < N; ++j) u[j] = 100.0;
}

}  // namespace

int main(int argc, char** argv) {
    const Config cfg = parse_args(argc, argv);
    if (cfg.N < 3) {
        std::fprintf(stderr, "N must be >= 3\n");
        return 2;
    }
    const stencil::Params p{cfg.N, cfg.alpha, cfg.dt, cfg.dx};
    const double c = p.coeff();
    if (c > 0.25 && !cfg.quiet) {
        std::fprintf(stderr, "WARNING: c=%.4f > 0.25, the scheme may be unstable\n", c);
    }

    const std::size_t cells = static_cast<std::size_t>(cfg.N) * cfg.N;
    std::vector<double> u(cells), u_next(cells);
    init_hot_top_edge(u, cfg.N);
    u_next = u;

    common::Timer timer;
    for (int step = 0; step < cfg.steps; ++step) {
        stencil::jacobi_step(u.data(), u_next.data(), cfg.N, c);
        u.swap(u_next);
    }
    const double elapsed = timer.seconds();

    double checksum = 0.0;
    for (double v : u) checksum += v;

    if (!cfg.out_bin.empty()) common::save_mesh_bin(cfg.out_bin, u, cfg.N);
    if (!cfg.out_csv.empty()) common::save_mesh_csv(cfg.out_csv, u, cfg.N);

    std::printf("baseline_seq,%d,1,%d,%.6f,%.6f\n", cfg.N, cfg.steps, elapsed, checksum);
    if (!cfg.quiet) {
        std::fprintf(stderr, "[baseline_seq] N=%d steps=%d c=%.4f -> %.6f s (checksum=%.6f)\n",
                     cfg.N, cfg.steps, c, elapsed, checksum);
    }
    return 0;
}
