// bench.h - Cronometro y salida en consola, iguales para todos los motores.
// Mismo formato en todos para poder comparar los tiempos entre modelos.
#ifndef BENCH_H
#define BENCH_H

#include <chrono>
#include <cstdio>

// Segundos con un reloj monotonico (no salta si cambia la hora del sistema).
inline double now_seconds() {
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

inline void print_csv_header() {
    std::printf("model,width,height,spheres,depth,workers,time_sec\n");
}

// workers: cantidad de hilos que uso el motor (1 en el secuencial).
inline void print_csv_row(const char* model, int width, int height, int spheres,
                          int depth, int workers, double time_sec) {
    std::printf("%s,%d,%d,%d,%d,%d,%.9f\n", model, width, height, spheres, depth,
                workers, time_sec);
}

#endif  // BENCH_H
