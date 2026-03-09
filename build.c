#include "include/soul_build.h"

int main(void) {
  char *cmd_str =
      "gcc -Wall -g -flto -o yxsh src/my_arena.c src/my_string.c "
      "src/yxsh_core.c src/yxsh_executor.c src/yxsh_parser.c main.c";
  return soul_building(cmd_str);
}
