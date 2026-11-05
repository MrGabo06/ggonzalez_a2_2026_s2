#include "common/validate.h"

#include <cmath>
#include <cstdlib>
#include <limits>
#include <stdexcept>

ValidationResult Validate(const Image& reference, const Image& test, int tolerance) {
    // Step 1: reject images of different size
    if (reference.width != test.width || reference.height != test.height)
        throw std::invalid_argument("Validate: image sizes differ");

    ValidationResult result;
    double sum_squares = 0.0;  // double avoids overflow on big images

    // Step 2: walk every pixel and accumulate the errors
    for (size_t i = 0; i < reference.data.size(); ++i) {
        // Cast to int first: uint8_t cannot hold negative differences
        const int diff = std::abs(static_cast<int>(reference.data[i]) - static_cast<int>(test.data[i]));
        if (diff != 0) ++result.differing_pixels;
        if (diff > result.max_abs_error) result.max_abs_error = diff;
        sum_squares += static_cast<double>(diff) * diff;
    }

    // Step 3: derive MSE and PSNR
    const size_t total_pixels = reference.data.size();
    result.mse = total_pixels ? sum_squares / static_cast<double>(total_pixels) : 0.0;
    // MSE = 0 would divide by zero, so identical images get infinity
    result.psnr = (result.mse == 0.0) ? std::numeric_limits<double>::infinity()
                                      : 10.0 * std::log10(255.0 * 255.0 / result.mse);

    // Step 4: compare the worst error against the tolerance
    result.ok = (result.max_abs_error <= tolerance);
    return result;
}