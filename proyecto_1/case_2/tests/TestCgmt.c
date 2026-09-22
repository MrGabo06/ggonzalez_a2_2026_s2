/*
 * TestCgmt.c
 *
 * Compares the CGMT (dynamic task queue) acceleration against the sequential
 * one, for several combinations of thread count and chunk size.
 */

#include <stdio.h>

#include "NBody.h"
#include "Cgmt.h"

#define BODY_COUNT   1024
#define INITIAL_SEED 12345ULL

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
    NBodySystem *reference = NBodyCreate(BODY_COUNT);
    if (reference == NULL)
    {
        fprintf(stderr, "Error: could not allocate the reference system.\n");
        return 1;
    }
    NBodyInit(reference, INITIAL_SEED);
    NBodyComputeAcceleration(reference, 0, reference->Count);

    /* Chunk sizes that do not divide 1024 evenly are included on purpose:
     * that is exactly the case where CMP's static split idles a thread. */
    int chunkSizes[] = {1, 7, 13, 64, 128, 1024};
    int threadCounts[] = {1, 3, 5, 8};

    int chunkCaseCount = sizeof(chunkSizes) / sizeof(chunkSizes[0]);
    int threadCaseCount = sizeof(threadCounts) / sizeof(threadCounts[0]);

    int allMatch = 1;
    for (int tc = 0; tc < threadCaseCount; tc++)
    {
        for (int cc = 0; cc < chunkCaseCount; cc++)
        {
            NBodySystem *parallel = NBodyCreate(BODY_COUNT);
            if (parallel == NULL)
            {
                fprintf(stderr, "Error: could not allocate a parallel system.\n");
                return 1;
            }
            NBodyInit(parallel, INITIAL_SEED);
            NBodyComputeAccelerationCgmt(parallel, threadCounts[tc], chunkSizes[cc]);

            int matches = AccelerationsMatch(reference, parallel);
            printf("threads=%d chunk=%5d : %s\n", threadCounts[tc], chunkSizes[cc],
                   matches ? "match" : "MISMATCH");
            if (!matches)
            {
                allMatch = 0;
            }

            NBodyDestroy(parallel);
        }
    }

    NBodyDestroy(reference);

    printf(allMatch ? "ALL OK\n" : "FAILURES FOUND\n");
    return allMatch ? 0 : 1;
}