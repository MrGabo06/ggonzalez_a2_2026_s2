#include "common/image.h"

#include <cctype>
#include <fstream>
#include <random>
#include <stdexcept>

namespace {
// Salta espacios y comentarios (#...) del encabezado PGM.
void skip_ws_and_comments(std::istream& in) {
    while (true) {
        int c = in.peek();
        if (c == '#') {
            std::string line;
            std::getline(in, line);
        } else if (std::isspace(c)) {
            in.get();
        } else {
            break;
        }
    }
}
}  // namespace

Image read_pgm(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("No se pudo abrir " + path);

    std::string magic;
    in >> magic;
    if (magic != "P5") throw std::runtime_error(path + " no es un PGM binario (P5)");

    int w = 0, h = 0, maxval = 0;
    skip_ws_and_comments(in); in >> w;
    skip_ws_and_comments(in); in >> h;
    skip_ws_and_comments(in); in >> maxval;
    in.get();  // un solo espacio separa el encabezado de los datos
    if (w <= 0 || h <= 0 || maxval <= 0 || maxval > 255)
        throw std::runtime_error(path + ": encabezado PGM no soportado");

    Image img(w, h);
    in.read(reinterpret_cast<char*>(img.data.data()), static_cast<std::streamsize>(img.data.size()));
    if (!in) throw std::runtime_error(path + ": datos incompletos");
    return img;
}

void write_pgm(const std::string& path, const Image& img) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("No se pudo escribir " + path);
    out << "P5\n" << img.width << " " << img.height << "\n255\n";
    out.write(reinterpret_cast<const char*>(img.data.data()), static_cast<std::streamsize>(img.data.size()));
}

Image generate_synthetic(int width, int height, uint32_t seed) {
    Image img(width, height);
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> noise(-20, 20);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Gradiente diagonal como fondo
            int v = (x + y) * 255 / (width + height);
            // Rectángulo central y franjas verticales para tener bordes que detectar
            if (x > width / 4 && x < 3 * width / 4 && y > height / 4 && y < 3 * height / 4) v = 255 - v;
            if ((x / 32) % 4 == 0) v /= 2;
            v += noise(rng);
            img.at(x, y) = static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
        }
    }
    return img;
}