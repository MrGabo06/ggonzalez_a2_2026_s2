// sequential.cpp - Motor secuencial (un solo hilo): pinta la imagen completa y mide el tiempo.
// Uso: sequential <width> <height> <spheres> <depth> [salida.bmp]
#include <cstdio>
#include <cstdlib>

#include "bench.h"
#include "image.h"
#include "render.h"
#include "scene.h"

int main(int argc, char** argv) {
    if (argc < 5) {
        std::fprintf(stderr, "Uso: %s <width> <height> <spheres> <depth> [salida.bmp]\n", argv[0]);
        return 1;
    }
    int width   = std::atoi(argv[1]);
    int height  = std::atoi(argv[2]);
    int spheres = std::atoi(argv[3]);  // cantidad de esferas (sin contar el piso)
    int depth   = std::atoi(argv[4]);  // rebotes maximos de reflexion
    const char* out_path = (argc > 5) ? argv[5] : nullptr;  // opcional: sin ruta no guarda imagen

    if (width < 1 || height < 1 || spheres < 0 || depth < 0) {
        std::fprintf(stderr, "Parametros invalidos: width/height >= 1, spheres/depth >= 0\n");
        return 1;
    }

    Scene scene = build_scene(spheres, 42);  // semilla fija: siempre la misma escena
    Image img(width, height);

    // Solo se mide el render: construir la escena y guardar el archivo quedan fuera.
    double t0 = now_seconds();

    // Toda la imagen como un unico rectangulo, en orden. Un solo hilo hace todo el trabajo.
    render_region(scene, img, 0, 0, width, height, depth);

    double t1 = now_seconds();

    if (out_path) {
        if (!write_bmp(img, out_path)) {
            std::fprintf(stderr, "No se pudo escribir %s\n", out_path);
            return 1;
        }
        std::fprintf(stderr, "Imagen guardada en %s\n", out_path);  // stderr: stdout queda solo para el CSV
    }

    print_csv_header();
    print_csv_row("sequential", width, height, spheres, depth, 1, t1 - t0);
    return 0;
}
