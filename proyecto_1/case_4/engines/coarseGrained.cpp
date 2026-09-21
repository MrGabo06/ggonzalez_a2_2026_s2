// coarseGrained.cpp - Multihilo de grano grueso (std::thread).
// Cada hilo pinta su franja completa de filas, de principio a fin y sin
// interrupcion. El unico punto de sincronizacion es la barrera del final
// (join): se espera a que todos terminen el cuadro.
// Uso: coarseGrained <width> <height> <spheres> <depth> <threads> [salida.bmp]
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

#include "bench.h"
#include "image.h"
#include "render.h"
#include "scene.h"

int main(int argc, char** argv) {
    if (argc < 6) {
        std::fprintf(stderr, "Uso: %s <width> <height> <spheres> <depth> <threads> [salida.bmp]\n", argv[0]);
        return 1;
    }
    int width   = std::atoi(argv[1]);
    int height  = std::atoi(argv[2]);
    int spheres = std::atoi(argv[3]);
    int depth   = std::atoi(argv[4]);
    int threads = std::atoi(argv[5]);
    const char* out_path = (argc > 6) ? argv[6] : nullptr;

    if (width < 1 || height < 1 || spheres < 0 || depth < 0 || threads < 1 || threads > height) {
        std::fprintf(stderr, "Parametros invalidos: width/height >= 1, spheres/depth >= 0, 1 <= threads <= height\n");
        return 1;
    }

    Scene scene = build_scene(spheres, 42);
    Image img(width, height);

    // Reparto de filas: cada hilo recibe una franja contigua. Si las filas no
    // se dividen exacto, los primeros `extra` hilos reciben una fila mas.
    int base  = height / threads;
    int extra = height % threads;

    // Se mide desde antes de crear los hilos hasta despues de la barrera.
    double t0 = now_seconds();

    std::vector<std::thread> pool;
    pool.reserve(threads);
    int y = 0;
    for (int t = 0; t < threads; ++t) {
        int y0 = y;
        int y1 = y + base + (t < extra ? 1 : 0);
        y = y1;
        // Cada hilo lee la escena (solo lectura) y escribe filas que ningun
        // otro hilo toca, por eso no hace falta ningun candado.
        pool.emplace_back([&scene, &img, width, y0, y1, depth]() {
            render_region(scene, img, 0, y0, width, y1, depth);
        });
    }

    // Barrera de fin de cuadro: espera a que todos los hilos terminen.
    for (std::thread& th : pool) th.join();

    double t1 = now_seconds();

    if (out_path) {
        if (!write_bmp(img, out_path)) {
            std::fprintf(stderr, "No se pudo escribir %s\n", out_path);
            return 1;
        }
        std::fprintf(stderr, "Imagen guardada en %s\n", out_path);
    }

    print_csv_header();
    print_csv_row("coarseGrained", width, height, spheres, depth, threads, t1 - t0);
    return 0;
}
