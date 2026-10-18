

#include "matrix.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <N> <block_size> [seed]\n", argv[0]);
        return 1;
    }
    int n          = atoi(argv[1]);
    int block_size = atoi(argv[2]);
    unsigned seed  = (argc > 3) ? (unsigned)atoi(argv[3]) : 42u;

    if (!mat_check_dims(n, block_size)) {
        fprintf(stderr, "Dimensiones invalidas: N debe ser multiplo de block_size\n");
        return 1;
    }

    Matrix *A = mat_create(n);
    Matrix *B = mat_create(n);
    Matrix *C = mat_create(n);
    mat_init_random(A, seed);
    mat_init_random(B, seed + 1);

    double t0 = now_seconds();
    mat_multiply_sequential(A, B, C, block_size);
    double t1 = now_seconds();

    print_csv_header();
    print_csv_row("sequential", n, block_size, 1, t1 - t0, mat_checksum(C));

    mat_free(A); mat_free(B); mat_free(C);
    return 0;
}
