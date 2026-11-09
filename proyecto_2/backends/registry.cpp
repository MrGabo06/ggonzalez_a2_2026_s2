#include "backends/backend.h"

// Cada backend define su propia función de creación.
std::unique_ptr<Backend> make_seq_backend();
std::unique_ptr<Backend> make_seq_separable_backend();
std::unique_ptr<Backend> make_fpga_backend();
#ifdef DLP_HAS_SIMD
std::unique_ptr<Backend> make_simd_backend();
std::unique_ptr<Backend> make_simd_separable_backend();
#ifdef DLP_HAS_AVX512
std::unique_ptr<Backend> make_simd512_backend();
std::unique_ptr<Backend> make_simd_separable512_backend();
#endif
#endif
#ifdef DLP_HAS_CUDA
std::unique_ptr<Backend> make_cuda_backend();
#endif

std::unique_ptr<Backend> create_backend(const std::string& name) {
    if (name == "seq") return make_seq_backend();
    if (name == "seq_sep") return make_seq_separable_backend();
    if (name == "fpga") return make_fpga_backend();
#ifdef DLP_HAS_SIMD
    if (name == "simd") return make_simd_backend();
    if (name == "simd_sep") return make_simd_separable_backend();
#ifdef DLP_HAS_AVX512
    if (name == "simd512") return make_simd512_backend();
    if (name == "simd_sep512") return make_simd_separable512_backend();
#endif
#endif
#ifdef DLP_HAS_CUDA
    if (name == "cuda") return make_cuda_backend();
#endif
    return nullptr;
}

std::vector<std::string> available_backends() {
    std::vector<std::string> names = {"seq", "seq_sep", "fpga"};
#ifdef DLP_HAS_SIMD
    names.push_back("simd");
    names.push_back("simd_sep");
#ifdef DLP_HAS_AVX512
    names.push_back("simd512");
    names.push_back("simd_sep512");
#endif
#endif
#ifdef DLP_HAS_CUDA
    names.push_back("cuda");
#endif
    return names;
}