/*
 * Fgmt.h
 *
 * FGMT (Fine-Grained Multithreading) execution model.
 *
 * Unlike CMP (real OS threads, static split, run to completion) and CGMT
 * (real OS threads, dynamic queue, switch only when a thread runs out of
 * its assigned work), FGMT here uses a SINGLE OS thread hosting several
 * cooperative "fibers" (ucontext.h). Every fiber gets a fixed, static
 * range of bodies, exactly like CMP's workers. The difference is HOW it
 * processes that range: instead of doing it all in one go, it processes
 * only "quantumBodies" bodies and then voluntarily gives up control to
 * the next fiber, no matter what (it never "needed" to stop, it always
 * has more work available). This models the defining trait of FGMT:
 * switching every fixed quantum, unconditionally, in contrast to CGMT's
 * event-triggered switching.
 */

#ifndef FGMT_H
#define FGMT_H

#include "NBody.h"

/*
 * Purpose:    Computes acceleration for every body, splitting the work
 *             across "fiberCount" cooperative fibers running on a single
 *             OS thread, round-robin, switching every "quantumBodies".
 * Why:        Lets us measure a fine-grained cooperative scheduler against
 *             CMP and CGMT, which both use real (coarse-grained) OS threads.
 * Parameters: sys           - system to update (AccX/AccY/AccZ written).
 *             fiberCount    - number of cooperative fibers to create.
 *             quantumBodies - how many bodies a fiber processes before it
 *                             yields back to the scheduler.
 * Returns:    Nothing.
 */
void NBodyComputeAccelerationFgmt(NBodySystem *sys, int fiberCount, int quantumBodies);

#endif /* FGMT_H */