/*
 * NBody.c
 *
 * Implementation of the N-body simulation core (checkpoint A: creation and
 * initialization only). The public interface is declared in NBody.h.
 */

#include <stdlib.h>

#include "NBody.h"

#include <math.h>

#include <stdlib.h>

/* Initial velocities are drawn uniformly from [-INITIAL_SPEED_SCALE, +INITIAL_SPEED_SCALE]. */
#define INITIAL_SPEED_SCALE 0.1

/* xorshift64 gets stuck at zero, so a zero seed is replaced by this constant. */
#define FALLBACK_SEED 0x9E3779B97F4A7C15ULL

/* First outputs of xorshift64 are poor for small seeds, so they are discarded. */
#define GENERATOR_WARMUP_DRAWS 32

/* 2^53 as a double: converts a 53-bit integer into a value in [0, 1). */
#define TWO_POW_53 9007199254740992.0

/* Gravitational constant in normalized units (G = 1). */
#define GRAVITATIONAL_CONSTANT 1.0

/* Plummer softening length: keeps the force finite when two bodies get very close. */
#define SOFTENING 0.05

/* Softening squared, computed once at compile time because the force loop uses it. */
#define SOFTENING_SQUARED (SOFTENING * SOFTENING)

/*
 * Purpose:    Returns the next pseudo-random number in [0, 1).
 * Why:        rand() varies between systems; we need reproducible initial
 *             states. This is the xorshift64 algorithm (Marsaglia, 2003).
 * Parameters: state - pointer to the 64-bit generator state (updated in place).
 * Returns:    A double in the range [0, 1).
 * Note:       "static" makes this function private to this file.
 */
static double NextRandom(uint64_t *state)
{
    uint64_t x = *state;

    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;

    *state = x;

    /* Keep the upper 53 bits (all a double can hold exactly) and scale to [0, 1). */
    return (double)(x >> 11) / TWO_POW_53;
}

NBodySystem *NBodyCreate(int count)
{
    if (count <= 0)
    {
        return NULL;
    }

    /* calloc zeroes the memory, so every pointer starts as NULL and every value as 0. */
    NBodySystem *sys = calloc(1, sizeof(NBodySystem));
    if (sys == NULL)
    {
        return NULL;
    }

    sys->Count = count;
    sys->PosX = calloc((size_t)count, sizeof(double));
    sys->PosY = calloc((size_t)count, sizeof(double));
    sys->PosZ = calloc((size_t)count, sizeof(double));
    sys->VelX = calloc((size_t)count, sizeof(double));
    sys->VelY = calloc((size_t)count, sizeof(double));
    sys->VelZ = calloc((size_t)count, sizeof(double));
    sys->AccX = calloc((size_t)count, sizeof(double));
    sys->AccY = calloc((size_t)count, sizeof(double));
    sys->AccZ = calloc((size_t)count, sizeof(double));
    sys->Mass = calloc((size_t)count, sizeof(double));

    /* If any allocation failed, release what was obtained (free(NULL) is safe). */
    if (sys->PosX == NULL || sys->PosY == NULL || sys->PosZ == NULL ||
        sys->VelX == NULL || sys->VelY == NULL || sys->VelZ == NULL ||
        sys->AccX == NULL || sys->AccY == NULL || sys->AccZ == NULL ||
        sys->Mass == NULL)
    {
        NBodyDestroy(sys);
        return NULL;
    }

    return sys;
}

void NBodyDestroy(NBodySystem *sys)
{
    if (sys == NULL)
    {
        return;
    }

    free(sys->PosX);
    free(sys->PosY);
    free(sys->PosZ);
    free(sys->VelX);
    free(sys->VelY);
    free(sys->VelZ);
    free(sys->AccX);
    free(sys->AccY);
    free(sys->AccZ);
    free(sys->Mass);
    free(sys);
}

double NBodyTotalMass(const NBodySystem *sys)
{
    double total = 0.0;

    for (int i = 0; i < sys->Count; i++)
    {
        total += sys->Mass[i];
    }

    return total;
}

void NBodyTotalMomentum(const NBodySystem *sys, double *px, double *py, double *pz)
{
    double sumX = 0.0;
    double sumY = 0.0;
    double sumZ = 0.0;

    for (int i = 0; i < sys->Count; i++)
    {
        sumX += sys->Mass[i] * sys->VelX[i];
        sumY += sys->Mass[i] * sys->VelY[i];
        sumZ += sys->Mass[i] * sys->VelZ[i];
    }

    *px = sumX;
    *py = sumY;
    *pz = sumZ;
}

void NBodyInit(NBodySystem *sys, uint64_t seed)
{
    uint64_t state = (seed != 0) ? seed : FALLBACK_SEED;

    for (int w = 0; w < GENERATOR_WARMUP_DRAWS; w++)
    {
        NextRandom(&state);
    }

    /* All bodies have the same mass and the masses add up to 1. */
    double massEach = 1.0 / (double)sys->Count;

    for (int i = 0; i < sys->Count; i++)
    {
        /* Positions: uniform in the unit cube [0, 1)^3. */
        sys->PosX[i] = NextRandom(&state);
        sys->PosY[i] = NextRandom(&state);
        sys->PosZ[i] = NextRandom(&state);

        /* Velocities: uniform in [-INITIAL_SPEED_SCALE, +INITIAL_SPEED_SCALE]. */
        sys->VelX[i] = (2.0 * NextRandom(&state) - 1.0) * INITIAL_SPEED_SCALE;
        sys->VelY[i] = (2.0 * NextRandom(&state) - 1.0) * INITIAL_SPEED_SCALE;
        sys->VelZ[i] = (2.0 * NextRandom(&state) - 1.0) * INITIAL_SPEED_SCALE;

        sys->AccX[i] = 0.0;
        sys->AccY[i] = 0.0;
        sys->AccZ[i] = 0.0;

        sys->Mass[i] = massEach;
    }

    /*
     * Remove the velocity of the center of mass so the total momentum is zero:
     * centerVelocity = (sum of mass * velocity) / (sum of mass).
     */
    double px, py, pz;
    NBodyTotalMomentum(sys, &px, &py, &pz);
    double totalMass = NBodyTotalMass(sys);

    for (int i = 0; i < sys->Count; i++)
    {
        sys->VelX[i] -= px / totalMass;
        sys->VelY[i] -= py / totalMass;
        sys->VelZ[i] -= pz / totalMass;
    }
}

void NBodyComputeAcceleration(NBodySystem *sys, int first, int last)
{
    for (int i = first; i < last; i++)
    {
        /* Position of body i, copied once into local variables. */
        double posXi = sys->PosX[i];
        double posYi = sys->PosY[i];
        double posZi = sys->PosZ[i];

        /* Running totals of the pull that all other bodies exert on body i. */
        double sumX = 0.0;
        double sumY = 0.0;
        double sumZ = 0.0;

                for (int j = 0; j < sys->Count; j++)
        {
            /* Vector pointing from body i to body j. */
            double dx = sys->PosX[j] - posXi;
            double dy = sys->PosY[j] - posYi;
            double dz = sys->PosZ[j] - posZi;

            /* Squared distance between the two bodies, plus the softening. */
            double distSquared = dx * dx + dy * dy + dz * dz + SOFTENING_SQUARED;

            /* 1 / distance^3, built from one square root and multiplications. */
            double invDist = 1.0 / sqrt(distSquared);
            double invDistCubed = invDist * invDist * invDist;

            /* How strongly body j pulls on body i (before applying the direction). */
            double factor = sys->Mass[j] * invDistCubed;

            /* Add the pull of body j, pointing along the vector (dx, dy, dz). */
            sumX += dx * factor;
            sumY += dy * factor;
            sumZ += dz * factor;
        }

                /* Store the total acceleration of body i (G is applied once, here). */
        sys->AccX[i] = GRAVITATIONAL_CONSTANT * sumX;
        sys->AccY[i] = GRAVITATIONAL_CONSTANT * sumY;
        sys->AccZ[i] = GRAVITATIONAL_CONSTANT * sumZ;
    }
}

void NBodyKick(NBodySystem *sys, double halfStep, int first, int last)
{
    for (int i = first; i < last; i++)
    {
        sys->VelX[i] += halfStep * sys->AccX[i];
        sys->VelY[i] += halfStep * sys->AccY[i];
        sys->VelZ[i] += halfStep * sys->AccZ[i];
    }
}

void NBodyDrift(NBodySystem *sys, double step, int first, int last)
{
    for (int i = first; i < last; i++)
    {
        sys->PosX[i] += step * sys->VelX[i];
        sys->PosY[i] += step * sys->VelY[i];
        sys->PosZ[i] += step * sys->VelZ[i];
    }
}