#include "../include/my_arena.h"
#include <sys/mman.h>

ssize_t arena_init(mem_arena_t *arena, ui64 reserve_size, char *errbuf) {
  if (arena == NULL || arena->base != NULL || reserve_size <= 0) {
    errno = ARENA_INVALARG;
    if (arena == NULL || arena->base != NULL) {
      sprintf(errbuf, "Faild Invalid arena\n");
    } else {
      sprintf(errbuf, "Faild Invalid reserve size or commit size\n");
    }
    return -1;
  }
  const int page_size = getpagesize();

  ui64 align_reverse_size = ALIGN_UP_POW2(reserve_size, page_size);

  byte_t *mem =
      (byte_t *)mmap(NULL, align_reverse_size, PROT_WRITE | PROT_READ,
                     MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE, -1, 0);
  if (mem == (void *)-1) {
    errno = ENOMEM;
    if (errbuf) {
      sprintf(errbuf, "Falid to alloc page");
    }
    *arena = INIT_ARENA;
    return -1;
  }

  arena->base = mem;
  arena->at = 0;
  arena->committed = 0;
  arena->capacity = align_reverse_size;
  return 0;
}

ssize_t arena_extend_room(mem_arena_t *arena, ui64 size, bool quick_use) {
  if (arena->committed + size > arena->capacity) {
    errno = ARENA_NOMEM;
    return -1;
  }
  arena->committed += size;
  return 0;
}

byte_t *arena_push(mem_arena_t *arena, ui64 request_size, bool set_zero,
                   char *errbuf) {

  if (arena == NULL || arena->base == NULL) {
    if (errbuf) {
      sprintf(errbuf, "Faild invalid arena\n");
    }
    return NULL;
  }
  ui64 align_at = ALIGN_UP_POW2(arena->at, ALIGN_SIZE);
  ui64 new_at = align_at + request_size;
  if (new_at > arena->capacity) {
    errno = ARENA_NOMEM;
    sprintf(errbuf, "Faild no mem\n");
    return NULL;
  }

  byte_t *mem = arena->base + align_at;
  if (set_zero) {
    memset((void *)mem, 0, request_size);
  }
  arena->at = new_at;
  return mem;
}

mem_tmp_arena_t arena_begin_tmp(mem_arena_t *arena) {
  return (mem_tmp_arena_t){.arena = arena, .pos_at = arena->at};
}

void arena_end_tmp(mem_tmp_arena_t *tmp_arena) {
  if (tmp_arena == NULL) {
    return;
  }
  tmp_arena->arena->at = tmp_arena->pos_at;
}

void arena_free(mem_arena_t *arena) {
  if (arena == NULL) {
    return;
  }
  munmap(arena->base, arena->capacity);
  *arena = INIT_ARENA;
}

void arena_reset(mem_arena_t *arena) {
  if (arena == NULL) {
    return;
  }
  arena->at = 0;
  arena->committed = 0;
}
