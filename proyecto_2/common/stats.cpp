#include "common/stats.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

double Mean(const std::vector<double>& values) {
    // Step 1: an empty list has no mean
    if (values.empty()) throw std::invalid_argument("Mean: empty list");

    // Step 2: add everything up and divide by the count
    const double sum = std::accumulate(values.begin(), values.end(), 0.0);
    return sum / static_cast<double>(values.size());
}

double Median(std::vector<double> values) {
    // Step 1: an empty list has no median
    if (values.empty()) throw std::invalid_argument("Median: empty list");

    // Step 2: sort the copy, smallest to largest
    std::sort(values.begin(), values.end());

    // Step 3: odd count -> the middle value, even count -> average of the two middle ones
    const size_t middle = values.size() / 2;
    if (values.size() % 2 == 1) return values[middle];
    return (values[middle - 1] + values[middle]) / 2.0;
}

double StdDev(const std::vector<double>& values) {
    // Step 1: an empty list has no deviation
    if (values.empty()) throw std::invalid_argument("StdDev: empty list");

    // Step 2: a single value has no spread
    if (values.size() == 1) return 0.0;

    // Step 3: add up the squared distances to the mean
    const double mean = Mean(values);
    double sum_squares = 0.0;
    for (double value : values) sum_squares += (value - mean) * (value - mean);

    // Step 4: divide by n - 1 and take the square root
    return std::sqrt(sum_squares / static_cast<double>(values.size() - 1));
}