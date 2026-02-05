#ifndef NN_LOADER_H
#define NN_LOADER_H
#include "my_matrix.h"
#include "my_nn.h"

#define NN_MODEL_MAGIC 0x584E4959
#define NN_VERSION 1

typedef struct nn_file_header_s nn_file_header_t;

struct nn_file_header_s {
  ui32 magic;
  ui32 version;
  ui32 layer_count;
};

void load_mnist(const char *, matrix_t *, ui32);
void nn_save_model(nn_t *, const char *path);
int nn_load_model(nn_t *, const char *path);
#endif
