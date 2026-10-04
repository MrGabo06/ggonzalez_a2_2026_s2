// fineGrained.cpp - grano fino: planificacion cooperativa por cuantum fijo.
// ---------------------------------------------------------------------------
// Enunciado (CE4302, Proyecto 1, seccion 4, inciso a): el grano fino debe
// implementarse mediante un modelo de planificacion cooperativa (o
// simulacion de ejecucion por ciclos o cuantums), donde el cambio de hilo
// ocurre en CADA ciclo (o cuantum minimo) en forma round-robin,
// INDEPENDIENTEMENTE de si el hilo actual ha sufrido un stall. Es decir: el
// cambio de contexto es ciego al estado del hilo, nunca una reaccion a un
// evento de bloqueo (eso es coarse-grained, ver coarseGrained.cpp, donde el
// cambio de contexto solo ocurre en el join final).
//
// Por eso esta version NO usa std::thread ni paralelismo real sobre varios
// nucleos: en hardware, el grano fino es UN solo pipeline/nucleo fisico que
// multiplexa el tiempo entre varios contextos de hilo "virtuales" ya
// cargados en el front-end. Aqui se simula justo eso: UN (1) hilo real del
// sistema operativo que se turna, pixel a pixel (el cuantum minimo posible
// en este problema), entre `threads` hilos virtuales. Cada hilo virtual
// tiene su propia franja de filas (mismo reparto que coarseGrained/cmp/smt,
// para que la unica variable entre motores sea COMO y CUANDO se reparte el
// trabajo, no como se particiona la imagen) y su propio "program counter":
// el siguiente pixel que le toca pintar dentro de su franja.
//
// Como solo hay un hilo real, no hace falta ningun candado ni atomico: los
// hilos virtuales nunca se ejecutan de verdad al mismo tiempo, solo se
// alternan. Por el mismo motivo, este motor no deberia dar speedup al
// aumentar `threads` (al contrario: mas cambios de contexto => mas
// overhead de contabilidad); lo que se mide y compara en el reporte es
// justamente ese costo del cambio de contexto frecuente, no una ganancia
// de paralelismo real.
//
// Uso: fineGrained <width> <height> <spheres> <depth> <threads> [salida.bmp]
// `threads` = cantidad de hilos VIRTUALES que se turnan (no hilos del SO).
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "bench.h"
#include "image.h"
#include "raytracer.h"
#include "render.h"
#include "scene.h"

// Cuantum: cantidad fija de pixeles que un hilo virtual pinta antes de
// ceder el turno, en ronda. El enunciado pide el "cuantum minimo", es
// decir 1 (cambio de contexto en cada ciclo). Se deja ajustable solo por
// variable de entorno (FG_QUANTUM) -sin tocar la firma del binario ni los
// scripts de medicion- para poder experimentar con el efecto del tamano
// del cuantum sobre el overhead de cambio de contexto; por defecto es 1.
static int quantum_from_env() {
    const char* v = std::getenv("FG_QUANTUM");
    if (!v) return 1;
    int q = std::atoi(v);
    return (q >= 1) ? q : 1;
}

// Estado de un hilo virtual: su franja [y0,y1) de filas (x siempre recorre
// 0..width) y el siguiente pixel (row, col) que le toca pintar. No se
// comparte con nadie, asi que no hace falta sincronizacion.
struct VThread {
    int y0, y1;
    int row, col;
    bool done() const { return row >= y1; }
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

    // Reparto en franjas contiguas, igual que coarseGrained/cmp/smt.
    std::vector<VThread> vthreads(threads);
    {
        int base  = height / threads;
        int extra = height % threads;
        int y = 0;
        for (int t = 0; t < threads; ++t) {
            int y0 = y;
            int y1 = y + base + (t < extra ? 1 : 0);
            y = y1;
            vthreads[t] = VThread{y0, y1, y0, 0};
        }
    }

    double t0 = now_seconds();

    // Planificador cooperativo por ronda: el unico hilo real recorre los
    // hilos virtuales en orden fijo (round-robin) y le da a cada uno
    // exactamente `quantum` pixeles antes de pasar al siguiente, sin
    // importar si ese hilo "se habria bloqueado". No hay bloqueos reales
    // que simular en este problema, pero el punto arquitectonico es que el
    // cambio de contexto NUNCA depende de eso (a diferencia de grano
    // grueso). Se repiten rondas hasta que todos los hilos virtuales
    // terminaron su franja.
    int remaining = threads;
    std::vector<char> finished(threads, 0);
    while (remaining > 0) {
        for (int t = 0; t < threads; ++t) {
            if (finished[t]) continue;
            VThread& vt = vthreads[t];
            for (int k = 0; k < quantum && !vt.done(); ++k) {
                img.set_pixel(vt.col, vt.row,
                              render_pixel(scene, vt.col, vt.row, width, height, depth));
                if (++vt.col >= width) { vt.col = 0; ++vt.row; }
            }
            if (vt.done()) { finished[t] = 1; --remaining; }
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
