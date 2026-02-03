#ifndef MY_NN_H
#define MY_NN_H
#include "my_matrix.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct nn_s nn_t;
typedef struct nn_cfg_s nn_cfg_t;

struct nn_cfg_s {
  mat_data_type rate;
  ui64 epochs;
  ui64 batch_size;
};

struct nn_s {
  ui64 count;
  matrix_t *ws;
  matrix_t *bs;
  matrix_t *as;
  ui64 *arch;
  ui64 arch_count;
};

#define INIT_NN ((nn_t){.count = 0, .ws = NULL, .bs = NULL, .as = NULL})

nn_t nn_init(mem_arena_t *, ui64 *, ui64, char *);
nn_cfg_t nn_cfg_init(mat_data_type, ui64, ui64);
void nn_finite_diff(mem_arena_t *, nn_t *, nn_t *, matrix_t *, mat_data_type);
matrix_t nn_forward(mem_arena_t *, nn_t *, matrix_t *);
nn_t nn_alloc_grads(mem_arena_t *, ui64 *, ui64, char *);
void nn_print(nn_t *nn);
void nn_zero(nn_t *);
void nn_train(mem_arena_t *, nn_t *, matrix_t *, matrix_t *, nn_cfg_t *);
#endif
