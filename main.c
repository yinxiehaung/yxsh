#include "include/shell.h"

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
      string_t command = str_new_variable_in(arena, buffer);
      if (str_cmp_char(&command, "exit")) {
        break;
      }
      shell_token_list_t *list = shell_tokenize(&arena, &command);
      shell_AST_t *root = shell_parser(&arena, list);
      if (root != NULL) {
        status = shell_executor(&arena, root, status);
      }
    } else {
      printf("\n");
      break;
    }
    arena_end_tmp(&tmp_arena);
  }
  arena_free(&arena);
  return 0;
}
