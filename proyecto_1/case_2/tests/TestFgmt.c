/*
 * TestFgmt.c
 *
 * Correctness test for the FGMT model.
 *
 * Computes a sequential reference once, then re-computes the same system
 * with NBodyComputeAccelerationFgmt across many (fiberCount, quantumBodies)
 * combinations, including edge cases: more fibers than bodies, a quantum
 * bigger than any fiber's whole range (single yield-free pass), and a
 * quantum of 1 (maximum number of switches). Every combination must match
 * the sequential result exactly, body by body, component by component.
 */

#include <stdio.h>
#include "../NBody.h"
#include "../engines/Fgmt.h"

#define N 1024

int main(void)
{
    NBodySystem *reference = NBodyCreate(N);
    NBodyInit(reference, 12345ULL);
    NBodyComputeAcceleration(reference, 0, N);

    int fiberCounts[] = {1, 2, 3, 4, 5, 8, 16, 1024, 2000};
    int quantums[] = {1, 7, 13, 64, 128, 1024, 5000};
    int fiberCountsLen = (int)(sizeof(fiberCounts) / sizeof(fiberCounts[0]));
    int quantumsLen = (int)(sizeof(quantums) / sizeof(quantums[0]));

    int allOk = 1;

    for (int fc = 0; fc < fiberCountsLen; fc++)
    {
        for (int q = 0; q < quantumsLen; q++)
        {
            NBodySystem *parallel = NBodyCreate(N);
            NBodyInit(parallel, 12345ULL);
            NBodyComputeAccelerationFgmt(parallel, fiberCounts[fc], quantums[q]);

            int matches = 1;
            for (int i = 0; i < N; i++)
            {
                if (reference->AccX[i] != parallel->AccX[i] ||
                    reference->AccY[i] != parallel->AccY[i] ||
                    reference->AccZ[i] != parallel->AccZ[i])
                {
                    matches = 0;
                    break;
                }
            }

            if (!matches)
            {
                printf("fibers=%4d quantum=%4d : MISMATCH\n", fiberCounts[fc], quantums[q]);
                allOk = 0;
            }

            NBodyDestroy(parallel);
        }
    }

    if (allOk)
    {
        printf("ALL OK (%d combos)\n", fiberCountsLen * quantumsLen);
    }
    else
    {
        printf("FAIL\n");
    }

    NBodyDestroy(reference);
    return allOk ? 0 : 1;
}