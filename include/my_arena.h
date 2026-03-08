#ifndef ARENA_H
#define ARENA_H
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef uint8_t byte_t;
typedef uint64_t ui64;
typedef struct mem_arena_s mem_arena_t;
typedef struct mem_tmp_arena_s mem_tmp_arena_t;

#define MiB(x) ((ui64)x << 20)
#define KiB(x) ((ui64)x << 10)
#define ALIGN_UP_POW2(n, p) (((ui64)(n) + ((ui64)(p) - 1)) & (~((ui64)(p) - 1)))
#define ARENA_NOMEM ENOMEM
#define ALIGN_SIZE sizeof(void *)
#define ARENA_INVALARG EINVAL

struct mem_arena_s {
  byte_t *base;
  size_t at;
  size_t capacity;
  size_t committed;
};

struct mem_tmp_arena_s {
  mem_arena_t *arena;
  size_t pos_at;
};

#define INIT_ARENA                                                             \
  ((mem_arena_t){.base = NULL, .at = 0, .capacity = 0, .committed = 0})

ssize_t arena_init(mem_arena_t *, ui64, char *);
byte_t *arena_push(mem_arena_t *, ui64, bool, char *);
void arena_free(mem_arena_t *);
void arena_reset(mem_arena_t *);
mem_tmp_arena_t arena_begin_tmp(mem_arena_t *);
void arena_end_tmp(mem_tmp_arena_t *);

#define arena_push_arr(arena, type, size, set_zero, errbuf)                    \
  ((type *)(arena_push((&arena), (sizeof(type) * (size)), set_zero, (errbuf))))
#define arena_push_type(arena, type, set_zero, errbuf)                         \
  ((type *)(arena_push((&arena), (sizeof(type)), set_zero, errbuf)))
#define arena_push_arr_to_zero(arean, type, size, errbuf)                      \
  arena_push_arr(arean, type, size, 1, errbuf)
#define arena_push_type_to_zero(arean, type, errbuf)                           \
  arena_push_type(arean, type, 1, errbuf)
#endif
