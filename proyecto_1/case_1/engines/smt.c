#include "matrix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_SIB 8   /* maximo de CPUs logicas hermanas por nucleo fisico */

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

/* Parsea una lista de CPUs estilo sysfs ("0,4", "0-1", "2,6\n") y escribe
 * los indices en out (ascendente). Devuelve cuantos escribio. */
static int parse_cpu_list(const char *s, int *out, int max) {
    int n = 0;
    const char *p = s;
    while (*p && n < max) {
        char *end;
        long a = strtol(p, &end, 10);
        if (end == p) break;
        long b = a;
        if (*end == '-') {
            p = end + 1;
            b = strtol(p, &end, 10);
        }
        for (long c = a; c <= b && n < max; c++) out[n++] = (int)c;
        p = end;
        if (*p == ',') p++;
        else break;
    }
    return n;
}

/* Lee las CPUs logicas hermanas (mismo nucleo fisico) de `cpu`.
 * Devuelve 0 si no hay informacion (CPU offline o sin sysfs). */
static int read_siblings(int cpu, int *out, int max) {
    char path[160];
    snprintf(path, sizeof path,
             "/sys/devices/system/cpu/cpu%d/topology/thread_siblings_list", cpu);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char buf[256];
    if (!fgets(buf, sizeof buf, f)) { fclose(f); return 0; }
    fclose(f);
    return parse_cpu_list(buf, out, max);
}


static int build_core_table(int physical_cores, int *table, int *nsib) {
    long ncpu = sysconf(_SC_NPROCESSORS_CONF);
    int found = 0;
    for (int cpu = 0; cpu < ncpu && found < physical_cores; cpu++) {
        int sib[MAX_SIB];
        int k = read_siblings(cpu, sib, MAX_SIB);
        if (k == 0) continue;          /* CPU offline o sin topologia */
        if (sib[0] != cpu) continue;   /* ya contada via su hermano de menor indice */
        for (int i = 0; i < k; i++) table[found * MAX_SIB + i] = sib[i];
        nsib[found] = k;
        found++;
    }
    return found == physical_cores;
}

int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr,
                "Uso: %s <N> <block_size> <nucleos_fisicos> <factor_smt> [seed] [cores|siblings]\n"
                "  cores    (default): el hilo t se fija a la CPU logica t %% nucleos_fisicos.\n"
                "  siblings : los hilos de un mismo nucleo se reparten entre sus CPUs logicas\n"
                "             hermanas (Hyper-Threading real, si esta activado en el BIOS).\n",
                argv[0]);
        return 1;
    }
    int n              = atoi(argv[1]);
    int block_size     = atoi(argv[2]);
    int physical_cores = atoi(argv[3]);
    int smt_factor     = atoi(argv[4]);
    unsigned seed      = (argc > 5) ? (unsigned)atoi(argv[5]) : 42u;
    const char *pin    = (argc > 6) ? argv[6] : "cores";

    if (!mat_check_dims(n, block_size) || physical_cores < 1 || smt_factor < 1) {
        fprintf(stderr, "Dimensiones invalidas\n");
        return 1;
    }
    int use_siblings;
    if (strcmp(pin, "cores") == 0)         use_siblings = 0;
    else if (strcmp(pin, "siblings") == 0) use_siblings = 1;
    else {
        fprintf(stderr, "Modo de pinning invalido '%s' (use cores o siblings)\n", pin);
        return 1;
    }

    long available = get_num_cores();
    if (physical_cores > available) {
        fprintf(stderr,
                "Advertencia: se pidieron %d nucleos fisicos pero solo hay %ld en linea\n",
                physical_cores, available);
    }

    int nthreads = physical_cores * smt_factor;

    /* Tabla nucleo fisico -> CPUs logicas hermanas (solo modo siblings). */
    int *core_table = NULL, *core_nsib = NULL;
    if (use_siblings) {
        core_table = calloc((size_t)physical_cores * MAX_SIB, sizeof(int));
        core_nsib  = calloc((size_t)physical_cores, sizeof(int));
        if (!core_table || !core_nsib) { perror("calloc"); return 1; }
        if (!build_core_table(physical_cores, core_table, core_nsib)) {
            fprintf(stderr,
                    "Advertencia: no se pudo leer la topologia de %d nucleos fisicos; "
                    "se usa el modo 'cores'\n", physical_cores);
            use_siblings = 0;
        }
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

    double t0 = now_seconds();

    for (int t = 0; t < nthreads; t++) {
        args[t].ctx = ctx;
        partition_range(total_blocks, nthreads, t, &args[t].start, &args[t].end);
        pthread_create(&tids[t], NULL, worker, &args[t]);


        int cpu;
        if (use_siblings) {
            int core = t % physical_cores;
            int slot = (t / physical_cores) % core_nsib[core];
            cpu = core_table[core * MAX_SIB + slot];
        } else {
            cpu = t % physical_cores;
        }
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu, &cpuset);
        pthread_setaffinity_np(tids[t], sizeof(cpu_set_t), &cpuset);
    }
    for (int t = 0; t < nthreads; t++) {
        pthread_join(tids[t], NULL);
    }

    double t1 = now_seconds();

    print_csv_header();
    print_csv_row("smt", n, block_size, nthreads, t1 - t0, mat_checksum(C));

    free(tids); free(args);
    free(core_table); free(core_nsib);
    mat_free(A); mat_free(B); mat_free(C);
    return 0;
}