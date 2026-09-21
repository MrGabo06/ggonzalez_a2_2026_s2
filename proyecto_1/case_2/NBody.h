/*
 * NBody.h
 *
 * Public interface of the N-body simulation core.
 * This file only DECLARES what exists (data layout and function names).
 * The implementation of every function lives in NBody.c.
 */

#ifndef NBODY_H
#define NBODY_H

#include <stdint.h>

/*
 * NBodySystem
 *
 * Holds the complete state of N bodies using a "structure of arrays" layout:
 * one array per property instead of one struct per body. Element i of every
 * array belongs to body i (for example PosX[i], PosY[i], PosZ[i] is the
 * position of body i).
 *
 * Why: the force loop reads the positions of all bodies one after another,
 * so keeping each coordinate contiguous in memory is cache friendly.
 */
typedef struct
{
    int Count;      /* Number of bodies (N). */

    double *PosX;   /* Position components. */
    double *PosY;
    double *PosZ;

    double *VelX;   /* Velocity components. */
    double *VelY;
    double *VelZ;

    double *AccX;   /* Acceleration components (computed in a later checkpoint). */
    double *AccY;
    double *AccZ;

    double *Mass;   /* Mass of each body. */
} NBodySystem;

/*
 * Purpose:    Allocates a system able to hold "count" bodies, all values zeroed.
 * Why:        Centralizes memory management so callers never touch malloc/free.
 * Parameters: count - number of bodies, must be greater than zero.
 * Returns:    Pointer to the new system, or NULL if count is invalid or
 *             memory could not be allocated.
 */
NBodySystem *NBodyCreate(int count);

/*
 * Purpose:    Releases a system created with NBodyCreate.
 * Why:        Every allocation must have a matching release (no memory leaks).
 * Parameters: sys - system to release; passing NULL is allowed and does nothing.
 * Returns:    Nothing.
 */
void NBodyDestroy(NBodySystem *sys);

/*
 * Purpose:    Fills the system with a reproducible initial state: random
 *             positions in the unit cube, small random velocities, equal
 *             masses that add up to 1, and zero total momentum.
 * Why:        The same seed always gives the same state, so every execution
 *             model can be compared starting from identical conditions.
 * Parameters: sys  - system created with NBodyCreate.
 *             seed - starting value of the random generator.
 * Returns:    Nothing.
 */
void NBodyInit(NBodySystem *sys, uint64_t seed);

/*
 * Purpose:    Computes the sum of all body masses.
 * Why:        Sanity check of initialization (must be 1) and needed to find
 *             the velocity of the center of mass.
 * Parameters: sys - initialized system.
 * Returns:    Total mass.
 */
double NBodyTotalMass(const NBodySystem *sys);

/*
 * Purpose:    Computes the total linear momentum (sum of mass * velocity).
 * Why:        Momentum must stay at zero throughout the simulation; it is one
 *             of the correctness checks used later by the verification step.
 * Parameters: sys - initialized system.
 *             px, py, pz - output: components of the total momentum.
 * Returns:    Nothing (results are written through the output pointers).
 */
void NBodyTotalMomentum(const NBodySystem *sys, double *px, double *py, double *pz);

#endif /* NBODY_H */