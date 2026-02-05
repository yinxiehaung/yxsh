#ifndef MY_MATRIX
#define MY_MATRIX
#include "my_arena.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef int64_t i64;
typedef uint32_t ui32;
typedef uint8_t ui8;
typedef double f64;
typedef float f32;
typedef struct matrix_s matrix_t;

#define USE_FLOAT
#ifdef USE_FLOAT
typedef f32 mat_data_type;
#define MAT_POW(x, y) powf((x), (y))
#define MAT_EXP(x) expf(x)
#define MAT_SQRT(x) sqrtf((x))
#define MAT_FMT "%.2f "
#else
typedef f64 mat_data_type;
#define MAT_POW(x, y) pow((x), (y))
#define MAT_EXP(x) exp(x)
#define MAT_SQRT(x) sqrt((x))
#define MAT_FMT "%.2lf "
#endif

struct matrix_s {
  mat_data_type *data;
  ui64 cols;
  ui64 rows;
};

#define INIT_MATRIX ((matrix_t){.data = NULL, .cols = 0, .rows = 0})

#define MAT_AT(m, i, j)                                                        \
  ((((mat_data_type *)((m)->data))[(i) * (m)->cols + (j)]))

matrix_t mat_init(mem_arena_t *, const ui64 cols, const ui64 rows,
                  char *errbuf);
void mat_rand(matrix_t *);
mat_data_type mat_sum(matrix_t *);
void mat_add(matrix_t *, const matrix_t *, const matrix_t *);
void mat_scale_add(matrix_t *, const matrix_t *, const matrix_t *,
                   mat_data_type);
void mat_mult(matrix_t *, const matrix_t *, const matrix_t *);
void mat_mult_scal(matrix_t *, matrix_t *, mat_data_type);
void mat_transpose(matrix_t *, matrix_t *);
void mat_copy(matrix_t *, matrix_t *);
matrix_t mat_slice(matrix_t *, ui64, ui64 count);
void mat_zero(matrix_t *);
void mat_print(matrix_t *);

matrix_t mat_row_view(matrix_t *, size_t, size_t);
void mat_sum_rows(matrix_t *, matrix_t *);
#endif
