// Unit tests for Validate and the sequential baseline. Run with: make test
#include <cmath>
#include <iostream>
#include <stdexcept>

#include "backends/backend.h"
#include "common/filter.h"
#include "common/image.h"
#include "common/validate.h"

// Defined in backends/seq/seq_backend.cpp
std::unique_ptr<Backend> make_seq_backend();

namespace {

int failure_count = 0;

// Prints the failing line and counts it, the program keeps running
#define CHECK(condition)                                                              \
    do {                                                                              \
        if (!(condition)) {                                                           \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #condition "\n"; \
            ++failure_count;                                                          \
        }                                                                             \
    } while (0)

// Image where every pixel has the same value
Image ConstantImage(int width, int height, uint8_t value) {
    Image image(width, height);
    for (auto& pixel : image.data) pixel = value;
    return image;
}

// Runs the sequential baseline once
Image RunSeq(const Image& input, const Filter& filter) {
    auto backend = make_seq_backend();
    Image output;
    backend->run(input, filter, output);
    return output;
}

// Same image twice: no difference expected
void TestIdenticalImages() {
    const Image image = generate_synthetic(16, 16, 1);
    const ValidationResult result = Validate(image, image);
    CHECK(result.max_abs_error == 0);
    CHECK(result.differing_pixels == 0);
    CHECK(std::isinf(result.psnr));
    CHECK(result.ok);
}

// One pixel off by 5: tolerance decides if it passes
void TestOnePixelDiffers() {
    const Image reference = ConstantImage(8, 8, 100);
    Image test = reference;
    test.at(3, 4) = 105;

    const ValidationResult strict = Validate(reference, test, 0);
    CHECK(strict.max_abs_error == 5);
    CHECK(strict.differing_pixels == 1);
    CHECK(!strict.ok);

    CHECK(!Validate(reference, test, 4).ok);  // tolerance below the error
    CHECK(Validate(reference, test, 5).ok);   // tolerance equal to the error
}

// 2x2 image with one pixel off by 255: MSE = 255^2 / 4, PSNR = 10 * log10(4) = 6.0206 dB
void TestPsnrKnownValue() {
    const Image reference = ConstantImage(2, 2, 0);
    Image test = reference;
    test.at(0, 0) = 255;
    const ValidationResult result = Validate(reference, test);
    CHECK(std::fabs(result.psnr - 6.0206) < 1e-3);
    CHECK(result.max_abs_error == 255);
}

// Different sizes cannot be compared
void TestSizeMismatchThrows() {
    bool threw = false;
    try {
        Validate(Image(4, 4), Image(4, 5));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

// Gaussian weights sum to 1, so a constant image must stay the same, borders included
void TestBaselineGaussConstantImage() {
    const Filter gauss = make_gaussian5();
    const int sizes[][2] = {{1, 1}, {7, 13}, {64, 64}};
    for (const auto& size : sizes) {
        const Image input = ConstantImage(size[0], size[1], 100);
        CHECK(Validate(input, RunSeq(input, gauss), 0).ok);
    }
}

// Sobel measures changes, a constant image has none: output must be all zeros
void TestBaselineSobelConstantImage() {
    const Filter sobel = make_sobel_x3();
    const Image input = ConstantImage(7, 13, 200);
    CHECK(Validate(ConstantImage(7, 13, 0), RunSeq(input, sobel), 0).ok);
}

// Two runs of the baseline must give identical images
void TestBaselineIsDeterministic() {
    const Image input = generate_synthetic(33, 17, 7);
    for (const Filter& filter : {make_gaussian5(), make_sobel_x3()}) {
        CHECK(Validate(RunSeq(input, filter), RunSeq(input, filter), 0).ok);
    }
}

}  // namespace

int main() {
    TestIdenticalImages();
    TestOnePixelDiffers();
    TestPsnrKnownValue();
    TestSizeMismatchThrows();
    TestBaselineGaussConstantImage();
    TestBaselineSobelConstantImage();
    TestBaselineIsDeterministic();

    if (failure_count == 0) {
        std::cout << "All tests passed\n";
        return 0;
    }
    std::cerr << failure_count << " test(s) failed\n";
    return 1;
}