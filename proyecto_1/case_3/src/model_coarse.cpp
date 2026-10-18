// Coarse-grained multithreading, simulated: H logical threads share one core
// (same engine as the fine-grained model) but a logical thread yields only on a
// predicted costly miss. It then prefetches the data it will need and lets
// another thread run while the miss is served; otherwise it keeps running.
//
// Usage:
//   model_coarse [--threads 4] [--cache-kib 0] [--trace] [--no-pin] [--N 512]
//                [--steps 1000] [--alpha 1.0] [--dt 0.2] [--dx 1.0]
//                [--out field.bin] [--csv field.csv] [--quiet]
//
// --cache-kib is the modeled cache (0 = this machine's L2); --trace prints the
// order in which logical threads ran during steps 0 and 1.
#include <algorithm>
#include <cstdio>
#include <vector>

#include "common/affinity.hpp"
#include "common/cli.hpp"
#include "common/coop.hpp"
#include "common/cache_model.hpp"
#include "common/report.hpp"
#include "common/timing.hpp"
#include "stencil_core.hpp"

namespace {

coop::Task sweep_strip(const double* u, double* u_next, int N, double c,
                       stencil::Strip s, int window, bool misses, long long& stalls) {
    const int rows_per_run = misses ? window : s.r1 - s.r0;
    for (int r = s.r0; r < s.r1; r += rows_per_run) {
        const int end = std::min(r + rows_per_run, s.r1);
        if (misses) {
            common::prefetch_window_heads(u, u_next, N, r);
            ++stalls;
            co_await coop::yield();
        }
        stencil::jacobi_rows(u, u_next, N, c, r, end);
    }
}

}  // namespace

int main(int argc, char** argv) {
    const common::Config cfg = common::parse_args(argc, argv, common::Threading::coarse_grained);
    const double c = common::coeff_or_exit(cfg);
    if (cfg.pin && !common::pin_this_thread(common::cpus_cores_first()[0]) && !cfg.quiet) {
        std::fprintf(stderr, "WARNING: could not pin the scheduler thread\n");
    }
    const std::size_t detected = common::l2_cache_bytes();
    const std::size_t cache_bytes = cfg.cache_kib > 0 ? std::size_t(cfg.cache_kib) * 1024
                                    : detected > 0    ? detected
                                                      : 256 * 1024;
    const common::CacheModel cache = common::model_cache(cache_bytes, cfg.N, cfg.threads);

    const std::size_t cells = static_cast<std::size_t>(cfg.N) * cfg.N;
    std::vector<double> u(cells), u_next(cells);
    stencil::init_hot_top_edge(u, cfg.N);
    u_next = u;

    coop::RoundRobin scheduler;
    long long stalls = 0;
    std::vector<int> traced_order[2];
    std::vector<coop::Task> tasks;
    tasks.reserve(cfg.threads);

    common::Timer timer;
    for (int step = 0; step < cfg.steps; ++step) {
        const bool misses = step == 0 || !cache.working_set_fits;
        tasks.clear();
        for (int k = 0; k < cfg.threads; ++k) {
            tasks.push_back(sweep_strip(u.data(), u_next.data(), cfg.N, c,
                                        stencil::strip_of(k, cfg.threads, cfg.N),
                                        cache.window_rows, misses, stalls));
        }
        scheduler.run(tasks, cfg.trace && step < 2 ? &traced_order[step] : nullptr);
        u.swap(u_next);
    }
    const double elapsed = timer.seconds();

    common::report("coarse", cfg, c, elapsed, u, scheduler.switches());
    if (cfg.trace) {
        for (int step = 0; step < std::min(cfg.steps, 2); ++step) coop::print_trace("coarse", step, traced_order[step]);
    }
    if (!cfg.quiet) {
        std::fprintf(stderr,
                     "[coarse] H=%d cache=%zu KiB %s window=%d rows stalls=%lld "
                     "context_switches=%lld (%.1f per step)\n",
                     cfg.threads, cache_bytes / 1024,
                     cache.working_set_fits ? "(mesh fits)" : "(mesh streams)",
                     cache.window_rows, stalls, scheduler.switches(),
                     static_cast<double>(scheduler.switches()) / cfg.steps);
    }
    return 0;
}
