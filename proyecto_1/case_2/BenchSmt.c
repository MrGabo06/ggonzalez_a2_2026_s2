/*
 * BenchSmt.c
 *
 * Times the full simulation loop using the SMT model for the force phase.
 * Meant to be run with threadCount above the physical core count, and with
 * SMT toggled on/off at the OS level between runs (see the project notes).
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "NBody.h"
#include "engines/Smt.h"

#define TIME_STEP           0.001
#define DEFAULT_BODY_COUNT  1024
#define DEFAULT_STEP_COUNT  200
#define DEFAULT_THREADS     16
#define DEFAULT_SEED        12345ULL

static double NowSeconds(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec * 1e-9;
}

int main(int argc, char **argv)
{
    int bodyCount = (argc > 1) ? atoi(argv[1]) : DEFAULT_BODY_COUNT;
    int stepCount = (argc > 2) ? atoi(argv[2]) : DEFAULT_STEP_COUNT;
    int threadCount = (argc > 3) ? atoi(argv[3]) : DEFAULT_THREADS;

    NBodySystem *sys = NBodyCreate(bodyCount);
    if (sys == NULL)
    {
        fprintf(stderr, "Error: could not allocate the system.\n");
        return 1;
    }

    NBodyInit(sys, DEFAULT_SEED);
    NBodyComputeAccelerationSmt(sys, threadCount);

    double startTime = NowSeconds();
    for (int step = 0; step < stepCount; step++)
    {
        NBodyKick(sys, 0.5 * TIME_STEP, 0, sys->Count);
        NBodyDrift(sys, TIME_STEP, 0, sys->Count);
        NBodyComputeAccelerationSmt(sys, threadCount);
        NBodyKick(sys, 0.5 * TIME_STEP, 0, sys->Count);
    }
    double totalSeconds = NowSeconds() - startTime;

    double interactions = (double)stepCount * (double)bodyCount * (double)bodyCount;

    printf("model,n,steps,workers,time_sec,checksum,ns_per_interaction\n");
    printf("smt,%d,%d,%d,%.9f,%.6f,%.3f\n",
           bodyCount, stepCount, threadCount, totalSeconds,
           NBodyChecksum(sys), 1e9 * totalSeconds / interactions);

    NBodyDestroy(sys);
    return 0;
}