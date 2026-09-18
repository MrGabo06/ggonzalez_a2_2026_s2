// Minimal tests for the common utilities and the stencil kernel.
// Built with -fsanitize=address,undefined to catch out-of-range access.
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "common/affinity.hpp"
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
    test_stencil_convergence();
    std::puts("OK: common utilities and stencil kernel");
    return 0;
}
