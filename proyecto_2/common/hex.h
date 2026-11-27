// Hex export and import of images: one pixel per line, two hex digits (00 to FF).
// This is the format that Verilog's $readmemh reads, so the FPGA testbench can load
// the same images and baseline outputs that the C++ side uses.
#pragma once
#include <string>

#include "common/image.h"

// Writes the pixels row by row, one line each, like "0F" or "FF".
// Throws std::runtime_error if the file cannot be opened
void WriteHex(const std::string& path, const Image& image);

// Reads a file written by WriteHex. The file does not store the size, so it is given here.
// Throws std::runtime_error if the file cannot be opened, a line is not two hex digits,
// or the number of lines is not width * height
Image ReadHex(const std::string& path, int width, int height);