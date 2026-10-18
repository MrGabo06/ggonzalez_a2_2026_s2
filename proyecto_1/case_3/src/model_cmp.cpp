// CMP model: real multicore parallelism. The mesh is split into T horizontal
// strips, one per thread, and a barrier closes every time step.
//
// Usage:
//   model_cmp [--threads 4] [--no-pin] [--trace] [--N 512] [--steps 1000]
//             [--alpha 1.0] [--dt 0.2] [--dx 1.0] [--out field.bin]
//             [--csv field.csv] [--quiet]
//
// Threads are pinned cores-first: one per physical core while T <= cores.
// --trace prints the CPU and core each thread ended up on.
#include <vector>

#include "common/affinity.hpp"
#include "common/cli.hpp"
#include "common/parallel_strips.hpp"
#include "common/report.hpp"
#include "stencil_core.hpp"

int main(int argc, char** argv) {
    const common::Config cfg = common::parse_args(argc, argv, common::Threading::parallel);
    const double c = common::coeff_or_exit(cfg);

    const std::size_t cells = static_cast<std::size_t>(cfg.N) * cfg.N;
    std::vector<double> u(cells), u_next(cells);
    stencil::init_hot_top_edge(u, cfg.N);
    u_next = u;

    const common::ParallelRun run =
        common::run_parallel_strips(cfg, c, common::cpus_cores_first(), u, u_next);

    common::report("cmp", cfg, c, run.elapsed, run.result_in_u ? u : u_next);
    if (cfg.trace) common::print_placement("cmp", run.cpu_of_thread);
    return 0;
}
