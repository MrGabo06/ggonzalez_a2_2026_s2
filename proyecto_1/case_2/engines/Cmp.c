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