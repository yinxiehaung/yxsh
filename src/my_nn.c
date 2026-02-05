#include "../include/my_nn.h"

mat_data_type sigmoid(mat_data_type x) { return 1.0 / (1.0 + MAT_EXP(-x)); }

static void mat_act(matrix_t *mat) {
  assert(mat != NULL);
  for (int i = 0; i < mat->rows; i++) {
    for (int j = 0; j < mat->cols; j++) {
      MAT_AT(mat, i, j) = sigmoid(MAT_AT(mat, i, j));
    }
  }
}
static void mat_add_bias(matrix_t *mat, matrix_t *bias) {
  assert(mat != NULL && bias != NULL);
  assert(bias->rows == 1 && bias->cols == mat->cols);

  for (ui64 r = 0; r < mat->rows; r++) {
    for (ui64 c = 0; c < mat->cols; c++) {
      MAT_AT(mat, r, c) += MAT_AT(bias, 0, c);
    }
  }
}

static ssize_t nn_alloc(mem_arena_t *arena, nn_t *nn, ui64 *arch,
                        ui64 arch_count, char *errbuf) {
  assert(nn != NULL && arena != NULL);
  mem_tmp_arena_t check_point = arena_begin_tmp(arena);
  nn->count = arch_count - 1;
  nn->arch = arena_push_arr(*arena, ui64, arch_count, true, errbuf);
  nn->ws = arena_push_arr(*arena, matrix_t, nn->count, true, errbuf);
  nn->bs = arena_push_arr(*arena, matrix_t, nn->count, true, errbuf);
  nn->as = arena_push_arr(*arena, matrix_t, nn->count + 1, true, errbuf);
  if (nn->ws == NULL || nn->bs == NULL || nn->as == NULL || nn->arch == NULL) {
    goto error;
  }
  for (int i = 0; i < arch_count; i++) {
    nn->arch[i] = arch[i];
  }
  nn->arch_count = arch_count;
  for (ui64 i = 0; i < arch_count - 1; i++) {
    ui64 rows = arch[i];
    ui64 cols = arch[i + 1];
    nn->ws[i] = mat_init(arena, rows, cols, errbuf);
    nn->bs[i] = mat_init(arena, 1, cols, errbuf);
    if (nn->ws[i].data == NULL || nn->bs[i].data == NULL) {
      goto error;
    }
  }
  return 0;
error:
  arena_end_tmp(&check_point);
  return -1;
}

nn_cfg_t nn_cfg_init(mat_data_type rate, ui64 batch_size, ui64 epochs) {
  return ((nn_cfg_t){.rate = rate, .batch_size = batch_size, .epochs = epochs});
}

nn_t nn_alloc_grads(mem_arena_t *arena, ui64 *arch, ui64 arch_count,
                    char *errbuf) {
  assert(arena != NULL && arch != NULL);
  nn_t nn = INIT_NN;
  ssize_t ok = nn_alloc(arena, &nn, arch, arch_count, errbuf);
  if (ok == -1) {
    return INIT_NN;
  }
  return nn;
}

nn_t nn_init(mem_arena_t *arena, ui64 *arch, ui64 arch_count, char *errbuf) {
  assert(arch_count > 1 && arch != NULL);
  assert(arena != NULL);
  nn_t nn = INIT_NN;
  ssize_t ok = nn_alloc(arena, &nn, arch, arch_count, errbuf);
  if (ok == -1) {
    return INIT_NN;
  }
  for (int i = 0; i < arch_count - 1; i++) {
    mat_rand(&nn.ws[i]);
    mat_rand(&nn.bs[i]);
  }
  return nn;
}

matrix_t nn_forward(mem_arena_t *arena, nn_t *nn, matrix_t *x) {
  matrix_t curr;
  nn->as[0] = *x;
  curr = nn->as[0];
  for (ui64 i = 0; i < nn->count; i++) {
    ui64 batch_size = curr.rows;
    ui64 next_cols = nn->ws[i].cols;
    nn->as[i + 1] = mat_init(arena, batch_size, next_cols, NULL);
    mat_mult(&nn->as[i + 1], &curr, &nn->ws[i]); // 2 * 2 1 * 2
    mat_add_bias(&nn->as[i + 1], &nn->bs[i]);
    mat_act(&nn->as[i + 1]);
    curr = nn->as[i + 1];
  }
  return curr;
}

mat_data_type nn_cost(mem_arena_t *arena, nn_t *nn, matrix_t *train) {
  mem_tmp_arena_t scope = arena_begin_tmp(arena);
  mat_data_type result = 0.0f;
  ui64 n = train->rows;
  for (ui64 i = 0; i < n; i++) {
    matrix_t x = mat_row_view(train, i, nn->ws[0].cols);
    matrix_t y = nn_forward(scope.arena, nn, &x);
    for (ui64 j = 0; j < y.cols; j++) {
      mat_data_type d = MAT_AT(train, i, j + nn->ws[0].cols) - MAT_AT(&y, 0, j);
      result += d * d;
    }
  }
  arena_end_tmp(&scope);
  return result / n;
}

void nn_finite_diff(mem_arena_t *arena, nn_t *nn, nn_t *grads, matrix_t *train,
                    mat_data_type esp) {
  assert(nn != NULL && grads != NULL && train != NULL);
  mat_data_type c = nn_cost(arena, nn, train);
  for (ui64 i = 0; i < nn->count; i++) {
    for (ui64 j = 0; j < nn->ws[i].rows; j++) {
      for (ui64 k = 0; k < nn->ws[i].cols; k++) {
        mat_data_type saved = MAT_AT(&nn->ws[i], j, k);
        MAT_AT(&nn->ws[i], j, k) += esp;
        MAT_AT(&grads->ws[i], j, k) = ((nn_cost(arena, nn, train) - c) / esp);
        MAT_AT(&nn->ws[i], j, k) = saved;
      }
    }
    for (ui64 j = 0; j < nn->bs[i].cols; j++) {
      mat_data_type saved = MAT_AT(&nn->bs[i], 0, j);
      MAT_AT(&nn->bs[i], 0, j) += esp;
      MAT_AT(&grads->bs[i], 0, j) = ((nn_cost(arena, nn, train) - c) / esp);
      MAT_AT(&nn->bs[i], 0, j) = saved;
    }
  }
}
// #######################################################
// The Cost function: C(i) = (1/n) * sum((as_i+1 - y)^2)
// #######################################################
// as[i] = sigmoid(w_i*as_(i-1) + b_i), i = 1 = input layer
// let as[i] is a_i, a[1] = x, a[n+1] is output layer
// #######################################################
// sigmoid'(x) = sigmoid(x) * (1-sigmoid(x))
// sigmoid'(g(x)) = sigmoid(g(x)) * (1-sigmoid(g(x))) * g'(x)
// #######################################################
// notice: i is layer
// pC/pw_1  = pa_2/pw_1 * pC/pa_2
//          = sigmoid(w_1*x + b_1) * (1-sigmoid(w_1*x + b_1))
//            * x * pC/pa_2             ;(x is from chain rule)
//          = a_2 * (1 - a_2) * x * pC/pa_2
// pC/pw_2  = pa_3/pw_2 * pC/pa_3       ;pa_3 = sigmoid(w_2 * a_2 + b_2)
//          = sigmoid(w_2*a_2 + b_1) * (1-sigmoid(w_2*a_2 + b_1))
//            * a_2 * pC/pa_2
//          = a_3 * (1-a_3) * a_2 * pC/pa_2
// pC/pw_3  = pa_4/pw_3 * pC/pa_4
// .
// .
// .
// -> pC/pw_n = pa_n+1/pw_n * pC/pa_n+1
//            = a_n+1 * (1-a_n+1) * a_n * pC/pa_n+1 ... (1)
// #######################################################
// pC/pa_2 = pa_3/pa_2 * pC/pa_3
//         = sigmoid(w_2*a_2 + b_2) * (1-sigmoid(w_2 * a_2 + b_2))
//             * w_2 * pC/pa_3
//         = a_3 * (1 - a_3) * w_2 * pC/pa_3
// pC/pa_3 = pa_4/pa_3 * pC/pa_3
//         = sigmoid(w_3*a_3 + b_3) * (1-sigmoid(w_3 * a_3 + b_3))
//         = a_4 * (1 - a_4) * w_3 * pC/pa_4
// .
// .
// .
// -> pC/pa_n = pa_3/pa_2 * pC/pa_3
//            = a_n+1 * (1-a_n+1) * w_n * pC/pa_n+1 ... (2)
// #######################################################
// by (1) and (2)
// pC/pw_n = pa_n+1/pw_n * pC/pa_n+1
//         = a_n+1 * (1-a_n+1) * a_n * pC/a_n+1
//         = a_n+1 * (1-a_n+1) * a_n * a_n+2 * (1-a_n+2) * w_n+1 * pC/pa_n+2
//         let f(n) = a_n+1 * (1-a_n+1)
//         = (f(n) * a_n) * (f(n+1) * w_n+1) * pC/(pa_n+2)
//         = (f(n) * a_n) * (f(n+1) * w_n+1) * ....
// #######################################################
// if reverse (begine output layer), set 10 is output layer
// C(10) = 1/n * layer sum of(a_10 - out)^2
// pC/pa_10 = 1/n layer sum of(a_10 - out)^2
//          = 2/n layer sum of((a_10 - out)
// pC/pw_9 = pa_10/pw_9 * pC/pa_10
//         = a_10 * (1-a_10) * a_9 * pC/pa_10 -> we know this
// pC/pw_8 = pa_9/pw_8 * pC/pa_9 -> we kown this
// so that O(n), n is layer
// #######################################################
void nn_backprop(mem_arena_t *arena, nn_t *nn, nn_t *grads, matrix_t *target) {
  assert(nn != NULL && grads != NULL && target != NULL && arena != NULL);
  assert(nn->as != NULL && nn->bs != NULL && nn->ws != NULL);
  assert(grads->as != NULL && grads->ws != NULL && grads->bs != NULL);
  assert(target->data != NULL);
  assert(arena->capacity != 0);

  ui64 n = target->rows;
  grads->as[nn->count] = mat_init(arena, n, nn->as[nn->count].cols, NULL);
  matrix_t *output_layer = &nn->as[nn->count];
  matrix_t *grad_output = &grads->as[nn->count];

  for (ui64 i = 0; i < n; i++) {
    for (ui64 j = 0; j < output_layer->cols; j++) {
      mat_data_type activation = MAT_AT(output_layer, i, j);
      mat_data_type y_true = MAT_AT(target, i, j);
      MAT_AT(grad_output, i, j) = (activation - y_true) * 2 / n;
    }
  }

  for (ui64 layer = nn->count - 1; layer != UINT64_MAX; layer--) {
    matrix_t *a_curr = &nn->as[layer];
    matrix_t *a_prev = &nn->as[layer + 1];
    matrix_t *w = &nn->ws[layer];
    matrix_t *delta_prev = &grads->as[layer + 1];
    matrix_t *delta_curr = &grads->as[layer];

    mem_tmp_arena_t tmp_arena = arena_begin_tmp(arena);
    matrix_t delta_act = mat_init(tmp_arena.arena, n, w->cols, NULL);

    for (ui64 r = 0; r < n; r++) {
      for (ui64 c = 0; c < delta_act.cols; c++) {
        mat_data_type da = MAT_AT(delta_prev, r, c);
        mat_data_type a = MAT_AT(a_prev, r, c);
        MAT_AT(&delta_act, r, c) = da * (a * (1.0 - a));
      }
    }

    matrix_t *grads_b = &grads->bs[layer];
    mat_zero(grads_b);
    for (ui64 r = 0; r < n; r++) {
      for (ui64 c = 0; c < w->cols; c++) {
        MAT_AT(grads_b, 0, c) += MAT_AT(&delta_act, r, c);
      }
    }

    matrix_t a_curr_t =
        mat_init(tmp_arena.arena, a_curr->cols, a_curr->rows, NULL);
    mat_transpose(&a_curr_t, a_curr);
    mat_mult(&grads->ws[layer], &a_curr_t, &delta_act);
    if (layer > 0) {
      grads->as[layer] = mat_init(arena, n, w->rows, NULL);
      delta_curr = &grads->as[layer];
      matrix_t w_t = mat_init(tmp_arena.arena, w->cols, w->rows, NULL);
      mat_transpose(&w_t, w);
      mat_mult(delta_curr, &delta_act, &w_t);
    }
    arena_end_tmp(&tmp_arena);
  }
}

void nn_learn(nn_t *nn, nn_t *grads, mat_data_type rate) {
  assert(nn != NULL && grads != NULL);
  for (ui64 i = 0; i < nn->count; i++) {
    mat_scale_add(&nn->ws[i], &nn->ws[i], &grads->ws[i], -1 * rate);
    mat_scale_add(&nn->bs[i], &nn->bs[i], &grads->bs[i], -1 * rate);
  }
}

void nn_zero(nn_t *nn) {
  for (ui64 i = 0; i < nn->count; i++) {
    mat_zero(&nn->ws[i]);
    mat_zero(&nn->bs[i]);
  }
}

static void _mat_swap_row(matrix_t *m, ui64 r1, ui64 r2,
                          mat_data_type *temp_buf) {
  if (r1 == r2)
    return;
  size_t row_size = m->cols * sizeof(mat_data_type);
  mat_data_type *row1_ptr = m->data + (r1 * m->cols);
  mat_data_type *row2_ptr = m->data + (r2 * m->cols);
  memcpy(temp_buf, row1_ptr, row_size);
  memcpy(row1_ptr, row2_ptr, row_size);
  memcpy(row2_ptr, temp_buf, row_size);
}

static void mat_shuffle_rows(mem_arena_t *arena, matrix_t *input,
                             matrix_t *target) {
  assert(input->rows == target->rows);

  mem_tmp_arena_t tmp_arena = arena_begin_tmp(arena);
  mat_data_type *tmp_in =
      arena_push_arr(*tmp_arena.arena, mat_data_type, input->cols, 1, NULL);

  mat_data_type *tmp_tar =
      arena_push_arr(*tmp_arena.arena, mat_data_type, target->cols, 1, NULL);
  if (!tmp_in || !tmp_tar) {
    fprintf(stderr, "arena failed in shuffle!\n");
    exit(1);
  }
  for (ui64 i = input->rows - 1; i > 0; i--) {
    ui64 j = rand() % (i + 1);
    _mat_swap_row(input, i, j, tmp_in);
    _mat_swap_row(target, i, j, tmp_tar);
  }
  arena_end_tmp(&tmp_arena);
}
void nn_train(mem_arena_t *arena, nn_t *nn, matrix_t *input, matrix_t *target,
              nn_cfg_t *cfg) {
  assert(nn != NULL && input != NULL && target != NULL && cfg != NULL);
  assert(nn->ws != NULL && nn->bs != NULL && nn->as != NULL);
  assert(input->data != NULL && target->data != NULL);
  ui64 simples = target->rows;

  mem_tmp_arena_t epoch_scope = arena_begin_tmp(arena);
  for (ui64 e = 0; e < cfg->epochs; e++) {
    mat_shuffle_rows(arena, input, target);
    for (ui64 s = 0; s < simples; s += cfg->batch_size) {
      mem_tmp_arena_t batch_scope = arena_begin_tmp(arena);
      ui64 current_batch_size = cfg->batch_size;
      if (s + current_batch_size > simples) {
        current_batch_size = simples - s;
      }
      matrix_t batch_x = mat_slice(input, s, current_batch_size);
      matrix_t batch_y = mat_slice(target, s, current_batch_size);
      nn_forward(batch_scope.arena, nn, &batch_x);
      nn_t batch_grads =
          nn_alloc_grads(batch_scope.arena, nn->arch, nn->arch_count, NULL);
      nn_backprop(batch_scope.arena, nn, &batch_grads, &batch_y);
      nn_learn(nn, &batch_grads, cfg->rate);
      arena_end_tmp(&batch_scope);
    }
  }
  arena_end_tmp(&epoch_scope);
}

void nn_print(nn_t *nn) {
  for (ui64 i = 0; i < nn->count; i++) {
    printf("ws[%lu]:\n", i);
    mat_print(&nn->ws[i]);
    printf("bs[%lu]:\n", i);
    mat_print(&nn->bs[i]);
    printf("-----------------------------------\n");
  }
}
