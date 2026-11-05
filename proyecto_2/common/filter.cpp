#include "common/filter.h"

#include <stdexcept>

Filter make_gaussian5() {
    // Aproximación binomial: [1 4 6 4 1]^T x [1 4 6 4 1] / 256
    const float k[5] = {1, 4, 6, 4, 1};
    Filter f;
    f.name = "gauss";
    f.size = 5;
    f.weights.resize(25);
    for (int i = 0; i < 5; ++i)
        for (int j = 0; j < 5; ++j)
            f.weights[i * 5 + j] = k[i] * k[j] / 256.0f;

    // Each 1D kernel carries half of the normalization: 1/16 * 1/16 = 1/256
    for (int i = 0; i < 5; ++i) {
        f.col_weights.push_back(k[i] / 16.0f);
        f.row_weights.push_back(k[i] / 16.0f);
    }
    return f;
}

Filter make_sobel_x3() {
    Filter f;
    f.name = "sobel";
    f.size = 3;
    f.weights = {-1, 0, 1,
                 -2, 0, 2,
                 -1, 0, 1};

    // Sobel x = vertical smoothing [1 2 1] times horizontal difference [-1 0 1]
    f.col_weights = {1, 2, 1};
    f.row_weights = {-1, 0, 1};
    return f;
}

Filter filter_by_name(const std::string& name) {
    if (name == "gauss") return make_gaussian5();
    if (name == "sobel") return make_sobel_x3();
    throw std::invalid_argument("Filtro desconocido: " + name + " (use gauss o sobel)");
}