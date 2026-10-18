/*
 * Cgmt.c
 *
 * Implementation of the CGMT execution model.
 */

#include <pthread.h>
#include <stdlib.h>

#include "Cgmt.h"

/* Shared state: where the next chunk starts, protected by a mutex so that
 * only one thread at a time reads or updates NextIndex. */
typedef struct
{
    NBodySystem *Sys;
    int ChunkSize;
    int NextIndex;
    pthread_mutex_t Lock;
} TaskQueue;

/*
 * Purpose:    Hands out the next available chunk of body indices to whichever
 *             thread calls this, one chunk per call.
 * Why:        This is the "dynamic" part of CGMT: threads do not receive a
 *             fixed range in advance, they ask for work as they finish it.
 * Parameters: queue       - the shared task queue.
 *             first, last - output: the range of this chunk.
 * Returns:    1 if a chunk was handed out, 0 if no work is left.
 */
static int GetNextChunk(TaskQueue *queue, int *first, int *last)
{
    pthread_mutex_lock(&queue->Lock);

    int start = queue->NextIndex;
    if (start >= queue->Sys->Count)
    {
        pthread_mutex_unlock(&queue->Lock);
        return 0;
    }

    int end = start + queue->ChunkSize;
    if (end > queue->Sys->Count)
    {
        end = queue->Sys->Count;
    }
    queue->NextIndex = end;

    pthread_mutex_unlock(&queue->Lock);

    *first = start;
    *last = end;
    return 1;
}

/*
 * Purpose:    What each worker thread runs: repeatedly pulls a chunk from the
 *             queue and computes it, until no chunks are left.
 * Why:        pthread_create requires this exact signature.
 * Parameters: arg - pointer to the shared TaskQueue.
 * Returns:    NULL (unused).
 */
static void *WorkerLoop(void *arg)
{
    TaskQueue *queue = (TaskQueue *)arg;
    int first, last;

    while (GetNextChunk(queue, &first, &last))
    {
        NBodyComputeAcceleration(queue->Sys, first, last);
    }

    return NULL;
}

void NBodyComputeAccelerationCgmt(NBodySystem *sys, int threadCount, int chunkSize)
{
    TaskQueue queue;
    queue.Sys = sys;
    queue.ChunkSize = chunkSize;
    queue.NextIndex = 0;
    pthread_mutex_init(&queue.Lock, NULL);

    pthread_t threads[threadCount];

    for (int t = 0; t < threadCount; t++)
    {
        pthread_create(&threads[t], NULL, WorkerLoop, &queue);
    }
    for (int t = 0; t < threadCount; t++)
    {
        pthread_join(threads[t], NULL);
    }

    pthread_mutex_destroy(&queue.Lock);
}