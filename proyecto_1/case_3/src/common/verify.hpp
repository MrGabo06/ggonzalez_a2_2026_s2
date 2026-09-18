#pragma once
#include <cmath>
#include <cstddef>
#include <vector>

namespace common {

struct Diff {
    double max_abs = 0.0;
    double max_rel = 0.0;
};

inline Diff compare_mesh(const std::vector<double>& a,
                         const std::vector<double>& b, double eps = 1e-12) {
    Diff d;
    const std::size_t n = a.size() < b.size() ? a.size() : b.size();
    for (std::size_t k = 0; k < n; ++k) {
        const double abs_diff = std::fabs(a[k] - b[k]);
        if (abs_diff > d.max_abs) d.max_abs = abs_diff;
        const double rel_diff = abs_diff / (std::fabs(a[k]) + eps);
        if (rel_diff > d.max_rel) d.max_rel = rel_diff;
    }
    return d;
}

inline bool meshes_match(const std::vector<double>& a,
                         const std::vector<double>& b, double tol = 1e-9) {
    if (a.size() != b.size()) return false;
    return compare_mesh(a, b).max_abs <= tol;
}

}  // namespace common
