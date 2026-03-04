#include "include/my_arena.h"
#include "include/my_string.h"
#include "include/shell_executor.h"
#include "include/shell_parser.h"
#include <stdio.h>

#define MAX_COMMAND_SIZE (1024 * sizeof(char))

int main(void) {
  mem_arena_t arena = INIT_ARENA;
  char errbuf[1024];
  arena_init(&arena, MiB(1), errbuf);
  while (1) {
    printf("$ ");
    mem_tmp_arena_t tmp_arena = arena_begin_tmp(&arena);
    char buffer[MAX_COMMAND_SIZE];
    if (fgets(buffer, MAX_COMMAND_SIZE, stdin)) {
      string_t command = str_new_variable_in(arena, buffer);
      if (str_cmp_char(&command, "exit\n")) {
        break;
      }
      shell_tokenize(&arena, NULL, &command);
    } else {
      printf("\n");
      break;
    }
    arena_end_tmp(&tmp_arena);
  }
  arena_free(&arena);
  return 0;
}
