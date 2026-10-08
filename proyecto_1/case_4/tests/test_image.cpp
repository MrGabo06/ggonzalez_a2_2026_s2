// tests/test_image.cpp - Pruebas de image.h/.cpp.
// Compilar y correr desde proyecto_1/case_4:
//   g++ -std=c++17 -Wall -Wextra -Isrc src/image.cpp tests/test_image.cpp -o /tmp/test_image && /tmp/test_image
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
#include "image.h"

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FALLA: %s\n", #c); fails++; } } while (0)

static std::vector<unsigned char> read_file(const char* path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

int main() {
    // ---------- set_pixel ----------
    Image img(4, 2);
    CHECK(img.pixels.size() == 4 * 2 * 3);
    img.set_pixel(0, 0, Vec3(1.0, 0.5, 0.0));   // arriba-izquierda
    CHECK(img.pixels[0] == 255 && img.pixels[1] == 128 && img.pixels[2] == 0);  // 0.5*255+0.5 = 128
    img.set_pixel(3, 1, Vec3(0.0, 0.0, 1.0));   // abajo-derecha
    CHECK(img.pixels[img.pixels.size() - 1] == 255);

    // ---------- write_bmp: ancho 4 -> fila de 12 bytes, sin relleno ----------
    const char* path = "/tmp/test_image_out.bmp";
    CHECK(write_bmp(img, path));
    std::vector<unsigned char> f = read_file(path);
    CHECK(f.size() == 54 + 12 * 2);
    CHECK(f[0] == 'B' && f[1] == 'M');
    CHECK(f[2] == f.size());                    // campo "tamano del archivo" (cabe en 1 byte aqui)
    CHECK(f[10] == 54);                         // offset de los pixeles
    CHECK(f[14] == 40);                         // tamano de la cabecera de imagen
    CHECK(f[18] == 4 && f[22] == 2);            // ancho 4, alto 2
    CHECK(f[26] == 1 && f[28] == 24);           // 1 plano, 24 bits
    // Va de abajo hacia arriba y en orden B,G,R:
    //   primera fila guardada = fila de abajo; su ultimo pixel (3,1) es azul puro.
    CHECK(f[54 + 9] == 255 && f[54 + 10] == 0 && f[54 + 11] == 0);   // B=255, G=0, R=0
    //   segunda fila guardada = fila de arriba; su primer pixel (0,0) es (R255,G128,B0).
    CHECK(f[54 + 12] == 0 && f[54 + 13] == 128 && f[54 + 14] == 255);  // B, G, R

    // ---------- relleno: ancho 1 -> fila de 3 bytes + 1 de relleno ----------
    Image one(1, 3);
    CHECK(write_bmp(one, path));
    f = read_file(path);
    CHECK(f.size() == 54 + 4 * 3);
    // Ancho 5 -> 15 bytes + 1 de relleno = 16.
    Image five(5, 2);
    CHECK(write_bmp(five, path));
    f = read_file(path);
    CHECK(f.size() == 54 + 16 * 2);

    // Ruta invalida -> false.
    CHECK(!write_bmp(img, "/no_existe_esta_carpeta/x.bmp"));
    std::remove(path);

    if (fails == 0) std::printf("RESULTADO: todas las pruebas OK\n");
    else            std::printf("RESULTADO: %d falla(s)\n", fails);
    return fails;
}
