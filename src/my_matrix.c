#include "../include/my_matrix.h"

matrix_t mat_init(mem_arena_t *arena, const ui64 rows, const ui64 cols,
                  char *errbuf) {
  assert(arena != NULL);
  mat_data_type *tmp =
      arena_push_arr(*arena, mat_data_type, cols * rows, true, errbuf);
  if (tmp == NULL) {
    return INIT_MATRIX;
  }
  return ((matrix_t){.data = tmp, .cols = cols, .rows = rows});
}

void mat_rand(matrix_t *mat) {
  assert(mat != NULL);
  for (ui64 i = 0; i < mat->rows; i++) {
    for (ui64 j = 0; j < mat->cols; j++) {
      MAT_AT(mat, i, j) = (mat_data_type)rand() / RAND_MAX * 2 - 1;
    }
  }
}

mat_data_type mat_sum(matrix_t *mat) {
  mat_data_type sum = 0.0f;
  for (ui64 i = 0; i < mat->rows; i++) {
    for (ui64 j = 0; j < mat->cols; j++) {
      sum += MAT_AT(mat, i, j);
    }
  }
  return sum;
}

void mat_add(matrix_t *dst, const matrix_t *_a, const matrix_t *_b) {
  assert(dst != NULL && _a != NULL && _b != NULL);
  assert(dst->data != NULL && _a->data != NULL && _b->data != NULL);
  assert(_a->cols == _b->cols);
  assert(_a->rows == _b->rows);

  for (ui64 i = 0; i < _a->rows; i++) {
    for (ui64 j = 0; j < _a->cols; j++) {
      MAT_AT(dst, i, j) = MAT_AT(_a, i, j) + MAT_AT(_b, i, j);
    }
  }
}

void mat_mult(matrix_t *dst, const matrix_t *_a, const matrix_t *_b) {
  assert(dst != _a && dst != _b);
  assert(dst != NULL && _b != NULL && _a != NULL);
  assert(dst->data != NULL && _a->data != NULL && _b->data != NULL);
  assert(_b->rows == _a->cols);
  assert(dst->cols == _b->cols && dst->rows == _a->rows);

  mat_zero(dst);
  for (ui64 i = 0; i < dst->rows; i++) {
    for (ui64 k = 0; k < _a->cols; k++) {
      mat_data_type r = MAT_AT(_a, i, k);
      for (int j = 0; j < dst->cols; j++) {
        MAT_AT(dst, i, j) += r * MAT_AT(_b, k, j);
      }
    }
  }
}

void mat_mult_scal(matrix_t *dst, matrix_t *a, mat_data_type scale) {
  assert(dst != NULL && dst->data != NULL);
  assert(a != NULL && a->data != NULL);
  assert(a->rows == dst->rows && a->cols == dst->cols);
  for (ui64 i = 0; i < dst->rows; i++) {
    for (ui64 j = 0; j < dst->cols; j++) {
      MAT_AT(dst, i, j) = MAT_AT(a, i, j) * scale;
    }
  }
}

void mat_print(matrix_t *mat) {
  printf("[\n");
  for (ui64 i = 0; i < mat->rows; i++) {
    for (ui64 j = 0; j < mat->cols; j++) {
      printf(MAT_FMT, MAT_AT(mat, i, j));
    }
    printf("\n");
  }
  printf("]\n");
}

matrix_t mat_row_view(matrix_t *mat, ui64 rows, ui64 cols) {
  if (mat == NULL || mat->cols < cols || mat->rows < rows) {
    return INIT_MATRIX;
  }
  return ((matrix_t){
      .data = &mat->data[rows * mat->cols], .cols = cols, .rows = 1});
}

void mat_transpose(matrix_t *dst, matrix_t *mat) {
  assert(mat != NULL && dst != NULL && dst->data != NULL && mat->data != NULL);
  for (ui64 i = 0; i < mat->cols; i++) {
    for (ui64 j = 0; j < mat->rows; j++) {
      MAT_AT(dst, i, j) = MAT_AT(mat, j, i);
    }
  }
}

void mat_scale_add(matrix_t *dst, const matrix_t *_a, const matrix_t *_b,
                   mat_data_type scale) {
  assert(dst != NULL && _a != NULL && _b != NULL);
  assert(dst->data != NULL && _a->data != NULL && _b != NULL);
  assert(_a->cols == _b->cols && _a->rows == _b->rows);
  assert(dst->rows == _a->rows && dst->cols == _a->cols);
  for (ui64 i = 0; i < _a->rows; i++) {
    for (ui64 j = 0; j < _a->cols; j++) {
      MAT_AT(dst, i, j) = MAT_AT(_a, i, j) + MAT_AT(_b, i, j) * scale;
    }
  }
}

void mat_copy(matrix_t *dst, matrix_t *a) {
  assert(dst != NULL && a != NULL);
  assert(dst->data != NULL && a->data != NULL);
  assert(a->cols == dst->cols && a->rows == dst->rows);
  for (ui64 i = 0; i < a->rows; i++) {
    for (ui64 j = 0; j < a->cols; j++) {
      MAT_AT(dst, i, j) = MAT_AT(a, i, j);
    }
  }
}

void mat_zero(matrix_t *dst) {
  assert(dst != NULL && dst->data != NULL);
  memset(dst->data, 0, sizeof(mat_data_type) * dst->cols * dst->rows);
}
