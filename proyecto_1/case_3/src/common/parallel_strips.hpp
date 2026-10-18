#pragma once
#include <barrier>
#include <cstdio>
#include <latch>
#include <thread>
#include <utility>
#include <vector>

#include "common/affinity.hpp"
#include "common/cli.hpp"
#include "common/timing.hpp"
#include "stencil_core.hpp"

// Real OS threads shared by CMP and SMT; the two models differ only in the order
// of CPUs that thread k is pinned to.
namespace common {

struct ParallelRun {
    double elapsed;
    bool result_in_u;
    std::vector<int> cpu_of_thread;
};

inline ParallelRun run_parallel_strips(const Config& cfg, double c, const std::vector<int>& cpus,
                                       std::vector<double>& u, std::vector<double>& u_next) {
    double* cur = u.data();
    double* nxt = u_next.data();
    std::barrier step_done(cfg.threads, [&]() noexcept { std::swap(cur, nxt); });
    std::latch pinned_and_ready(cfg.threads + 1);
    std::vector<int> cpu_of_thread(cfg.threads, -1);

    auto worker = [&](int k) {
        if (cfg.pin && !pin_this_thread(cpus[k % cpus.size()]) && !cfg.quiet) {
            std::fprintf(stderr, "WARNING: could not pin thread %d\n", k);
        }
        cpu_of_thread[k] = current_cpu();
        const stencil::Strip s = stencil::strip_of(k, cfg.threads, cfg.N);
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
    Timer timer;
    for (auto& t : pool) t.join();
    return {timer.seconds(), cur == u.data(), std::move(cpu_of_thread)};
}

inline void print_placement(const char* model, const std::vector<int>& cpu_of_thread) {
    std::fprintf(stderr, "[%s] placement:", model);
    for (std::size_t k = 0; k < cpu_of_thread.size(); ++k) {
        const int cpu = cpu_of_thread[k];
        std::fprintf(stderr, " T%zu->cpu%d(core%d)", k, cpu, cpu < 0 ? -1 : core_of(cpu));
    }
    std::fputc('\n', stderr);
}

}  // namespace common
