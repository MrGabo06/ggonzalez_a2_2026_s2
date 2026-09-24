// Minimal tests for the common utilities and the stencil kernel.
// Built with -fsanitize=address,undefined to catch out-of-range access.
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "common/affinity.hpp"
#include "common/cache_model.hpp"
#include "common/coop.hpp"
#include "common/mesh_io.hpp"
#include "common/timing.hpp"
#include "common/verify.hpp"
#include "stencil_core.hpp"

static void test_timing() {
    common::Timer t;
    volatile double x = 0;
    for (int i = 0; i < 100000; ++i) x += i;
    assert(t.seconds() >= 0.0);
    (void)x;
}

static void test_mesh_io_roundtrip() {
    const int N = 8;
    std::vector<double> u(static_cast<std::size_t>(N) * N);
    for (std::size_t k = 0; k < u.size(); ++k) u[k] = static_cast<double>(k) * 0.5;
    const std::string path = "build/_test_mesh.bin";
    common::save_mesh_bin(path, u, N);
    int loaded_N = 0;
    std::vector<double> reloaded = common::load_mesh_bin(path, loaded_N);
    assert(loaded_N == N);
    assert(common::meshes_match(u, reloaded, 0.0));
}

static void test_verify() {
    std::vector<double> a{1.0, 2.0, 3.0};
    std::vector<double> b{1.0, 2.0, 3.0 + 1e-11};
    assert(common::meshes_match(a, b, 1e-9));
    assert(!common::meshes_match(a, b, 1e-13));
    assert(common::compare_mesh(a, b).max_abs > 0.0);
}

static void test_affinity() {
    (void)common::pin_this_thread(0);
    assert(common::hw_threads() >= 1u);
}

static void test_cpus_cores_first_is_a_permutation() {
    std::vector<int> cpus = common::cpus_cores_first();
    assert(cpus.size() == common::hw_threads());
    std::sort(cpus.begin(), cpus.end());
    for (std::size_t k = 0; k < cpus.size(); ++k) assert(cpus[k] == static_cast<int>(k));
}

static coop::Task yield_times(int yields) {
    for (int i = 0; i < yields; ++i) co_await coop::yield();
}

// Counted: H1->H2 and H2->H1 after yields. Not counted: H0->H1 and H1->H2 after
// a thread finished, and H2 resuming itself.
static void test_round_robin_order_and_switches() {
    std::vector<coop::Task> tasks;
    tasks.push_back(yield_times(0));
    tasks.push_back(yield_times(1));
    tasks.push_back(yield_times(3));
    coop::RoundRobin scheduler;
    std::vector<int> order;
    scheduler.run(tasks, &order);
    assert((order == std::vector<int>{0, 1, 2, 1, 2, 2, 2}));
    assert(scheduler.switches() == 2);
}

// Packed order keeps every sibling pair adjacent: a core's contexts are
// consecutive entries, so a core never reappears after another core started.
static void test_cpus_siblings_first_packs_cores() {
    const std::vector<int> packed = common::cpus_siblings_first();
    std::vector<int> sorted = packed;
    std::sort(sorted.begin(), sorted.end());
    for (std::size_t k = 0; k < sorted.size(); ++k) assert(sorted[k] == static_cast<int>(k));
    std::vector<int> finished_cores;
    for (std::size_t k = 1; k < packed.size(); ++k) {
        const int prev = common::core_of(packed[k - 1]);
        const int core = common::core_of(packed[k]);
        if (core == prev) continue;
        finished_cores.push_back(prev);
        assert(std::find(finished_cores.begin(), finished_cores.end(), core) == finished_cores.end());
    }
}

// 256 KiB holds exactly both 128x128 meshes (2 x 128 KiB), so N=128 is the last
// cache-resident size and N=129 already streams.
static void test_cache_model_regimes() {
    const std::size_t l2 = 256 * 1024;
    assert(common::model_cache(l2, 128, 4).working_set_fits);
    assert(!common::model_cache(l2, 129, 4).working_set_fits);
    assert(common::model_cache(l2, 1024, 4).window_rows == 4);
    assert(common::model_cache(l2, 1024, 16).window_rows == 1);
    assert(common::model_cache(l2, 1024, 64).window_rows == 1);
}

static void test_prefetch_stays_inside_small_meshes() {
    for (int N : {3, 5, 8, 9}) {
        std::vector<double> u(static_cast<std::size_t>(N) * N), u_next(u.size());
        for (int r = 0; r < N; ++r) common::prefetch_window_heads(u.data(), u_next.data(), N, r);
    }
}

// A uniform fixed boundary drives the whole interior to that same value at
// equilibrium, so the center reaching V is the observable convergence check.
static void test_stencil_convergence() {
    const int N = 16;
    const double boundary_value = 50.0;
    const stencil::Params p{N, 1.0, 0.2, 1.0};
    const double c = p.coeff();

    std::vector<double> u(static_cast<std::size_t>(N) * N, 0.0);
    std::vector<double> u_next(u.size(), 0.0);
    for (int i = 0; i < N; ++i)
        for (int j = 0; j < N; ++j)
            if (i == 0 || i == N - 1 || j == 0 || j == N - 1)
                u[static_cast<std::size_t>(i) * N + j] = boundary_value;

    for (int step = 0; step < 5000; ++step) {
        stencil::jacobi_step(u.data(), u_next.data(), N, c);
        u.swap(u_next);
    }

    const double center = u[static_cast<std::size_t>(N / 2) * N + N / 2];
    assert(std::fabs(center - boundary_value) < 1e-3);
}

int main() {
    test_timing();
    test_mesh_io_roundtrip();
    test_verify();
    test_affinity();
    test_cpus_cores_first_is_a_permutation();
    test_round_robin_order_and_switches();
    test_cpus_siblings_first_packs_cores();
    test_cache_model_regimes();
    test_prefetch_stays_inside_small_meshes();
    test_stencil_convergence();
    std::puts("OK: common utilities and stencil kernel");
    return 0;
}
