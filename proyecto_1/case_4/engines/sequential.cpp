// sequential.cpp - Motor secuencial (un solo hilo): pinta la imagen completa y la guarda como BMP.
// Uso: sequential <width> <height> <spheres> <depth> <salida.bmp>
#include <cstdio>
#include <cstdlib>

#include "image.h"
#include "raytracer.h"
#include "scene.h"

int main(int argc, char** argv) {
    if (argc < 6) {
        std::fprintf(stderr, "Uso: %s <width> <height> <spheres> <depth> <salida.bmp>\n", argv[0]);
        return 1;
    }
    int width   = std::atoi(argv[1]);
    int height  = std::atoi(argv[2]);
    int spheres = std::atoi(argv[3]);  // cantidad de esferas (sin contar el piso)
    int depth   = std::atoi(argv[4]);  // rebotes maximos de reflexion
    const char* out_path = argv[5];

    if (width < 1 || height < 1 || spheres < 0 || depth < 0) {
        std::fprintf(stderr, "Parametros invalidos: width/height >= 1, spheres/depth >= 0\n");
        return 1;
    }

    Scene scene = build_scene(spheres, 42);  // semilla fija: siempre la misma escena
    Image img(width, height);

    // Un pixel a la vez, fila por fila. Cada pixel es un rayo que sale de la camara.
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            img.set_pixel(x, y, render_pixel(scene, x, y, width, height, depth));
        }
    }

    if (!write_bmp(img, out_path)) {
        std::fprintf(stderr, "No se pudo escribir %s\n", out_path);
        return 1;
    }
    std::printf("Imagen guardada en %s (%dx%d, %d esferas, profundidad %d)\n",
                out_path, width, height, spheres, depth);
    return 0;
}
