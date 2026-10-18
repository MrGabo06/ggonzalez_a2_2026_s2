/*
 * TestCmp.c
 *
 * Compares the CMP (multithreaded) acceleration against the sequential one,
 * for several thread counts, including edge cases.
 */

#include <stdio.h>

#include "NBody.h"
#include "Cmp.h"

#define BODY_COUNT   1024
#define INITIAL_SEED 12345ULL

/*
 * Purpose:    Checks whether two systems have exactly the same accelerations.
 * Parameters: a, b - systems with the same Count.
 * Returns:    1 if every AccX/AccY/AccZ matches exactly, 0 otherwise.
 */
static int AccelerationsMatch(const NBodySystem *a, const NBodySystem *b)
{
    for (int i = 0; i < a->Count; i++)
    {
        if (a->AccX[i] != b->AccX[i] || a->AccY[i] != b->AccY[i] || a->AccZ[i] != b->AccZ[i])
        {
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    /* Thread counts to check: typical values, the hardware's core/thread counts,
     * and two edge cases (as many threads as bodies, and more threads than bodies). */
    int threadCounts[] = {1, 2, 3, 4, 5, 6, 7, 8, 16, BODY_COUNT, 2000};
    int caseCount = sizeof(threadCounts) / sizeof(threadCounts[0]);

    NBodySystem *reference = NBodyCreate(BODY_COUNT);
    if (reference == NULL)
    {
        fprintf(stderr, "Error: could not allocate the reference system.\n");
        return 1;
    }
    NBodyInit(reference, INITIAL_SEED);
    NBodyComputeAcceleration(reference, 0, reference->Count);

    int allMatch = 1;
    for (int c = 0; c < caseCount; c++)
    {
        int threadCount = threadCounts[c];

        NBodySystem *parallel = NBodyCreate(BODY_COUNT);
        if (parallel == NULL)
        {
            fprintf(stderr, "Error: could not allocate a parallel system.\n");
            return 1;
        }
        NBodyInit(parallel, INITIAL_SEED);
        NBodyComputeAccelerationParallel(parallel, threadCount);

        int matches = AccelerationsMatch(reference, parallel);
        printf("Threads = %5d : %s\n", threadCount, matches ? "match" : "MISMATCH");
        if (!matches)
        {
            allMatch = 0;
        }

        NBodyDestroy(parallel);
    }

    NBodyDestroy(reference);

    printf(allMatch ? "ALL OK\n" : "FAILURES FOUND\n");
    return allMatch ? 0 : 1;
}