#include "include/my_string.h"
#include "include/yxsh_core.h"

#define MAX_COMMAND_SIZE (1024 * sizeof(char))

int main(void) {
  shell_ctx_t gctx;
  char errbuf[1024];
  init_shell_ctx(&gctx, MiB(1), errbuf);
  while (1) {
    printf("[%ld]%%", gctx.command_counter);
    mem_tmp_arena_t tmp_arena = arena_begin_tmp(&gctx.arena);
    char buffer[MAX_COMMAND_SIZE];
    if (fgets(buffer, MAX_COMMAND_SIZE, stdin)) {
      buffer[strcspn(buffer, "\n")] = '\0';
      gctx.command = str_new_variable_in(gctx.arena, buffer);
      gctx.exit_status = yxsh_run(&gctx);
    } else {
      printf("\n");
      break;
    }
    arena_end_tmp(&tmp_arena);
  }
  arena_free(&gctx.arena);
  return 0;
}
