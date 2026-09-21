/*
 * Main.c
 *
 * Program entry point for checkpoint A: creates a system, initializes it and
 * prints basic sanity values (total mass and total momentum).
 */

#include <stdio.h>

#include "NBody.h"

#define BODY_COUNT   1024
#define INITIAL_SEED 12345ULL

int main(void)
{
    NBodySystem *sys = NBodyCreate(BODY_COUNT);
    if (sys == NULL)
    {
        fprintf(stderr, "Error: could not allocate the system.\n");
        return 1;
    }

    NBodyInit(sys, INITIAL_SEED);

    double px, py, pz;
    NBodyTotalMomentum(sys, &px, &py, &pz);

    printf("Bodies:         %d\n", sys->Count);
    printf("Total mass:     %.17g\n", NBodyTotalMass(sys));
    printf("Total momentum: (%.3e, %.3e, %.3e)\n", px, py, pz);
    printf("Body 0 position: (%.6f, %.6f, %.6f)\n", sys->PosX[0], sys->PosY[0], sys->PosZ[0]);

    NBodyDestroy(sys);
    return 0;
}