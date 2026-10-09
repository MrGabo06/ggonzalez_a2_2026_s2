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
    Image*       img;    // compartida; cada hilo escribe solo sus filas
    int          width;
    int          y0, y1; // franja de filas [y0, y1) de este hilo
    int          depth;
};

// Funcion que ejecuta cada hilo. La firma void*(void*) la exige pthreads.
static void* worker(void* p) {
    const WorkerArgs* a = static_cast<const WorkerArgs*>(p);
    render_region(*a->scene, *a->img, 0, a->y0, a->width, a->y1, a->depth);
    return nullptr;
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

    // Reparto de filas: cada hilo recibe una franja contigua. Si las filas no
    // se dividen exacto, los primeros `extra` hilos reciben una fila mas.
    int base  = height / threads;
    int extra = height % threads;

    // Los argumentos se preparan ANTES de medir y el vector se dimensiona de
    // una vez: cada hilo guarda un puntero a su elemento, que no puede moverse
    // de lugar mientras el hilo corre.
    std::vector<WorkerArgs> args(threads);
    std::vector<pthread_t>  ids(threads);
    int y = 0;
    for (int t = 0; t < threads; ++t) {
        int y0 = y;
        int y1 = y + base + (t < extra ? 1 : 0);
        y = y1;
        args[t] = WorkerArgs{&scene, &img, width, y0, y1, depth};
    }

    // Se mide desde antes de crear los hilos hasta despues de la barrera.
    double t0 = now_seconds();

    // Cada hilo lee la escena (solo lectura) y escribe filas que ningun otro
    // hilo toca, por eso no hace falta ningun candado.
    int created = 0;
    for (int t = 0; t < threads; ++t) {
        int rc = pthread_create(&ids[t], nullptr, worker, &args[t]);
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
