// image.h - Imagen RGB de 8 bits por canal y escritura a BMP.
#ifndef IMAGE_H
#define IMAGE_H

#include <string>
#include <vector>
#include "vec3.h"

struct Image {
    int width, height;
    // RGB intercalado, fila por fila: 3 bytes por pixel. El tamano no cambia
    // despues de crearla, asi que hilos distintos pueden escribir pixeles distintos.
    std::vector<unsigned char> pixels;

    Image(int w, int h);

    // Guarda un color en [0,1] como 3 bytes en el pixel (x, y).
    void set_pixel(int x, int y, const Vec3& color);
};

// Escribe la imagen como BMP de 24 bits. Devuelve false si no pudo escribir el archivo.
bool write_bmp(const Image& img, const std::string& path);

#endif  // IMAGE_H
