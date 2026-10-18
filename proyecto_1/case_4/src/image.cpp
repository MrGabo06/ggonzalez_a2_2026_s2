// image.cpp - Imagen RGB y escritura a BMP.
#include "image.h"

#include <cstddef>  // std::size_t
#include <cstdint>  // std::uint32_t
#include <fstream>

Image::Image(int w, int h)
    : width(w), height(h), pixels(static_cast<std::size_t>(w) * h * 3, 0) {}

// [0,1] -> [0,255]. El +0.5 redondea al entero mas cercano en vez de truncar.
static unsigned char to_byte(double v) {
    return static_cast<unsigned char>(v * 255.0 + 0.5);
}

void Image::set_pixel(int x, int y, const Vec3& color) {
    std::size_t i = (static_cast<std::size_t>(y) * width + x) * 3;
    pixels[i]     = to_byte(color.x);
    pixels[i + 1] = to_byte(color.y);
    pixels[i + 2] = to_byte(color.z);
}

// BMP guarda los numeros en little-endian (el byte menos significativo primero).
static void put_le16(std::ofstream& out, std::uint32_t v) {
    out.put(static_cast<char>(v));
    out.put(static_cast<char>(v >> 8));
}
static void put_le32(std::ofstream& out, std::uint32_t v) {
    put_le16(out, v);
    put_le16(out, v >> 16);
}

bool write_bmp(const Image& img, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    // Cada fila del BMP se rellena con ceros hasta ocupar un multiplo de 4 bytes.
    std::uint32_t row_bytes = static_cast<std::uint32_t>(img.width) * 3;
    std::uint32_t padding   = (4 - row_bytes % 4) % 4;
    std::uint32_t data_size = (row_bytes + padding) * static_cast<std::uint32_t>(img.height);

    // Cabecera de archivo (14 bytes): "BM", tamano total, 4 bytes reservados y
    // el offset donde empiezan los pixeles (14 + 40 = 54).
    out.put('B'); out.put('M');
    put_le32(out, 54 + data_size);
    put_le32(out, 0);
    put_le32(out, 54);

    // Cabecera de imagen (40 bytes): tamano de esta cabecera, ancho, alto,
    // 1 plano, 24 bits por pixel, sin compresion, tamano de los datos,
    // resolucion (2835 px/m ~ 72 dpi, dos veces) y 2 campos de paleta sin uso.
    put_le32(out, 40);
    put_le32(out, static_cast<std::uint32_t>(img.width));
    put_le32(out, static_cast<std::uint32_t>(img.height));
    put_le16(out, 1);
    put_le16(out, 24);
    put_le32(out, 0);
    put_le32(out, data_size);
    put_le32(out, 2835);
    put_le32(out, 2835);
    put_le32(out, 0);
    put_le32(out, 0);

    // Pixeles: el BMP va de la fila de ABAJO hacia arriba y cada pixel en orden B, G, R.
    for (int y = img.height - 1; y >= 0; --y) {
        for (int x = 0; x < img.width; ++x) {
            std::size_t i = (static_cast<std::size_t>(y) * img.width + x) * 3;
            out.put(static_cast<char>(img.pixels[i + 2]));  // B
            out.put(static_cast<char>(img.pixels[i + 1]));  // G
            out.put(static_cast<char>(img.pixels[i]));      // R
        }
        for (std::uint32_t p = 0; p < padding; ++p) out.put(0);
    }
    return static_cast<bool>(out);
}
