// Imagen en escala de grises de 8 bits y utilidades de E/S en formato PGM (P5).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct Image {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> data;  // fila por fila, width * height bytes

    Image() = default;
    Image(int w, int h) : width(w), height(h), data(static_cast<size_t>(w) * h, 0) {}

    uint8_t& at(int x, int y) { return data[static_cast<size_t>(y) * width + x]; }
    uint8_t at(int x, int y) const { return data[static_cast<size_t>(y) * width + x]; }
};

// Lee un PGM binario (P5, maxval <= 255). Lanza std::runtime_error si falla.
Image read_pgm(const std::string& path);

// Escribe la imagen como PGM binario (P5).
void write_pgm(const std::string& path, const Image& img);

// Genera una imagen sintética reproducible: gradiente + figuras + ruido con semilla.
Image generate_synthetic(int width, int height, uint32_t seed);