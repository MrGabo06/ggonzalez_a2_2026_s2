#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "stencil_core.hpp"

namespace common {

struct Config {
    int N = 512;
    int steps = 1000;
    double alpha = 1.0;
    double dt = 0.2;
    double dx = 1.0;
    int threads = 1;
    bool pin = true;
    std::string out_bin;
    std::string out_csv;
    bool quiet = false;
};

enum class Threading { sequential, parallel };

inline Config parse_args(int argc, char** argv, Threading threading) {
    Config c;
    const bool parallel = threading == Threading::parallel;
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
        else if (parallel && !std::strcmp(argv[i], "--threads")) c.threads = std::atoi(value_of("--threads"));
        else if (parallel && !std::strcmp(argv[i], "--no-pin")) c.pin = false;
        else {
            std::fprintf(stderr, "unknown argument: %s\n", argv[i]);
            std::exit(2);
        }
    }
    return c;
}

inline double coeff_or_exit(const Config& cfg) {
    if (cfg.N < 3) {
        std::fprintf(stderr, "N must be >= 3\n");
        std::exit(2);
    }
    if (cfg.threads < 1 || cfg.threads > cfg.N) {
        std::fprintf(stderr, "threads must be in [1, N]\n");
        std::exit(2);
    }
    const double c = stencil::Params{cfg.N, cfg.alpha, cfg.dt, cfg.dx}.coeff();
    if (c > 0.25 && !cfg.quiet) {
        std::fprintf(stderr, "WARNING: c=%.4f > 0.25, the scheme may be unstable\n", c);
    }
    return c;
}

}  // namespace common
