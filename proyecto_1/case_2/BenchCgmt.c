/*
 * BenchCgmt.c
 *
 * Times the full simulation loop using the CGMT model for the force phase,
 * for a chosen number of threads and chunk size.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "NBody.h"
#include "engines/Cgmt.h"

#define TIME_STEP           0.001
#define DEFAULT_BODY_COUNT  1024
#define DEFAULT_STEP_COUNT  200
#define DEFAULT_THREADS     8
#define DEFAULT_CHUNK_SIZE  32
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
    int chunkSize = (argc > 4) ? atoi(argv[4]) : DEFAULT_CHUNK_SIZE;

    NBodySystem *sys = NBodyCreate(bodyCount);
    if (sys == NULL)
    {
        fprintf(stderr, "Error: could not allocate the system.\n");
        return 1;
    }

    NBodyInit(sys, DEFAULT_SEED);
    NBodyComputeAccelerationCgmt(sys, threadCount, chunkSize);

    double startTime = NowSeconds();
    for (int step = 0; step < stepCount; step++)
    {
        NBodyKick(sys, 0.5 * TIME_STEP, 0, sys->Count);
        NBodyDrift(sys, TIME_STEP, 0, sys->Count);
        NBodyComputeAccelerationCgmt(sys, threadCount, chunkSize);
        NBodyKick(sys, 0.5 * TIME_STEP, 0, sys->Count);
    }
    double totalSeconds = NowSeconds() - startTime;

    double interactions = (double)stepCount * (double)bodyCount * (double)bodyCount;

    printf("model,n,steps,workers,chunk,time_sec,checksum,ns_per_interaction\n");
    printf("cgmt,%d,%d,%d,%d,%.9f,%.6f,%.3f\n",
           bodyCount, stepCount, threadCount, chunkSize, totalSeconds,
           NBodyChecksum(sys), 1e9 * totalSeconds / interactions);

    NBodyDestroy(sys);
    return 0;
}