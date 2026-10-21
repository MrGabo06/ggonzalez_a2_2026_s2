#include "backends/backend.h"

// Cada backend define su propia función de creación.
std::unique_ptr<Backend> make_seq_backend();
std::unique_ptr<Backend> make_fpga_backend();
#ifdef DLP_HAS_SIMD
std::unique_ptr<Backend> make_simd_backend();
#endif
#ifdef DLP_HAS_CUDA
std::unique_ptr<Backend> make_cuda_backend();
#endif

std::unique_ptr<Backend> create_backend(const std::string& name) {
    if (name == "seq") return make_seq_backend();
    if (name == "fpga") return make_fpga_backend();
#ifdef DLP_HAS_SIMD
    if (name == "simd") return make_simd_backend();
#endif
#ifdef DLP_HAS_CUDA
    if (name == "cuda") return make_cuda_backend();
#endif
    return nullptr;
}

std::vector<std::string> available_backends() {
    std::vector<std::string> names = {"seq", "fpga"};
#ifdef DLP_HAS_SIMD
    names.push_back("simd");
#endif
#ifdef DLP_HAS_CUDA
    names.push_back("cuda");
#endif
    return names;
}