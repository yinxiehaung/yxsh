#include "../include/yxsh_core.h"
#include "yxsh_internal.h"

int yxsh_run(mem_arena_t *arena, const char *command_char, int status) {
  string_t command_string = str_new_variable_in(*arena, command_char);
  shell_token_list_t *list = shell_tokenize(arena, &command_string);
  shell_AST_t *root = shell_parser(arena, list);
  if (root != NULL) {
    return shell_executor(arena, root, status);
  }
  return -1;
}
