// smt.cpp - SMT: hilos fijados a IDs de CPU logica especificos (hermanos de un
// mismo nucleo fisico). Solo Linux. Verificar los IDs hermanos con "lscpu -e".
// Uso: smt <width> <height> <spheres> <depth> <core_ids_csv> [salida.bmp]
// Ejemplo: core_ids_csv="0,4" -> 2 hilos, uno en cpu logica 0 y otro en la 4.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <thread>
#include <vector>

#include "bench.h"
#include "image.h"
#include "render.h"
#include "scene.h"

// Fija un hilo a una cpu logica.
static void pin_to_core(std::thread& th, int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(th.native_handle(), sizeof(cpu_set_t), &cpuset);
}

// Parsea "0,4,1" -> {0, 4, 1}.
static std::vector<int> parse_core_ids(const char* csv) {
    std::vector<int> ids;
    char* buf = strdup(csv);
    for (char* tok = strtok(buf, ","); tok; tok = strtok(nullptr, ",")) {
        ids.push_back(std::atoi(tok));
    }
    free(buf);
    return ids;
}

int main(int argc, char** argv) {
    if (argc < 6) {
        std::fprintf(stderr, "Uso: %s <width> <height> <spheres> <depth> <core_ids_csv> [salida.bmp]\n", argv[0]);
        return 1;
    }
    int width   = std::atoi(argv[1]);
    int height  = std::atoi(argv[2]);
    int spheres = std::atoi(argv[3]);
    int depth   = std::atoi(argv[4]);
    std::vector<int> core_ids = parse_core_ids(argv[5]);
    int threads = (int)core_ids.size();
    const char* out_path = (argc > 6) ? argv[6] : nullptr;

    if (width < 1 || height < 1 || spheres < 0 || depth < 0 || threads < 1 || threads > height) {
        std::fprintf(stderr, "Parametros invalidos: width/height >= 1, spheres/depth >= 0, 1 <= cantidad de core_ids <= height\n");
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
        pin_to_core(pool.back(), core_ids[t]);
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
    print_csv_row("smt", width, height, spheres, depth, threads, t1 - t0);
    return 0;
}
