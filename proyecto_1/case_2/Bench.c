/*
 * Bench.c
 *
 * Timing program for the sequential N-body baseline.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "NBody.h"

#define TIME_STEP          0.001
#define DEFAULT_BODY_COUNT 1024
#define DEFAULT_STEP_COUNT 200
#define DEFAULT_SEED       12345ULL

/*
 * Purpose:    Returns the current value of a monotonic clock, in seconds.
 * Why:        Differences between two calls measure elapsed wall-clock time.
 * Returns:    Seconds as a double (only differences are meaningful).
 */
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
    uint64_t seed = (argc > 3) ? strtoull(argv[3], NULL, 10) : DEFAULT_SEED;

    NBodySystem *sys = NBodyCreate(bodyCount);
    if (sys == NULL)
    {
        fprintf(stderr, "Error: could not allocate the system.\n");
        return 1;
    }

    NBodyInit(sys, seed);
    NBodyComputeAcceleration(sys, 0, sys->Count);

       double kickSeconds = 0.0;
    double driftSeconds = 0.0;
    double forceSeconds = 0.0;

    double startTime = NowSeconds();
    for (int step = 0; step < stepCount; step++)
    {
        /* Same four phases, in the same order, as NBodyStep, each one timed. */
        double t0 = NowSeconds();
        NBodyKick(sys, 0.5 * TIME_STEP, 0, sys->Count);
        double t1 = NowSeconds();
        NBodyDrift(sys, TIME_STEP, 0, sys->Count);
        double t2 = NowSeconds();
        NBodyComputeAcceleration(sys, 0, sys->Count);
        double t3 = NowSeconds();
        NBodyKick(sys, 0.5 * TIME_STEP, 0, sys->Count);
        double t4 = NowSeconds();

        kickSeconds += (t1 - t0) + (t4 - t3);
        driftSeconds += t2 - t1;
        forceSeconds += t3 - t2;
    }
    double totalSeconds = NowSeconds() - startTime;

        double interactions = (double)stepCount * (double)bodyCount * (double)bodyCount;

    /* Same column style as case_1; the last four columns are specific to this case. */
    printf("model,n,steps,workers,time_sec,checksum,ns_per_interaction,kick_sec,drift_sec,force_sec\n");
    printf("sequential,%d,%d,1,%.9f,%.6f,%.3f,%.9f,%.9f,%.9f\n",
           bodyCount, stepCount, totalSeconds, NBodyChecksum(sys),
           1e9 * totalSeconds / interactions, kickSeconds, driftSeconds, forceSeconds);

    NBodyDestroy(sys);
    return 0;
}