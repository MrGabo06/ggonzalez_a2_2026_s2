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

/*
 * Purpose:    Computes the gravitational acceleration of the bodies in the range
 *             [first, last) caused by ALL bodies of the system.
 * Why:        This is the O(N^2) core of the simulation. It receives a range so
 *             that, later, each thread can compute its own slice.
 * Parameters: sys   - initialized system; positions and masses are read,
 *                     AccX/AccY/AccZ are written only for indices in the range.
 *             first - first body to compute (inclusive).
 *             last  - one past the last body to compute (exclusive).
 * Returns:    Nothing (results are stored in sys->AccX, AccY and AccZ).
 */
void NBodyComputeAcceleration(NBodySystem *sys, int first, int last);

/*
 * Purpose:    Updates the velocity of the bodies in [first, last) using their
 *             current acceleration: Vel = Vel + halfStep * Acc.
 * Why:        First and last phase of every velocity-Verlet step. It is called
 *             twice per step, each time with half of the time step.
 * Parameters: sys      - system with valid accelerations (AccX/AccY/AccZ).
 *             halfStep - half of the time step (dt / 2).
 *             first    - first body to update (inclusive).
 *             last     - one past the last body to update (exclusive).
 * Returns:    Nothing (velocities are updated in place).
 */
void NBodyKick(NBodySystem *sys, double halfStep, int first, int last);

#endif /* NBODY_H */