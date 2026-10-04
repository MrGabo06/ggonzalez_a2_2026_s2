/*
 * Smt.h
 *
 * SMT (Simultaneous Multithreading) execution model: reuses the CMP thread
 * pool, but is meant to be run with more threads than physical cores
 * (oversubscription), and compared with SMT enabled vs disabled at the OS
 * level (/sys/devices/system/cpu/smt/control).
 */

#ifndef SMT_H
#define SMT_H

#include "NBody.h"

/*
 * Purpose:    Computes the acceleration of every body in sys using threadCount
 *             threads. Identical mechanism to the CMP model; what makes this
 *             an "SMT" run is choosing threadCount above the physical core
 *             count, and/or toggling SMT itself before calling this.
 * Why:        Named separately from Cmp so measurements and diagrams keep the
 *             two experiments (real parallelism vs. oversubscription) distinct,
 *             even though the underlying thread pool code is shared.
 * Parameters: sys         - system with valid positions and masses.
 *             threadCount - number of worker threads to use.
 * Returns:    Nothing (AccX/AccY/AccZ are updated).
 */
void NBodyComputeAccelerationSmt(NBodySystem *sys, int threadCount);

#endif /* SMT_H */