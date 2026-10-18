/*
 * TestForces.c
 *
 * Manual checks for NBodyComputeAcceleration. Build it together with NBody.c
 * (it does not replace Main.c).
 */

#include <math.h>
#include <stdio.h>

#include "NBody.h"

/* Must match SOFTENING in NBody.c. */
#define TEST_SOFTENING 0.05

#define BODY_COUNT   1024
#define INITIAL_SEED 12345ULL

int main(void)
{
    /* Check 1: two bodies of mass 1 placed at distance 1 along the X axis. */
    NBodySystem *pair = NBodyCreate(2);
    if (pair == NULL)
    {
        fprintf(stderr, "Error: could not allocate the pair system.\n");
        return 1;
    }

    pair->Mass[0] = 1.0;
    pair->Mass[1] = 1.0;
    pair->PosX[1] = 1.0;

    NBodyComputeAcceleration(pair, 0, 2);

    double expected = 1.0 / pow(1.0 + TEST_SOFTENING * TEST_SOFTENING, 1.5);
    printf("Check 1 (two bodies)\n");
    printf("  Body 0 AccX: %.10f  (expected  %.10f)\n", pair->AccX[0], expected);
    printf("  Body 1 AccX: %.10f  (expected %.10f)\n", pair->AccX[1], -expected);
    printf("  Body 0 AccY, AccZ: %.3e, %.3e  (expected 0)\n", pair->AccY[0], pair->AccZ[0]);

    NBodyDestroy(pair);

    /* Check 2: total force (sum of mass * acceleration) must be zero. */
    NBodySystem *sys = NBodyCreate(BODY_COUNT);
    if (sys == NULL)
    {
        fprintf(stderr, "Error: could not allocate the system.\n");
        return 1;
    }

    NBodyInit(sys, INITIAL_SEED);
    NBodyComputeAcceleration(sys, 0, BODY_COUNT);

    double forceX = 0.0;
    double forceY = 0.0;
    double forceZ = 0.0;
    for (int i = 0; i < BODY_COUNT; i++)
    {
        forceX += sys->Mass[i] * sys->AccX[i];
        forceY += sys->Mass[i] * sys->AccY[i];
        forceZ += sys->Mass[i] * sys->AccZ[i];
    }
    printf("Check 2 (N = %d)\n", BODY_COUNT);
    printf("  Total force: (%.3e, %.3e, %.3e)  (expected ~1e-17 or smaller)\n", forceX, forceY, forceZ);

    /* Check 3: computing two halves must give the same values as computing everything. */
    double fullFirst = sys->AccX[0];
    double fullLast = sys->AccX[BODY_COUNT - 1];

    for (int i = 0; i < BODY_COUNT; i++)
    {
        sys->AccX[i] = 0.0;
    }

    NBodyComputeAcceleration(sys, 0, BODY_COUNT / 2);
    NBodyComputeAcceleration(sys, BODY_COUNT / 2, BODY_COUNT);

    printf("Check 3 (range split)\n");
    printf("  Body 0 equal: %s, last body equal: %s\n",
           (sys->AccX[0] == fullFirst) ? "yes" : "NO",
           (sys->AccX[BODY_COUNT - 1] == fullLast) ? "yes" : "NO");

    NBodyDestroy(sys);
    return 0;
}