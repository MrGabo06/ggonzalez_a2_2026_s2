// Sequential baseline for the 2D heat-diffusion stencil (no threads).
// It is the denominator of every speedup and the correctness reference.
//
// Usage:
//   baseline_seq [--N 512] [--steps 1000] [--alpha 1.0] [--dt 0.2] [--dx 1.0]
//                [--out field.bin] [--csv field.csv] [--quiet]
#include <vector>

#include "common/cli.hpp"
#include "common/report.hpp"
#include "common/timing.hpp"
#include "stencil_core.hpp"

int main(int argc, char** argv) {
    const common::Config cfg = common::parse_args(argc, argv, common::Threading::sequential);
    const double c = common::coeff_or_exit(cfg);

    const std::size_t cells = static_cast<std::size_t>(cfg.N) * cfg.N;
    std::vector<double> u(cells), u_next(cells);
    stencil::init_hot_top_edge(u, cfg.N);
    u_next = u;

    common::Timer timer;
    for (int step = 0; step < cfg.steps; ++step) {
        stencil::jacobi_step(u.data(), u_next.data(), cfg.N, c);
        u.swap(u_next);
    }
    const double elapsed = timer.seconds();

    common::report("baseline_seq", cfg, c, elapsed, u);
    return 0;
}
