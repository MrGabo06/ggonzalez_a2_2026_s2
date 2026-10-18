#pragma once
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace common {

// The binary format is byte-exact, so it is the one used to compare models.
inline void save_mesh_bin(const std::string& path, const std::vector<double>& u,
                          int N) {
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open for writing: " + path);
    const std::int32_t n = N;
    f.write(reinterpret_cast<const char*>(&n), sizeof(n));
    f.write(reinterpret_cast<const char*>(u.data()),
            static_cast<std::streamsize>(u.size() * sizeof(double)));
}

inline std::vector<double> load_mesh_bin(const std::string& path, int& N) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open for reading: " + path);
    std::int32_t n = 0;
    f.read(reinterpret_cast<char*>(&n), sizeof(n));
    if (n <= 0) throw std::runtime_error("invalid mesh size in: " + path);
    std::vector<double> u(static_cast<std::size_t>(n) * n);
    f.read(reinterpret_cast<char*>(u.data()),
           static_cast<std::streamsize>(u.size() * sizeof(double)));
    N = n;
    return u;
}

inline void save_mesh_csv(const std::string& path, const std::vector<double>& u,
                          int N) {
    std::ofstream f(path);
    if (!f) throw std::runtime_error("cannot open for writing: " + path);
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            f << u[static_cast<std::size_t>(i) * N + j];
            if (j + 1 < N) f << ',';
        }
        f << '\n';
    }
}

}  // namespace common
