// cmp.cpp - CMP: un hilo por nucleo fisico, con afinidad (solo Linux).
// Uso: cmp <width> <height> <spheres> <depth> <threads> [salida.bmp]
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

#if defined(__linux__)
#include <pthread.h>
#endif

#include "bench.h"
#include "image.h"
#include "render.h"
#include "scene.h"

// Fija un hilo a un nucleo. No-op fuera de Linux.
static void pin_to_core(std::thread& th, int core_id) {
#if defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(th.native_handle(), sizeof(cpu_set_t), &cpuset);
#else
    (void)th;
    (void)core_id;
#endif
}

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

    int base  = height / threads;
    int extra = height % threads;

    double t0 = now_seconds();

    std::vector<std::thread> pool;
    pool.reserve(threads);
    int y = 0;
    for (int t = 0; t < threads; ++t) {
        int y0 = y;
        int y1 = y + base + (t < extra ? 1 : 0);
        y = y1;
        pool.emplace_back([&scene, &img, width, y0, y1, depth]() {
            render_region(scene, img, 0, y0, width, y1, depth);
        });
        pin_to_core(pool.back(), t);  // hilo t -> nucleo t
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
    print_csv_row("cmp", width, height, spheres, depth, threads, t1 - t0);
    return 0;
}
