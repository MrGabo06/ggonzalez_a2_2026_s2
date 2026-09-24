// CMP model: real multicore parallelism. The mesh is split into T horizontal
// strips, one per thread, and a barrier closes every time step.
//
// Usage:
//   model_cmp [--threads 4] [--no-pin] [--N 512] [--steps 1000] [--alpha 1.0]
//             [--dt 0.2] [--dx 1.0] [--out field.bin] [--csv field.csv] [--quiet]
//
// Threads are pinned cores-first, so T above the physical core count
// oversubscribes onto SMT siblings (the SMT dummy runs this binary with T=8).
#include <barrier>
#include <cstdio>
#include <latch>
#include <thread>
#include <utility>
#include <vector>

#include "common/affinity.hpp"
#include "common/cli.hpp"
#include "common/report.hpp"
#include "common/timing.hpp"
#include "stencil_core.hpp"

namespace {

struct Strip {
    int r0;
    int r1;
};

Strip strip_of(int k, int threads, int N) {
    return {k * N / threads, (k + 1) * N / threads};
}

}  // namespace

int main(int argc, char** argv) {
    const common::Config cfg = common::parse_args(argc, argv, common::Threading::parallel);
    const double c = common::coeff_or_exit(cfg);

    const std::size_t cells = static_cast<std::size_t>(cfg.N) * cfg.N;
    std::vector<double> u(cells), u_next(cells);
    stencil::init_hot_top_edge(u, cfg.N);
    u_next = u;

    double* cur = u.data();
    double* nxt = u_next.data();
    std::barrier step_done(cfg.threads, [&]() noexcept { std::swap(cur, nxt); });

    const std::vector<int> cpus = common::cpus_cores_first();
    std::latch pinned_and_ready(cfg.threads + 1);

    auto worker = [&](int k) {
        if (cfg.pin && !common::pin_this_thread(cpus[k % cpus.size()]) && !cfg.quiet) {
            std::fprintf(stderr, "WARNING: could not pin thread %d\n", k);
        }
        const Strip s = strip_of(k, cfg.threads, cfg.N);
        pinned_and_ready.arrive_and_wait();
        for (int step = 0; step < cfg.steps; ++step) {
            stencil::jacobi_rows(cur, nxt, cfg.N, c, s.r0, s.r1);
            step_done.arrive_and_wait();
        }
    };

    std::vector<std::jthread> pool;
    pool.reserve(cfg.threads);
    for (int k = 0; k < cfg.threads; ++k) pool.emplace_back(worker, k);

    pinned_and_ready.arrive_and_wait();
    common::Timer timer;
    for (auto& t : pool) t.join();
    const double elapsed = timer.seconds();

    common::report("cmp", cfg, c, elapsed, cur == u.data() ? u : u_next);
    return 0;
}
