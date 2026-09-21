/*
 * Cmp.h
 *
 * CMP (Chip Multi-Processor) execution model: splits the O(N^2) force
 * computation across real OS threads, one per requested worker.
 */

#ifndef CMP_H
#define CMP_H

#include "NBody.h"

/*
 * Purpose:    Computes the acceleration of every body in sys, splitting the
 *             work across threadCount real threads (CMP model).
 * Why:        Same math as NBodyComputeAcceleration, but using several CPU
 *             cores at once instead of a single one.
 * Parameters: sys         - system with valid positions and masses.
 *             threadCount - number of worker threads to use (must be >= 1).
 * Returns:    Nothing (AccX/AccY/AccZ are updated, same as the sequential version).
 */
void NBodyComputeAccelerationParallel(NBodySystem *sys, int threadCount);

#endif /* CMP_H */