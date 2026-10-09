// Un solo hilo real simula `threads` hilos virtuales: les reparte los
// mismos bloques de filas que coarseGrained/cmp/smt (ver render_blocks en
// render.h), pero se turna entre ellos en ronda fija, pintando `quantum`
// pixeles (minimo = 1) por turno
// Uso: fineGrained <width> <height> <spheres> <depth> <threads> [salida.bmp]
#include <algorithm>  // std::min
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "bench.h"
#include "image.h"
#include "raytracer.h"
#include "render.h"
#include "scene.h"

// Cuantum minimo = 1 pixel, ajustable solo por variable de entorno (sin
// tocar la firma del binario) para poder experimentar con su tamano.
static int quantum_from_env() {
    const char* v = std::getenv("FG_QUANTUM");
    if (!v) return 1;
    int q = std::atoi(v);
    return (q >= 1) ? q : 1;
}

// Estado de un hilo virtual: el bloque de filas en que va (el hilo t pinta los
// bloques t, t+n, t+2n, ..., igual que render_blocks) y el siguiente pixel.
struct VThread {
    int block;     // bloque de filas actual
    int row, col;  // siguiente pixel a pintar
    // Termino cuando su bloque ya no existe (paso del ultimo bloque de la imagen).
    bool done(int nblocks) const { return block >= nblocks; }
};

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

    const int quantum = quantum_from_env();

    Scene scene = build_scene(spheres, 42);
    Image img(width, height);

    // Reparto en bloques ciclicos de ROW_BLOCK filas, igual que coarseGrained/cmp/smt:
    // el hilo virtual t arranca en el bloque t.
    const int nblocks = (height + ROW_BLOCK - 1) / ROW_BLOCK;
    std::vector<VThread> vthreads(threads);
    for (int t = 0; t < threads; ++t) {
        vthreads[t] = VThread{t, t * ROW_BLOCK, 0};
    }

    double t0 = now_seconds();

    // Round-robin ciego: cada hilo virtual pinta `quantum` pixeles y cede
    // el turno, sin importar su estado. Se repite hasta que todos terminan.
    int remaining = threads;
    std::vector<char> finished(threads, 0);
    while (remaining > 0) {
        for (int t = 0; t < threads; ++t) {
            if (finished[t]) continue;
            VThread& vt = vthreads[t];
            for (int k = 0; k < quantum && !vt.done(nblocks); ++k) {
                img.set_pixel(vt.col, vt.row,
                              render_pixel(scene, vt.col, vt.row, width, height, depth));
                if (++vt.col >= width) {
                    vt.col = 0;
                    ++vt.row;
                    // Fin del bloque (o de la imagen): salta al siguiente bloque de este hilo.
                    if (vt.row >= std::min((vt.block + 1) * ROW_BLOCK, height)) {
                        vt.block += threads;
                        vt.row = vt.block * ROW_BLOCK;
                    }
                }
            }
            if (vt.done(nblocks)) { finished[t] = 1; --remaining; }
        }
    }

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
