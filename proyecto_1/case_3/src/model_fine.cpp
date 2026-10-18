// Fine-grained multithreading, simulated: H logical threads share one core and
// the scheduler switches round-robin after every quantum of q rows, whether or
// not the thread stalled.
//
// Usage:
//   model_fine [--threads 4] [--quantum 1] [--trace] [--no-pin] [--N 512]
//              [--steps 1000] [--alpha 1.0] [--dt 0.2] [--dx 1.0]
//              [--out field.bin] [--csv field.csv] [--quiet]
//
// --threads is the number of logical threads H; --trace prints the order in
// which logical threads ran during the first step.
#include <algorithm>
#include <cstdio>
#include <vector>

#include "common/affinity.hpp"
#include "common/cli.hpp"
#include "common/coop.hpp"
#include "common/report.hpp"
#include "common/timing.hpp"
#include "stencil_core.hpp"

namespace {

coop::Task sweep_strip(const double* u, double* u_next, int N, double c,
                       stencil::Strip s, int quantum) {
    for (int r = s.r0; r < s.r1; r += quantum) {
        stencil::jacobi_rows(u, u_next, N, c, r, std::min(r + quantum, s.r1));
        if (r + quantum < s.r1) co_await coop::yield();
    }
}

}  // namespace

int main(int argc, char** argv) {
    const common::Config cfg = common::parse_args(argc, argv, common::Threading::fine_grained);
    const double c = common::coeff_or_exit(cfg);
    if (cfg.pin && !common::pin_this_thread(common::cpus_cores_first()[0]) && !cfg.quiet) {
        std::fprintf(stderr, "WARNING: could not pin the scheduler thread\n");
    }

    const std::size_t cells = static_cast<std::size_t>(cfg.N) * cfg.N;
    std::vector<double> u(cells), u_next(cells);
    stencil::init_hot_top_edge(u, cfg.N);
    u_next = u;

    coop::RoundRobin scheduler;
    std::vector<int> first_step_order;
    std::vector<coop::Task> tasks;
    tasks.reserve(cfg.threads);

    common::Timer timer;
    for (int step = 0; step < cfg.steps; ++step) {
        tasks.clear();
        for (int k = 0; k < cfg.threads; ++k) {
            tasks.push_back(sweep_strip(u.data(), u_next.data(), cfg.N, c,
                                        stencil::strip_of(k, cfg.threads, cfg.N),
                                        cfg.quantum));
        }
        scheduler.run(tasks, cfg.trace && step == 0 ? &first_step_order : nullptr);
        u.swap(u_next);
    }
    const double elapsed = timer.seconds();

    common::report("fine", cfg, c, elapsed, u, scheduler.switches());
    if (cfg.trace) coop::print_trace("fine", 0, first_step_order);
    if (!cfg.quiet) {
        std::fprintf(stderr, "[fine] H=%d quantum=%d context_switches=%lld (%.1f per step)\n",
                     cfg.threads, cfg.quantum, scheduler.switches(),
                     static_cast<double>(scheduler.switches()) / cfg.steps);
    }
    return 0;
}
