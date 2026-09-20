
#include "matrix.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct {
    MatMulCtx ctx;
    int start, end; /* rango [start,end) sobre indices bi*nb+bj aplanados */
} ThreadArg;

static void *worker(void *arg) {
    ThreadArg *ta = (ThreadArg *)arg;
    int nb = ta->ctx.nb;
    for (int idx = ta->start; idx < ta->end; idx++) {
        int bi = idx / nb;
        int bj = idx % nb;
        mat_block_full(&ta->ctx, bi, bj);
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <N> <block_size> <hilos> [seed]\n", argv[0]);
        return 1;
    }
    int n          = atoi(argv[1]);
    int block_size = atoi(argv[2]);
    int nthreads   = atoi(argv[3]);
    unsigned seed  = (argc > 4) ? (unsigned)atoi(argv[4]) : 42u;

    if (!mat_check_dims(n, block_size) || nthreads < 1) {
        fprintf(stderr, "Dimensiones invalidas: N multiplo de block_size, hilos >= 1\n");
        return 1;
    }

    Matrix *A = mat_create(n);
    Matrix *B = mat_create(n);
    Matrix *C = mat_create(n);
    mat_init_random(A, seed);
    mat_init_random(B, seed + 1);
    mat_zero(C);

    MatMulCtx ctx = { A, B, C, block_size, n / block_size };
    int total_blocks = ctx.nb * ctx.nb;

    pthread_t  *tids = malloc(sizeof(pthread_t) * (size_t)nthreads);
    ThreadArg  *args = malloc(sizeof(ThreadArg) * (size_t)nthreads);
    if (!tids || !args) { perror("malloc"); return 1; }

    /* Confinar cada hilo al nucleo fisico 0: la condicion que define
     * este modelo es que NO hay paralelismo real de hardware, solo
     * multiplexacion en el tiempo de un unico nucleo. */
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);

    double t0 = now_seconds();

    for (int t = 0; t < nthreads; t++) {
        args[t].ctx = ctx;
        partition_range(total_blocks, nthreads, t, &args[t].start, &args[t].end);
        pthread_create(&tids[t], NULL, worker, &args[t]);
        pthread_setaffinity_np(tids[t], sizeof(cpu_set_t), &cpuset);
    }
    for (int t = 0; t < nthreads; t++) {
        pthread_join(tids[t], NULL);
    }

    double t1 = now_seconds();

    print_csv_header();
    print_csv_row("coarse_grained", n, block_size, nthreads, t1 - t0, mat_checksum(C));

    free(tids); free(args);
    mat_free(A); mat_free(B); mat_free(C);
    return 0;
}
