#include "../include/my_nn_loader.h"

static ui32 swap_ui32(ui32 val) {
  return ((val << 24) & 0xFF000000) | ((val << 8) & 0x00FF0000) |
         ((val >> 8) & 0x0000FF00) | ((val >> 24) & 0x000000FF);
}

void load_mnist(const char *filename, matrix_t *m, ui32 num_classes) {
  FILE *f = fopen(filename, "rb");
  if (!f) {
    fprintf(stderr, "Could not open MNIST file: %s\n", filename);
    exit(1);
  }
  ui32 magic, count;
  fread(&magic, sizeof(ui32), 1, f);
  fread(&count, sizeof(ui32), 1, f);
  magic = swap_ui32(magic);
  count = swap_ui32(count);
  if (count != m->rows) {
    fprintf(stderr, "Size mismatch! File has %u items, Matrix has %lu rows.\n",
            count, m->rows);
    exit(1);
  }
  if (magic == 2051) {
    printf("[MNIST] Loading Images from %s...\n", filename);
    ui32 rows, cols;
    fread(&rows, sizeof(ui32), 1, f);
    fread(&cols, sizeof(ui32), 1, f);
    rows = swap_ui32(rows);
    cols = swap_ui32(cols);
    for (uint32_t i = 0; i < count; i++) {
      for (uint32_t j = 0; j < rows * cols; j++) {
        uint8_t pixel;
        fread(&pixel, 1, 1, f);
        MAT_AT(m, i, j) = (mat_data_type)pixel / 255.0f;
      }
    }
  } else if (magic == 2049) {
    printf("[MNIST] Loading Labels from %s...\n", filename);
    for (uint32_t i = 0; i < count; i++) {
      uint8_t label;
      fread(&label, 1, 1, f);
      if (label >= num_classes) {
        fprintf(stderr, "Error: Label %d is out of bounds (max %d)\n", label,
                num_classes);
        exit(1);
      }
      for (int k = 0; k < num_classes; k++) {
        MAT_AT(m, i, k) = 0.0f;
      }
      MAT_AT(m, i, label) = 1.0f;
    }
  } else {
    fprintf(stderr,
            "Unknown Magic Number: %u (Are you sure this is a MNIST file?)\n",
            magic);
    exit(1);
  }
  fclose(f);
  printf("Done.\n");
}

void nn_save_model(nn_t *nn, const char *path) {
  FILE *f = fopen(path, "wb");
  if (!f)
    return;
  nn_file_header_t header = {
      .magic = NN_MODEL_MAGIC, .version = 1, .layer_count = nn->count};
  fwrite(&header, sizeof(header), 1, f);
  fwrite(nn->arch, sizeof(uint64_t), nn->count, f);
  for (size_t i = 0; i < nn->count; i++) {
    fwrite(nn->ws[i].data, sizeof(mat_data_type),
           nn->ws[i].rows * nn->ws[i].cols, f);
    fwrite(nn->bs[i].data, sizeof(mat_data_type),
           nn->bs[i].rows * nn->bs[i].cols, f);
  }
  fclose(f);
  printf("[Loader] Model saved to %s\n", path);
}

int nn_load_model(nn_t *nn, const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return 0;
  nn_file_header_t header;
  if (fread(&header, sizeof(header), 1, f) != 1)
    return 0;

  if (header.magic != NN_MODEL_MAGIC) {
    fprintf(stderr, "Invalid file format!\n");
    return 0;
  }
  if (header.version != NN_VERSION) {
    fprintf(stderr, "Unsupported version!\n");
    return 0;
  }
  if (header.layer_count != nn->count) {
    fprintf(stderr, "Layer count mismatch! File: %d, Model: %lu\n",
            header.layer_count, nn->count);
    return 0;
  }
  uint64_t *file_arch = malloc(sizeof(uint64_t) * header.layer_count);
  fread(file_arch, sizeof(uint64_t), header.layer_count, f);

  for (size_t i = 0; i < header.layer_count; i++) {
    if (file_arch[i] != nn->arch[i]) {
      fprintf(stderr, "Architecture mismatch at layer %zu!\n", i);
      free(file_arch);
      return 0;
    }
  }
  free(file_arch);
  for (size_t i = 0; i < nn->count; i++) {
    matrix_t *w = &nn->ws[i];
    fread(w->data, sizeof(mat_data_type), w->rows * w->cols, f);

    matrix_t *b = &nn->bs[i];
    fread(b->data, sizeof(mat_data_type), b->rows * b->cols, f);
  }

  fclose(f);
  printf("Model loaded from %s\n", path);
  return 1;
}
