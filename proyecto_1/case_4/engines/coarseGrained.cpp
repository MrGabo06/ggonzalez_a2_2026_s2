// coarseGrained.cpp - Multihilo de grano grueso (pthreads).
// Un hilo POSIX por franja de filas; el principal espera a todos con
// pthread_join (barrera de fin de cuadro).
// Uso: coarseGrained <width> <height> <spheres> <depth> <threads> [salida.bmp]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <vector>

#include "bench.h"
#include "image.h"
#include "render.h"
#include "scene.h"

// pthread_create solo acepta UN argumento void*, asi que todo lo que el hilo
// necesita viaja empaquetado en esta estructura (una por hilo).
struct WorkerArgs {
    const Scene* scene;  // solo lectura, compartida por todos los hilos
    Image*       img;    // compartida; cada hilo escribe solo sus bloques de filas
    int          t, n;   // este hilo es el `t` de `n` (define sus bloques de filas)
    int          depth;
};

// Funcion que ejecuta cada hilo. La firma void*(void*) la exige pthreads.
static void* worker(void* p) {
    const WorkerArgs* a = static_cast<const WorkerArgs*>(p);
    render_blocks(*a->scene, *a->img, a->t, a->n, a->depth);  // reparto ciclico de filas
    return nullptr;
}

// Grano grueso: todos los hilos comparten UN solo nucleo (una sola CPU logica)
// y el SO los turna ahi, en vez de repartirlos entre varios nucleos (eso seria CMP).
static const int COARSE_CPU = 0;

// Deja en `attr` la afinidad a COARSE_CPU. Al ir en los atributos, el hilo
// nace ya fijado (no hay un instante inicial en que corra en otra CPU).
// No-op fuera de Linux (ahi los hilos quedan libres).
static void set_single_cpu_attr(pthread_attr_t* attr) {
#if defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(COARSE_CPU, &cpuset);
    pthread_attr_setaffinity_np(attr, sizeof(cpu_set_t), &cpuset);
#else
    (void)attr;
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

    // Los argumentos se preparan ANTES de medir y el vector se dimensiona de
    // una vez: cada hilo guarda un puntero a su elemento, que no puede moverse
    // de lugar mientras el hilo corre. Las filas se reparten en bloques
    // ciclicos dentro de render_blocks (ver render.h).
    std::vector<WorkerArgs> args(threads);
    std::vector<pthread_t>  ids(threads);
    for (int t = 0; t < threads; ++t) {
        args[t] = WorkerArgs{&scene, &img, t, threads, depth};
    }

    // Se mide desde antes de crear los hilos hasta despues de la barrera.
    double t0 = now_seconds();

    // Cada hilo lee la escena (solo lectura) y escribe filas que ningun otro
    // hilo toca, por eso no hace falta ningun candado.
    int created = 0;
    for (int t = 0; t < threads; ++t) {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        set_single_cpu_attr(&attr);  // todos los hilos -> la misma CPU
        int rc = pthread_create(&ids[t], &attr, worker, &args[t]);
        pthread_attr_destroy(&attr);
        if (rc != 0) {  // pthreads devuelve el codigo de error (no usa errno)
            std::fprintf(stderr, "pthread_create fallo (hilo %d): %s\n", t, std::strerror(rc));
            break;
        }
        ++created;
    }

    // Barrera de fin de cuadro: espera a los hilos que si se crearon.
    for (int t = 0; t < created; ++t) pthread_join(ids[t], nullptr);

    double t1 = now_seconds();

    if (created != threads) return 1;  // faltaron hilos: la imagen esta incompleta

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
