#include "../include/soul_build.h"

int soul_building(const char *building_char) {
  printf("building command:\n %s\n", building_char);
  mem_arena_t arena = INIT_ARENA;
  arena_init(&arena, KiB(1), NULL);
  int status = 0;
  status = yxsh_run(&arena, building_char, status);
  if (status != 0) {
    perror("building error\n");
    arena_free(&arena);
    exit(status);
  }
  arena_free(&arena);
  return status;
}
