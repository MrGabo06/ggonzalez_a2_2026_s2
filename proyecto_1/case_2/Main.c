/*
 * Main.c
 *
 * Program entry point: creates a system, initializes it, advances it a number of
 * time steps with velocity-Verlet and prints basic sanity values before and after.
 */

#include <stdio.h>

#include "NBody.h"

#define BODY_COUNT   1024
#define INITIAL_SEED 12345ULL
#define TIME_STEP    0.001
#define STEP_COUNT   1000

int main(void)
{
    NBodySystem *sys = NBodyCreate(BODY_COUNT);
    if (sys == NULL)
    {
        fprintf(stderr, "Error: could not allocate the system.\n");
        return 1;
    }

    NBodyInit(sys, INITIAL_SEED);

    /* The first half kick needs valid accelerations, so compute them once. */
    NBodyComputeAcceleration(sys, 0, sys->Count);

    double px, py, pz;
    NBodyTotalMomentum(sys, &px, &py, &pz);

    printf("Bodies:          %d\n", sys->Count);
    printf("Total mass:      %.17g\n", NBodyTotalMass(sys));
    printf("Initial momentum: (%.3e, %.3e, %.3e)\n", px, py, pz);
    printf("Initial body 0:   (%.6f, %.6f, %.6f)\n", sys->PosX[0], sys->PosY[0], sys->PosZ[0]);

    for (int step = 0; step < STEP_COUNT; step++)
    {
        NBodyStep(sys, TIME_STEP);
    }

    NBodyTotalMomentum(sys, &px, &py, &pz);

    printf("Steps done:      %d (time step %g)\n", STEP_COUNT, TIME_STEP);
    printf("Final momentum:   (%.3e, %.3e, %.3e)\n", px, py, pz);
    printf("Final body 0:     (%.6f, %.6f, %.6f)\n", sys->PosX[0], sys->PosY[0], sys->PosZ[0]);

    NBodyDestroy(sys);
    return 0;
}