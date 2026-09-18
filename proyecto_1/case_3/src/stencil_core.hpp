#pragma once
#include <cstddef>

namespace stencil {

// One kernel serves all four execution models; only the caller and the row range
// differ, so measurements compare scheduling rather than algorithms.
struct Params {
    int N;
    double alpha;
    double dt;
    double dx;

    // The explicit 2D scheme is stable only while this stays <= 0.25.
    double coeff() const { return alpha * dt / (dx * dx); }
};

// Boundary cells are held fixed (Dirichlet); the row range enables domain
// decomposition into horizontal strips.
inline void jacobi_rows(const double* u, double* u_next, int N, double c, int r0,
                        int r1) {
    for (int i = r0; i < r1; ++i) {
        const std::size_t row = static_cast<std::size_t>(i) * N;
        for (int j = 0; j < N; ++j) {
            const std::size_t idx = row + j;
            if (i == 0 || i == N - 1 || j == 0 || j == N - 1) {
                u_next[idx] = u[idx];
            } else {
                u_next[idx] = u[idx] +
                              c * (u[idx - N] + u[idx + N] + u[idx - 1] +
                                   u[idx + 1] - 4.0 * u[idx]);
            }
        }
    }
}

inline void jacobi_step(const double* u, double* u_next, int N, double c) {
    jacobi_rows(u, u_next, N, c, 0, N);
}

}  // namespace stencil
