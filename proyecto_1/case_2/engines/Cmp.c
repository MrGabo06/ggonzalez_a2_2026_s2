/*
 * Cmp.c
 *
 * Implementation of the CMP execution model.
 */

#include <pthread.h>
#include <stdlib.h>

#include "Cmp.h"

/* Everything a single worker thread needs: its own slice of the system. */
typedef struct
{
    NBodySystem *Sys;
    int First;
    int Last;
} WorkerArgs;

/*
 * Purpose:    Computes the [first, last) range of body indices assigned to
 *             worker "workerId" out of "workerCount" workers splitting "total"
 *             bodies, so that every worker gets a similar amount of work.
 * Why:        "total" bodies rarely divide evenly among "workerCount" workers;
 *             the remainder is spread one extra body at a time to the first
 *             workers, so no worker is left without work and no gap is left
 *             uncomputed.
 * Parameters: total       - number of bodies (sys->Count).
 *             workerCount - number of workers splitting the work.
 *             workerId    - which worker this range is for (0 to workerCount-1).
 *             first, last - output: the range assigned to this worker.
 * Returns:    Nothing (results are written through first and last).
 */
static void ComputeWorkerRange(int total, int workerCount, int workerId, int *first, int *last)
{
    int baseCount = total / workerCount;
    int remainder = total % workerCount;

    if (workerId < remainder)
    {
        *first = workerId * (baseCount + 1);
        *last = *first + baseCount + 1;
    }
    else
    {
        *first = remainder * (baseCount + 1) + (workerId - remainder) * baseCount;
        *last = *first + baseCount;
    }
}

/*
 * Purpose:    Entry point for a worker thread: computes accelerations for
 *             its assigned range only.
 * Why:        pthread_create requires this exact signature (void *in, void *out).
 * Parameters: arg - pointer to this thread's own WorkerArgs.
 * Returns:    NULL (unused).
 */
static void *WorkerComputeAcceleration(void *arg)
{
    WorkerArgs *args = (WorkerArgs *)arg;
    NBodyComputeAcceleration(args->Sys, args->First, args->Last);
    return NULL;
}

void NBodyComputeAccelerationParallel(NBodySystem *sys, int threadCount)
{
    pthread_t threads[threadCount];
    WorkerArgs args[threadCount];

    /* Launch every worker: each gets its own range and starts immediately. */
    for (int t = 0; t < threadCount; t++)
    {
        int first, last;
        ComputeWorkerRange(sys->Count, threadCount, t, &first, &last);

        args[t].Sys = sys;
        args[t].First = first;
        args[t].Last = last;

        pthread_create(&threads[t], NULL, WorkerComputeAcceleration, &args[t]);
    }

    /* Wait for every worker to finish before returning. */
    for (int t = 0; t < threadCount; t++)
    {
        pthread_join(threads[t], NULL);
    }
}