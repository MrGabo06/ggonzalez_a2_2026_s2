#include "common/filter.h"

#include <stdexcept>

Filter make_gaussian5() {
    // Aproximación binomial: [1 4 6 4 1]^T x [1 4 6 4 1] / 256
    const float k[5] = {1, 4, 6, 4, 1};
    Filter f{"gauss", 5, std::vector<float>(25)};
    for (int i = 0; i < 5; ++i)
        for (int j = 0; j < 5; ++j)
            f.weights[i * 5 + j] = k[i] * k[j] / 256.0f;
    return f;
}

Filter make_sobel_x3() {
    return Filter{"sobel", 3, {-1, 0, 1,
                               -2, 0, 2,
                               -1, 0, 1}};
}

Filter filter_by_name(const std::string& name) {
    if (name == "gauss") return make_gaussian5();
    if (name == "sobel") return make_sobel_x3();
    throw std::invalid_argument("Filtro desconocido: " + name + " (use gauss o sobel)");
}