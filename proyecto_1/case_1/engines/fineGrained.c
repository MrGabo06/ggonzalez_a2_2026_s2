
#define _POSIX_C_SOURCE 199309L

#include "matrix.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct { int bi, bj, bk; } Task;

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <N> <block_size> <hilos_virtuales> [seed]\n", argv[0]);
        return 1;
    }
    int n          = atoi(argv[1]);
    int block_size = atoi(argv[2]);
    int vthreads   = atoi(argv[3]);
    unsigned seed  = (argc > 4) ? (unsigned)atoi(argv[4]) : 42u;

    if (!mat_check_dims(n, block_size) || vthreads < 1) {
        fprintf(stderr, "Dimensiones invalidas: N multiplo de block_size, hilos_virtuales >= 1\n");
        return 1;
    }

    Matrix *A = mat_create(n);
    Matrix *B = mat_create(n);
    Matrix *C = mat_create(n);
    mat_init_random(A, seed);
    mat_init_random(B, seed + 1);
    mat_zero(C);

    MatMulCtx ctx = { A, B, C, block_size, n / block_size };
    int nb = ctx.nb;
    int total_tasks = nb * nb * nb; /* una por cada tripla (bi,bj,bk) */

    Task *tasks = malloc(sizeof(Task) * (size_t)total_tasks);
    if (!tasks) { perror("malloc"); return 1; }
    int idx = 0;
    for (int bi = 0; bi < nb; bi++)
        for (int bj = 0; bj < nb; bj++)
            for (int bk = 0; bk < nb; bk++)
                tasks[idx++] = (Task){ bi, bj, bk };

    /* queue[v] contiene los indices (dentro de `tasks`) asignados al
     * hilo virtual v, en orden de ejecucion. qhead[v] es el siguiente
     * pendiente; qlen[v] el total asignado a v. */
    int **queue = malloc(sizeof(int *) * (size_t)vthreads);
    int  *qlen  = calloc((size_t)vthreads, sizeof(int));
    int  *qhead = calloc((size_t)vthreads, sizeof(int));
    if (!queue || !qlen || !qhead) { perror("malloc"); return 1; }

    for (int v = 0; v < vthreads; v++) {
        queue[v] = malloc(sizeof(int) * (size_t)(total_tasks / vthreads + 1));
        if (!queue[v]) { perror("malloc"); return 1; }
    }
    for (int t = 0; t < total_tasks; t++) {
        int v = t % vthreads;
        queue[v][qlen[v]++] = t;
    }

    double t0 = now_seconds();

    /* Planificador cooperativo round-robin: exactamente un quantum (una
     * tarea) por hilo virtual por pasada, en orden fijo, saltando a los
     * hilos virtuales que ya terminaron. */
    int remaining = total_tasks;
    while (remaining > 0) {
        for (int v = 0; v < vthreads; v++) {
            if (qhead[v] < qlen[v]) {
                Task tk = tasks[queue[v][qhead[v]]];
                mat_block_partial(&ctx, tk.bi, tk.bj, tk.bk);
                qhead[v]++;
                remaining--;
            }
        }
    }

    double t1 = now_seconds();

    print_csv_header();
    print_csv_row("fine_grained", n, block_size, vthreads, t1 - t0, mat_checksum(C));

    for (int v = 0; v < vthreads; v++) free(queue[v]);
    free(queue); free(qlen); free(qhead); free(tasks);
    mat_free(A); mat_free(B); mat_free(C);
    return 0;
}
