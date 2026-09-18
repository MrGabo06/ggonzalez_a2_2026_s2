#define _POSIX_C_SOURCE 199309L

#include "matrix.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

Matrix *mat_create(int n) {
    Matrix *m = malloc(sizeof(Matrix));
    if (!m) { perror("malloc"); exit(1); }
    m->n = n;
    m->data = malloc(sizeof(double) * (size_t)n * (size_t)n);
    if (!m->data) { perror("malloc"); exit(1); }
    return m;
}

void mat_free(Matrix *m) {
    if (!m) return;
    free(m->data);
    free(m);
}

void mat_init_random(Matrix *m, unsigned seed) {
    /* srand/rand no son thread-safe si se llaman concurrentemente, pero
     * aqui solo se usan para inicializar A y B una vez, de forma
     * secuencial, antes de arrancar cualquier hilo -> no hay condicion
     * de carrera real en ningun programa de este proyecto. */
    srand(seed);
    size_t total = (size_t)m->n * (size_t)m->n;
    for (size_t i = 0; i < total; i++) {
        m->data[i] = (double)(rand() % 1000) / 10.0; /* valores 0.0 - 99.9 */
    }
}

void mat_zero(Matrix *m) {
    size_t total = (size_t)m->n * (size_t)m->n;
    for (size_t i = 0; i < total; i++) m->data[i] = 0.0;
}

int mat_check_dims(int n, int block_size) {
    return (n > 0) && (block_size > 0) && (n % block_size == 0);
}

void mat_block_partial(const MatMulCtx *ctx, int bi, int bj, int bk) {
    int bs = ctx->block_size;
    int i0 = bi * bs, j0 = bj * bs, k0 = bk * bs;
    for (int i = 0; i < bs; i++) {
        for (int j = 0; j < bs; j++) {
            double sum = 0.0;
            for (int k = 0; k < bs; k++) {
                sum += MAT_AT(ctx->A, i0 + i, k0 + k) * MAT_AT(ctx->B, k0 + k, j0 + j);
            }
            /* Distintos (bi,bj) nunca escriben la misma celda de C, y
             * distintos bk para el MISMO (bi,bj) siempre se ejecutan
             * secuencialmente dentro de un mismo hilo en todos nuestros
             * modelos (mat_block_full los recorre en orden, y
             * fineGrained.c nunca reparte el mismo (bi,bj) entre dos
             * hilos virtuales a la vez) -> esta suma en sitio (+=) nunca
             * requiere proteccion adicional (mutex/atomic). */
            MAT_AT(ctx->C, i0 + i, j0 + j) += sum;
        }
    }
}

void mat_block_full(const MatMulCtx *ctx, int bi, int bj) {
    for (int bk = 0; bk < ctx->nb; bk++) {
        mat_block_partial(ctx, bi, bj, bk);
    }
}

void mat_multiply_sequential(const Matrix *A, const Matrix *B, Matrix *C, int block_size) {
    mat_zero(C);
    MatMulCtx ctx = { A, B, C, block_size, A->n / block_size };
    for (int bi = 0; bi < ctx.nb; bi++) {
        for (int bj = 0; bj < ctx.nb; bj++) {
            mat_block_full(&ctx, bi, bj);
        }
    }
}

double mat_checksum(const Matrix *m) {
    double sum = 0.0;
    size_t total = (size_t)m->n * (size_t)m->n;
    for (size_t i = 0; i < total; i++) sum += m->data[i];
    return sum;
}

double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

void print_csv_header(void) {
    printf("model,n,block_size,workers,time_sec,checksum\n");
}

void print_csv_row(const char *model, int n, int block_size, int workers,
                    double time_sec, double checksum) {
    printf("%s,%d,%d,%d,%.9f,%.6f\n", model, n, block_size, workers, time_sec, checksum);
}

void partition_range(int total, int workers, int worker_id, int *start, int *end) {
    int base = total / workers;
    int rem  = total % workers;
    /* los primeros `rem` workers reciben una unidad extra para que la
     * particion quede lo mas balanceada posible cuando total no es
     * multiplo exacto de workers. */
    int s = worker_id * base + (worker_id < rem ? worker_id : rem);
    int extra = (worker_id < rem) ? 1 : 0;
    *start = s;
    *end   = s + base + extra;
}

long get_num_cores(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return (n < 1) ? 1 : n;
}
