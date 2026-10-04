/*
 * BenchFgmt.c
 *
 * Timing tool for the FGMT model. Runs a full velocity-Verlet simulation
 * (kick, drift, recompute forces, kick) for "steps" steps, using
 * NBodyComputeAccelerationFgmt for every force computation, and prints one
 * CSV row with the total time and a checksum (so the result can be checked
 * against the sequential and CMP/CGMT reference values).
 *
 * Usage: ./benchfgmt <N> <steps> <fiberCount> <quantumBodies> [seed]
 *
 * CSV columns: model,n,steps,workers,quantum,time_sec,checksum,ns_per_interaction
 * "workers" is the fiber count (kept with the same column name used by
 * BenchCmp/BenchCgmt so every model's CSV lines up in the same table).
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "NBody.h"
#include "engines/Fgmt.h"

static double NowSeconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(int argc, char **argv)
{
    if (argc < 5)
    {
        fprintf(stderr, "Uso: %s <N> <steps> <fiberCount> <quantumBodies> [seed]\n", argv[0]);
        return 1;
    }

    int n = atoi(argv[1]);
    int steps = atoi(argv[2]);
    int fiberCount = atoi(argv[3]);
    int quantumBodies = atoi(argv[4]);
    unsigned long long seed = (argc > 5) ? (unsigned long long)atoll(argv[5]) : 12345ULL;

    NBodySystem *sys = NBodyCreate(n);
    NBodyInit(sys, seed);

    NBodyComputeAccelerationFgmt(sys, fiberCount, quantumBodies);

    double t0 = NowSeconds();
    for (int s = 0; s < steps; s++)
    {
        NBodyKick(sys, 0.001, 0, n);
        NBodyDrift(sys, 0.001, 0, n);
        NBodyComputeAccelerationFgmt(sys, fiberCount, quantumBodies);
        NBodyKick(sys, 0.001, 0, n);
    }
    double t1 = NowSeconds();

    double elapsed = t1 - t0;
    double interactions = (double)n * (double)n * (double)steps;
    double nsPerInteraction = (elapsed * 1e9) / interactions;

    printf("model,n,steps,workers,quantum,time_sec,checksum,ns_per_interaction\n");
    printf("fgmt,%d,%d,%d,%d,%.6f,%.6f,%.4f\n",
           n, steps, fiberCount, quantumBodies, elapsed, NBodyChecksum(sys), nsPerInteraction);

    NBodyDestroy(sys);
    return 0;
}