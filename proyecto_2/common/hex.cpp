#include "common/hex.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <stdexcept>

void WriteHex(const std::string& path, const Image& image) {
    // Step 1: open the file
    std::ofstream out(path);
    if (!out) throw std::runtime_error("cannot write " + path);

    // Step 2: two uppercase hex digits per pixel, padded with a zero (7 -> "07")
    out << std::uppercase << std::hex << std::setfill('0');

    // Step 3: one line per pixel, in the same order as image.data
    for (uint8_t pixel : image.data) out << std::setw(2) << static_cast<int>(pixel) << '\n';
}

Image ReadHex(const std::string& path, int width, int height) {
    // Step 1: open the file and prepare an image of the expected size
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open " + path);
    Image image(width, height);

    // Step 2: read exactly one line per pixel
    for (size_t i = 0; i < image.data.size(); ++i) {
        std::string line;
        if (!std::getline(in, line)) throw std::runtime_error(path + ": too few lines");
        if (!line.empty() && line.back() == '\r') line.pop_back();  // Windows line ending

        // Step 3: each line must be exactly two hex digits
        const bool valid = line.size() == 2 && std::isxdigit(static_cast<unsigned char>(line[0])) &&
                           std::isxdigit(static_cast<unsigned char>(line[1]));
        if (!valid) throw std::runtime_error(path + ": bad line " + std::to_string(i + 1));
        image.data[i] = static_cast<uint8_t>(std::strtoul(line.c_str(), nullptr, 16));
    }

    // Step 4: anything left over means the file is bigger than width * height
    std::string extra;
    if (std::getline(in, extra) && !extra.empty()) throw std::runtime_error(path + ": too many lines");
    return image;
}