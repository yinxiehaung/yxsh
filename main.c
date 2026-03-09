#include "include/yxsh_core.h"

#define MAX_COMMAND_SIZE (1024 * sizeof(char))

int main(void) {
  mem_arena_t arena = INIT_ARENA;
  char errbuf[1024];
  arena_init(&arena, MiB(1), errbuf);
  int status = 0;
  while (1) {
    printf("$ ");
    mem_tmp_arena_t tmp_arena = arena_begin_tmp(&arena);
    char buffer[MAX_COMMAND_SIZE];
    if (fgets(buffer, MAX_COMMAND_SIZE, stdin)) {
      buffer[strcspn(buffer, "\n")] = '\0';
      status = yxsh_run(&arena, buffer, status);
    } else {
      printf("\n");
      break;
    }
    arena_end_tmp(&tmp_arena);
  }
  arena_free(&arena);
  return 0;
}
