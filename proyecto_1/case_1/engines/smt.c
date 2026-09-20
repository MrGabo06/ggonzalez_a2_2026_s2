#include "matrix.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct {
    MatMulCtx ctx;
    int start, end;
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
    if (argc < 5) {
        fprintf(stderr, "Uso: %s <N> <block_size> <nucleos_fisicos> <factor_smt> [seed]\n", argv[0]);
        return 1;
    }
    int n              = atoi(argv[1]);
    int block_size     = atoi(argv[2]);
    int physical_cores = atoi(argv[3]);
    int smt_factor     = atoi(argv[4]);
    unsigned seed      = (argc > 5) ? (unsigned)atoi(argv[5]) : 42u;

    if (!mat_check_dims(n, block_size) || physical_cores < 1 || smt_factor < 1) {
        fprintf(stderr, "Dimensiones invalidas\n");
        return 1;
    }

    long available = get_num_cores();
    if (physical_cores > available) {
        fprintf(stderr,
                "Advertencia: se pidieron %d nucleos fisicos pero solo hay %ld en linea\n",
                physical_cores, available);
    }

    int nthreads = physical_cores * smt_factor;

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

    double t0 = now_seconds();

    for (int t = 0; t < nthreads; t++) {
        args[t].ctx = ctx;
        partition_range(total_blocks, nthreads, t, &args[t].start, &args[t].end);
        pthread_create(&tids[t], NULL, worker, &args[t]);

        /* El hilo logico t se fija al nucleo fisico (t % physical_cores):
         * asi, `smt_factor` hilos logicos comparten cada nucleo fisico,
         * sobre-suscribiendolo. */
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(t % physical_cores, &cpuset);
        pthread_setaffinity_np(tids[t], sizeof(cpu_set_t), &cpuset);
    }
    for (int t = 0; t < nthreads; t++) {
        pthread_join(tids[t], NULL);
    }

    double t1 = now_seconds();

    print_csv_header();
    print_csv_row("smt", n, block_size, nthreads, t1 - t0, mat_checksum(C));

    free(tids); free(args);
    mat_free(A); mat_free(B); mat_free(C);
    return 0;
}
