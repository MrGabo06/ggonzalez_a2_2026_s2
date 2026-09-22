/*
 * Cgmt.h
 *
 * CGMT (Coarse-Grained Multithreading) execution model: a fixed pool of
 * threads pulls small chunks of work from a shared queue, one at a time.
 * A thread only switches to a new chunk when it runs out of assigned work
 * (the "costly event" in this model), instead of being given a fixed range
 * up front like CMP.
 */

#ifndef CGMT_H
#define CGMT_H

#include "NBody.h"

/*
 * Purpose:    Computes the acceleration of every body in sys, splitting the
 *             work into chunks of chunkSize bodies, pulled dynamically by
 *             threadCount worker threads.
 * Why:        Fixes the load imbalance CMP can have when N does not divide
 *             evenly among threads: instead of idling, a thread that
 *             finishes early just takes the next available chunk.
 * Parameters: sys         - system with valid positions and masses.
 *             threadCount - number of worker threads.
 *             chunkSize   - number of bodies per chunk (must be >= 1).
 * Returns:    Nothing (AccX/AccY/AccZ are updated).
 */
void NBodyComputeAccelerationCgmt(NBodySystem *sys, int threadCount, int chunkSize);

#endif /* CGMT_H */