#include "../include/yxsh_core.h"
#include "yxsh_internal.h"

int yxsh_run(mem_arena_t *arena, const char *command_char, int prev_status) {
  string_t command_string = str_new_variable_in(*arena, command_char);
  shell_token_list_t *list = shell_tokenize(arena, &command_string);
  if (list == NULL) {
    return -1;
  }
  shell_AST_t *root = shell_parser(arena, list);
  if (root != NULL) {
    command_status_t status;
    status.exit_status = prev_status;
    for (int i = 0; i < 1024; i++) {
      status.pipe_buffer[i] = -1;
    }
    return shell_executor(arena, root, &status);
  }
  return -1;
}
