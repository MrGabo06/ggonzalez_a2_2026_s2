/*
 * Smt.c
 *
 * Implementation of the SMT execution model: a thin wrapper over the CMP
 * thread pool (see Cmp.c). The distinction between the two models is
 * experimental (how many threads, and whether SMT is on), not mechanical.
 */

#include "Smt.h"
#include "Cmp.h"

void NBodyComputeAccelerationSmt(NBodySystem *sys, int threadCount)
{
    NBodyComputeAccelerationParallel(sys, threadCount);
}