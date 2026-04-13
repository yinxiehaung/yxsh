#include "../include/yxsh_core.h"
#include "yxsh_internal.h"

int yxsh_run(shell_ctx_t *ctx) {
  int status;
  pid_t done_pid;

  while ((done_pid = waitpid(-1, &status, WNOHANG)) > 0) {
    ctx->exit_status = status;
  }

  shell_token_list_t *list = shell_tokenize(&ctx->arena, &ctx->command);
  if (list == NULL || list->head == NULL) {
    return -1;
  }
  shell_AST_t *root = shell_parser(&ctx->arena, list);
  ui64 ret_status = -1;
  if (root != NULL) {
    ret_status = shell_executor(root, ctx);
  }
  return ret_status;
}

void init_shell_ctx(shell_ctx_t *ctx, ui64 arena_size, char *errbuf) {
  // setenv("PATH", "bin:.", 1);
  ctx->arena = INIT_ARENA;
  arena_init(&ctx->arena, arena_size, errbuf);
  ctx->exit_status = 0;
  ctx->command = INIT_STRING;
  ctx->command_counter = 0;
  memset(ctx->pipe_buffer, -1, sizeof(ctx->pipe_buffer));
}
