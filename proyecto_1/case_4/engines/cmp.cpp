// cmp.cpp - CMP (Chip Multiprocessor / multinucleo real): un hilo por nucleo
// fisico, cada uno con sus propias unidades de ejecucion, sin compartir nada
// entre si salvo la lectura de la escena. A diferencia de coarseGrained (que
// deja al SO decidir donde corre cada hilo), aca se intenta fijar cada hilo
// a un nucleo fisico distinto (afinidad), para que la comparacion contra SMT
// (mismo nucleo, hilos logicos compartiendo recursos) sea real.
// Uso: cmp <width> <height> <spheres> <depth> <threads> [salida.bmp]
// threads no deberia superar la cantidad de nucleos fisicos reales de la
// maquina (en Linux: nproc; en macOS: sysctl -n hw.physicalcpu), sino se
// sobre-suscribe y deja de representar "un hilo por nucleo".
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

// Intenta fijar un hilo a un nucleo fisico especifico. Solo tiene efecto en
// Linux (pthread_setaffinity_np); en otros sistemas (p. ej. macOS) es un
// no-op y el SO reparte los hilos libremente. La afinidad real se valida en
// la maquina Linux del proyecto, la misma que se usa para medir SMT.
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

    // Reparto de filas: cada hilo recibe una franja contigua, igual que en
    // coarseGrained. Lo que cambia es que cada hilo se fija a un nucleo
    // fisico distinto en vez de dejarselo al SO.
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
        pin_to_core(pool.back(), t);  // hilo t -> nucleo fisico t
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
