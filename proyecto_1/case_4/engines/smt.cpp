// smt.cpp - SMT: hilos (pthreads) fijados a IDs de CPU logica especificos
// (hermanos de un mismo nucleo fisico). Solo compila en Linux (afinidad de CPU).
// Uso: smt <width> <height> <spheres> <depth> <core_ids_csv> [salida.bmp]
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

// Deja en `attr` la afinidad de un hilo a UNA cpu logica. Al ir en los
// atributos, el hilo nace YA fijado a esa cpu (no hay un instante inicial en
// que corra en otra).
static void set_core_attr(pthread_attr_t* attr, int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_attr_setaffinity_np(attr, sizeof(cpu_set_t), &cpuset);
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

    // Argumentos listos antes de medir; el vector no cambia de tamano porque
    // cada hilo guarda un puntero a su elemento.
    std::vector<WorkerArgs> args(threads);
    std::vector<pthread_t>  ids(threads);
    int y = 0;
    for (int t = 0; t < threads; ++t) {
        int y0 = y;
        int y1 = y + base + (t < extra ? 1 : 0);
        y = y1;
        args[t] = WorkerArgs{&scene, &img, width, y0, y1, depth};
    }

    double t0 = now_seconds();

    int created = 0;
    for (int t = 0; t < threads; ++t) {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        set_core_attr(&attr, core_ids[t]);  // hilo t -> cpu logica core_ids[t]
        int rc = pthread_create(&ids[t], &attr, worker, &args[t]);
        pthread_attr_destroy(&attr);
        if (rc != 0) {  // pthreads devuelve el codigo de error (no usa errno)
            std::fprintf(stderr, "pthread_create fallo (hilo %d, cpu %d): %s\n", t, core_ids[t], std::strerror(rc));
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
    print_csv_row("smt", width, height, spheres, depth, threads, t1 - t0);
    return 0;
}
