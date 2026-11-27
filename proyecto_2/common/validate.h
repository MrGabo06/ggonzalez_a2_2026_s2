// Compares a backend output against the baseline output.
#pragma once
#include <cstddef>

#include "common/image.h"

// Report returned by Validate
struct ValidationResult {
    int max_abs_error = 0;        // Worst per-pixel difference (0..255)
    size_t differing_pixels = 0;  // Pixels that are not equal
    double mse = 0.0;             // Mean squared error
    double psnr = 0.0;            // In dB, infinity if images are identical
    bool ok = false;              // True if max_abs_error <= tolerance
};

// Throws std::invalid_argument if the image sizes differ
// tolerance: allowed per-pixel error (0 = exact match)
ValidationResult Validate(const Image& reference, const Image& test, int tolerance = 0);