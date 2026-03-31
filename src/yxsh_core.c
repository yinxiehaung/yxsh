#include "../include/yxsh_core.h"
#include "yxsh_internal.h"

int yxsh_run(shell_ctx_t *ctx) {
  ctx->command_counter++;
  shell_token_list_t *list = shell_tokenize(&ctx->arena, &ctx->command);
  if (list == NULL) {
    return -1;
  }
  shell_AST_t *root = shell_parser(&ctx->arena, list);
  if (root != NULL) {
    return shell_executor(root, ctx);
  }
  return -1;
}

void init_shell_ctx(shell_ctx_t *ctx, ui64 arena_size, char *errbuf) {
  ctx->arena = INIT_ARENA;
  arena_init(&ctx->arena, arena_size, errbuf);
  ctx->exit_status = 0;
  ctx->command = INIT_STRING;
  ctx->command_counter = 0;
  memset(ctx->pipe_buffer, -1, sizeof(ctx->pipe_buffer));
}
