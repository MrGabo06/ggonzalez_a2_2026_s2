// Basic statistics for benchmark timings.
#pragma once
#include <vector>

// Average of the values. Throws std::invalid_argument if the list is empty
double Mean(const std::vector<double>& values);

// Middle value (average of the two middle ones if the count is even).
// Takes a copy on purpose: it sorts the list and the caller's list stays untouched.
// Throws std::invalid_argument if the list is empty
double Median(std::vector<double> values);

// Sample standard deviation (divides by n - 1), 0 if there is only one value.
// Throws std::invalid_argument if the list is empty
double StdDev(const std::vector<double>& values);