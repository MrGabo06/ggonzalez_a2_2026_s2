/*
 * TestForces.c
 *
 * Automated checks for NBodyComputeAcceleration. Build it together with
 * NBody.c (it does not replace Main.c). Every check is verified against a
 * numeric tolerance and tallied in FailureCount; main() returns a non-zero
 * exit code if any check fails, so `make test-forces` (and `make test`) can
 * detect a broken force calculation on their own, without a human reading
 * printed numbers.
 */

#include <math.h>
#include <stdio.h>

#include "NBody.h"

/* Must match SOFTENING in NBody.c. */
#define TEST_SOFTENING 0.05

#define BODY_COUNT   1024
#define INITIAL_SEED 12345ULL

/* Tolerance for checks that involve floating-point arithmetic (pow/sqrt
 * rounding). Check 3 (range split) compares with exact "==" on purpose: it
 * is the same work done in the same order, just split into two calls, so it
 * must be bit-for-bit identical, not just "close". */
#define TOLERANCE 1e-9

static int FailureCount = 0;

/*
 * Purpose:    Reports one check's result, tallies failures, and keeps every
 *             check self-verifying instead of just printing a number for a
 *             human to eyeball.
 * Parameters: description - short label for this check.
 *             passed      - 1 if the check held, 0 otherwise.
 */
static void ReportCheck(const char *description, int passed)
{
    printf("%-28s : %s\n", description, passed ? "OK" : "FAIL");
    if (!passed)
    {
        FailureCount++;
    }
}

int main(void)
{
    /* Check 1: two bodies of mass 1 placed at distance 1 along the X axis.
     * The expected acceleration comes from the softened gravity formula
     * directly, independent of NBodyComputeAcceleration's implementation. */
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

    ReportCheck("  body 0 AccX matches", fabs(pair->AccX[0] - expected) < TOLERANCE);
    ReportCheck("  body 1 AccX matches", fabs(pair->AccX[1] + expected) < TOLERANCE);
    ReportCheck("  body 0 AccY/AccZ ~= 0", fabs(pair->AccY[0]) < TOLERANCE && fabs(pair->AccZ[0]) < TOLERANCE);

    NBodyDestroy(pair);

    /* Check 2: total force (sum of mass * acceleration) must be zero -- this
     * is Newton's third law: every pairwise force is applied in both
     * directions, so the system-wide sum has to cancel out. */
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

    ReportCheck("  total force ~= 0", fabs(forceX) < TOLERANCE && fabs(forceY) < TOLERANCE && fabs(forceZ) < TOLERANCE);

    /* Check 3: computing two halves must give the same values as computing
     * everything at once. This is the property every threaded model (CMP,
     * CGMT, FGMT) relies on when it splits the body range across workers;
     * if this ever failed, none of them could be correct either. */
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

    ReportCheck("  first body exact match", sys->AccX[0] == fullFirst);
    ReportCheck("  last body exact match", sys->AccX[BODY_COUNT - 1] == fullLast);

    NBodyDestroy(sys);

    if (FailureCount == 0)
    {
        printf("ALL OK\n");
    }
    else
    {
        printf("FAILURES FOUND (%d)\n", FailureCount);
    }

    return (FailureCount == 0) ? 0 : 1;
}