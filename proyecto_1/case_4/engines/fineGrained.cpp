// fineGrained.cpp - grano fino: contador atomico compartido, reparto fila por fila.
// Uso: fineGrained <width> <height> <spheres> <depth> <threads> [salida.bmp]
#include <atomic>
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

    std::atomic<int> next_row{0};  // siguiente fila libre

    double t0 = now_seconds();

    std::vector<std::thread> pool;
    pool.reserve(threads);
    for (int t = 0; t < threads; ++t) {
        pool.emplace_back([&scene, &img, &next_row, width, height, depth]() {
            int y;
            while ((y = next_row.fetch_add(1)) < height) {
                render_region(scene, img, 0, y, width, y + 1, depth);
            }
        });
    }

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
    print_csv_row("fineGrained", width, height, spheres, depth, threads, t1 - t0);
    return 0;
}
