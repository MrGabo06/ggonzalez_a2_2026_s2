// Compares a model's output mesh against the baseline's.
//
// Usage: verify_mesh <reference.bin> <candidate.bin> [tolerance]
//
// The default tolerance is 0: every model computes each cell from the same
// operands in the same order (Jacobi reads only the previous step), so any
// difference is a scheduling bug, not floating-point reordering.
#include <cstdio>
#include <cstdlib>
#include <exception>

#include "common/mesh_io.hpp"
#include "common/verify.hpp"

int main(int argc, char** argv) {
    if (argc < 3 || argc > 4) {
        std::fprintf(stderr, "usage: verify_mesh <reference.bin> <candidate.bin> [tolerance]\n");
        return 2;
    }
    const double tolerance = argc == 4 ? std::atof(argv[3]) : 0.0;
    try {
        int n_ref = 0, n_cand = 0;
        const auto ref = common::load_mesh_bin(argv[1], n_ref);
        const auto cand = common::load_mesh_bin(argv[2], n_cand);
        if (n_ref != n_cand) {
            std::printf("FAIL size %d vs %d\n", n_ref, n_cand);
            return 1;
        }
        const common::Diff d = common::compare_mesh(ref, cand);
        const bool ok = d.max_abs <= tolerance;
        std::printf("%s max_abs=%.3e max_rel=%.3e\n", ok ? "OK" : "FAIL", d.max_abs, d.max_rel);
        return ok ? 0 : 1;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "verify_mesh: %s\n", e.what());
        return 2;
    }
}
