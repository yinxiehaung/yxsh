#ifndef YXSH_H
#define YXSH_H
#include "my_arena.h"
#include "my_string.h"

#define NUM_PIPE_MAX 128
typedef struct shell_ctx_s shell_ctx_t;

struct shell_ctx_s {
  int exit_status;
  int pipe_buffer[NUM_PIPE_MAX][2];
  string_t command;
  ui64 command_counter;
  mem_arena_t arena;
};
void init_shell_ctx(shell_ctx_t *, ui64, char *);
int yxsh_run(shell_ctx_t *);
#endif
