/*
 * Fgmt.c
 *
 * Implementation of the FGMT execution model. See Fgmt.h for the design
 * summary. This file is the only place in the project that touches
 * ucontext.h (getcontext / makecontext / swapcontext).
 */

#include <ucontext.h>
#include <stdlib.h>
#include "Fgmt.h"

#define FIBER_STACK_SIZE 65536

/*
 * Fiber
 *
 * State of one cooperative fiber: its own execution context (registers +
 * stack pointer, saved/restored by swapcontext), its own stack memory,
 * the fixed body range it owns, where it currently is inside that range
 * (Cursor), how many bodies it processes per quantum, and whether it has
 * already finished its whole range.
 */
typedef struct
{
    ucontext_t Context;
    char *Stack;
    NBodySystem *Sys;
    int First;
    int Last;
    int Cursor;
    int QuantumBodies;
    int Finished;
} Fiber;

/*
 * These three are file-scope (static) because ucontext's makecontext only
 * accepts a plain "void (void)" entry function, with no way to pass it an
 * argument pointer directly. So the fiber array, the scheduler's own
 * context (where control returns after a swapcontext), and "which fiber
 * is currently running" are shared through file-scope state instead.
 */
static Fiber *gFibers;
static ucontext_t gSchedulerContext;
static int gStartIndex;

/*
 * Purpose:    Splits "total" bodies into "fiberCount" contiguous ranges as
 *             evenly as possible (identical logic to CMP's worker split).
 * Why:        Reused so the fixed range each fiber owns is decided the
 *             same way CMP decides each thread's range - the only
 *             difference between the models is how a range gets consumed.
 * Parameters: total, fiberCount, fiberId - as in ComputeWorkerRange.
 *             first, last - output: half-open range [first, last).
 * Returns:    Nothing.
 */
static void ComputeFiberRange(int total, int fiberCount, int fiberId, int *first, int *last)
{
    int base = total / fiberCount;
    int remainder = total % fiberCount;

    if (fiberId < remainder)
    {
        *first = fiberId * (base + 1);
        *last = *first + base + 1;
    }
    else
    {
        *first = remainder * (base + 1) + (fiberId - remainder) * base;
        *last = *first + base;
    }
}

/*
 * Purpose:    Entry point every fiber runs on. Processes its fixed range in
 *             quantum-sized chunks, yielding after every chunk regardless
 *             of whether more work remains.
 * Why:        This loop is the actual "fine-grained" behavior: it never
 *             asks "should I switch now?" - it always switches, every
 *             quantum, unconditionally.
 * Parameters: none (fiber identity comes from the file-scope gStartIndex,
 *             set by the scheduler right before each swapcontext into it).
 * Returns:    Nothing. When the range is exhausted the function returns,
 *             and uc_link automatically resumes the scheduler context.
 */
static void FiberEntry(void)
{
    Fiber *self = &gFibers[gStartIndex];
    self->Cursor = self->First;

    for (;;)
    {
        int chunkEnd = self->Cursor + self->QuantumBodies;
        if (chunkEnd > self->Last)
        {
            chunkEnd = self->Last;
        }

        NBodyComputeAcceleration(self->Sys, self->Cursor, chunkEnd);
        self->Cursor = chunkEnd;

        if (self->Cursor >= self->Last)
        {
            break;
        }

        swapcontext(&self->Context, &gSchedulerContext);
    }
}

/*
 * Purpose:    Runs the full FGMT schedule: creates "fiberCount" fibers,
 *             each owning a fixed range, then round-robins between them
 *             (one quantum each) until every fiber has finished its range.
 * Why:        This is the FGMT counterpart of NBodyComputeAccelerationParallel
 *             (CMP) and NBodyComputeAccelerationCgmt (CGMT): same physics,
 *             different scheduling policy.
 * Parameters: sys           - system to update.
 *             fiberCount    - number of cooperative fibers.
 *             quantumBodies - bodies processed per fiber per turn.
 * Returns:    Nothing.
 *
 * The "volatile" on the loop counters below is required, not decorative:
 * GCC warns "-Wclobbered" on local variables that are still live across a
 * swapcontext/getcontext call, because it cannot prove the value survives
 * the context switch through the compiler's own register allocation.
 * Marking them volatile forces GCC to always read/write them from memory,
 * which removes the warning and guarantees correctness across the switch.
 */
void NBodyComputeAccelerationFgmt(NBodySystem *sys, int fiberCount, int quantumBodies)
{
    gFibers = calloc((size_t)fiberCount, sizeof(Fiber));
    int remaining = 0;

    for (volatile int f = 0; f < fiberCount; f++)
    {
        int first, last;
        ComputeFiberRange(sys->Count, fiberCount, f, &first, &last);

        gFibers[f].Sys = sys;
        gFibers[f].First = first;
        gFibers[f].Last = last;
        gFibers[f].QuantumBodies = quantumBodies;
        gFibers[f].Finished = (first >= last) ? 1 : 0;

        if (!gFibers[f].Finished)
        {
            gFibers[f].Stack = malloc(FIBER_STACK_SIZE);
            getcontext(&gFibers[f].Context);
            gFibers[f].Context.uc_stack.ss_sp = gFibers[f].Stack;
            gFibers[f].Context.uc_stack.ss_size = FIBER_STACK_SIZE;
            gFibers[f].Context.uc_link = &gSchedulerContext;
            makecontext(&gFibers[f].Context, FiberEntry, 0);
            remaining++;
        }
    }

    while (remaining > 0)
    {
        for (volatile int f = 0; f < fiberCount; f++)
        {
            if (gFibers[f].Finished)
            {
                continue;
            }

            gStartIndex = f;
            swapcontext(&gSchedulerContext, &gFibers[f].Context);

            if (gFibers[f].Cursor >= gFibers[f].Last)
            {
                gFibers[f].Finished = 1;
                free(gFibers[f].Stack);
                remaining--;
            }
        }
    }

    free(gFibers);
}