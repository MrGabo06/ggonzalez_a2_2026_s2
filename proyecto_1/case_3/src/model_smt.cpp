// SMT model: the CMP strips, but threads are packed siblings-first, so both
// hardware contexts of a core are busy before the next core is used. T threads
// thus run on T/2 cores, and comparing against CMP at the same T isolates what
// sharing a core through SMT adds.
//
// Usage:
//   model_smt [--threads 8] [--no-pin] [--trace] [--N 512] [--steps 1000]
//             [--alpha 1.0] [--dt 0.2] [--dx 1.0] [--out field.bin]
//             [--csv field.csv] [--quiet]
//
// With SMT disabled in the BIOS each core has one context, so threads beyond
// the CPU count are time-sliced by the OS: that run is the SMT-off baseline.
#include <cstdio>
#include <vector>

#include "common/affinity.hpp"
#include "common/cli.hpp"
#include "common/parallel_strips.hpp"
#include "common/report.hpp"
#include "stencil_core.hpp"

int main(int argc, char** argv) {
    const common::Config cfg = common::parse_args(argc, argv, common::Threading::parallel);
    const double c = common::coeff_or_exit(cfg);
    if (common::smt_active() == 0 && !cfg.quiet) {
        std::fprintf(stderr, "[smt] SMT is off: threads sharing a CPU are time-sliced by the OS\n");
    }

    const std::size_t cells = static_cast<std::size_t>(cfg.N) * cfg.N;
    std::vector<double> u(cells), u_next(cells);
    stencil::init_hot_top_edge(u, cfg.N);
    u_next = u;

    const common::ParallelRun run =
        common::run_parallel_strips(cfg, c, common::cpus_siblings_first(), u, u_next);

    common::report("smt", cfg, c, run.elapsed, run.result_in_u ? u : u_next);
    if (cfg.trace) common::print_placement("smt", run.cpu_of_thread);
    return 0;
}
