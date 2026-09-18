#ifndef MATRIX_H
#define MATRIX_H


#include <stddef.h>

typedef struct {
    int n;          /* dimension de la matriz (n x n)   */
    double *data;   /* almacenamiento row-major, n*n doubles */
} Matrix;

/* Acceso a elemento (row-major) */
#define MAT_AT(m, i, j) ((m)->data[(size_t)(i) * (size_t)(m)->n + (size_t)(j)])

/* ---- ciclo de vida ------------------------------------------------- */
Matrix *mat_create(int n);
void    mat_free(Matrix *m);
void    mat_init_random(Matrix *m, unsigned seed);
void    mat_zero(Matrix *m);

/* ---- multiplicacion por bloques ------------------------------------
 * nb = n / block_size debe ser exacto (validar con mat_check_dims). */
typedef struct {
    const Matrix *A;
    const Matrix *B;
    Matrix       *C;
    int           block_size;
    int           nb;
} MatMulCtx;

int mat_check_dims(int n, int block_size); /* 1 si n % block_size == 0 */

/* C[bi][bj] += A[bi][bk] * B[bk][bj]  (unidad minima de trabajo) */
void mat_block_partial(const MatMulCtx *ctx, int bi, int bj, int bk);

/* C[bi][bj] = suma_bk A[bi][bk] * B[bk][bj]  (un bloque de salida completo) */
void mat_block_full(const MatMulCtx *ctx, int bi, int bj);

/* Referencia secuencial (un solo hilo, sin ningun esquema paralelo).
 * Esta es la base (baseline) contra la que se calcula speedup/eficiencia
 * para los otros cuatro modelos, tal como exige el enunciado ("usen
 * como base el problema sin esquema de hilos"). */
void mat_multiply_sequential(const Matrix *A, const Matrix *B, Matrix *C, int block_size);

/* ---- verificacion y utilidades compartidas -------------------------- */

/* Checksum simple (suma de todos los elementos) para verificar que las
 * cinco implementaciones producen el mismo resultado numerico. */
double mat_checksum(const Matrix *m);

/* Reloj monotonico de alta resolucion, en segundos. */
double now_seconds(void);

/* Salida uniforme en CSV, igual para los cinco programas, para que el
 * post-procesamiento (calculo de speedup, eficiencia, IC 95%, graficas)
 * pueda tratarlos de forma identica sin importar el modelo. */
void print_csv_header(void);
void print_csv_row(const char *model, int n, int block_size, int workers,
                    double time_sec, double checksum);

/* Particiona los indices [0, total) en `workers` fragmentos contiguos y
 * balanceados, y devuelve el rango [start, end) que le corresponde a
 * worker_id. La usan coarseGrained/smt/cmp para repartir los bloques de
 * salida de la misma manera; solo difieren en COMO planifican esos
 * fragmentos sobre hilos/nucleos. */
void partition_range(int total, int workers, int worker_id, int *start, int *end);

/* Numero de nucleos logicos en linea (sysconf). */
long get_num_cores(void);

#endif /* MATRIX_H */
