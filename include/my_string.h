#ifndef MY_STRING_H
#define MY_STRING_H
#include "my_arena.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h> // provide malloc, realooc ... etc but can repleace arena allocator
#include <string.h>

typedef uint64_t ui64;
typedef uint8_t ui8;
typedef struct string_s string_t;
typedef struct string_list_s string_list_t;

#define STR_SIZE sizeof(string_t)
#define STR_PTR_SIZE sizeof(string_t *)
#define CHAR_SIZE sizeof(char)
#define STR_ALLOC_ERR ENOMEM
#define STR_INVALARG EINVAL

// #define MiB (1 << 20);
// #define KiB (1 << 10);

// maybe can refers sds implenment
// github: https://github.com/antirez/sds/tree/master
struct string_s {
  char *str;
  ui64 alloc;
  ui64 len;
  ui8 is_arena;
};

struct string_list_s {
  string_t *items;
  ui64 capacity;
  ui64 count;
  ui8 is_arena;
};

typedef enum {
  STR_FIND_AUTO = 1000,
  STR_FIND_KMP,
  STR_FIND_BMH,
  STR_FIND_BRUTE
} str_find_e;

char *str_to_cstr(mem_arena_t *, string_t *);
string_t str_new_internal(mem_arena_t *, const char *, ui64);
void __str_free(string_t *);

string_t __str_dup(mem_arena_t *, const string_t *);

void __str_clear(string_t *);
void __str_reset(string_t *);

void __str_cat_char_to(mem_arena_t *, string_t *, ui64, const char *);
void __str_cat_str_to(mem_arena_t *, string_t *, ui64, const string_t *);

string_t __str_substr(mem_arena_t *, const string_t *, const ui64, const ui64);

bool str_cmp_char(const string_t *, const char *);
bool str_cmp_str(const string_t *, const string_t *);

void str_extend_room(mem_arena_t *, string_t *, ui64);

char *str_find(const string_t *, const char *, str_find_e);
char *__str_find_bmh(const string_t *, const char *, const ui64);
char *__str_find_kmp(const string_t *, const char *, const ui64);
char *__str_find_brute(const string_t *, const char *, const ui64);
ssize_t str_index_of(const string_t *, const char *, str_find_e method);

string_list_t str_split(mem_arena_t *, const string_t *, const char);
string_list_t str_token(const string_t *, const char *);
void __str_list_free(string_list_t *);

string_t __str_trim(mem_arena_t *, string_t *);
bool str_start_with(const string_t *, const char);
bool str_start_with_digit(const string_t *);
bool str_start_with_letter(const string_t *);

// void str_fmt(string_t, ...);
void __str_print(const string_t *);

#define INIT_STRING ((string_t){.str = NULL, .alloc = 0, .len = 0})
#define INIT_STR_LIST                                                          \
  ((string_list_t){.items = NULL, .capacity = 0, .count = 0})
#define CHAR_STR_EMPTY ""
#define STR_MALLOC malloc
#define STR_FREE free
#define STR_REALLOC realloc

/**
 * Warring: if string_list_t list to using uder marcor like this
 * str_cat_str_to_begin(list[i++]) -> __str_str_char_to(list[i++],
 * list[i++].len, c) is warring becuaze i++ do twise and seconend i++ is not
 * previous i++ string
 */

#define str_new(x, len) str_new_internal(NULL, x, len)
#define str_new_in(arena, x, len) str_new_internal((&arena), x, len)

#define str_new_static(x) (str_new((x), sizeof((x)) - 1))
#define str_new_variable(x) (str_new((x), strlen(x)))

#define str_new_static_in(arena, x) (str_new_in(arena, (x), sizeof((x)) - 1))
#define str_new_variable_in(arena, x) (str_new_in(arena, (x), strlen(x)))

#define str_cat_char_to_end(s1, c) (__str_cat_char_to(NULL, &s1, s1.len, c))
#define str_cat_char_to_begin(s1, c) (__str_cat_char_to(NULL, &s1, 0, c))
#define str_cat_char_to(s1, i, c) (__str_cat_char_to(NULL, &s1, i, c))

#define str_cat_char_to_end_in(arena, s1, c)                                   \
  (__str_cat_char_to(&arena, &s1, s1.len, c))
#define str_cat_char_to_begin_in(arena, s1, c)                                 \
  (__str_cat_char_to(&arena, &s1, 0, c))
#define str_cat_char_in(arena, s1, i, c) (__str_cat_char_to(&arena, &s1, i, c))

#define str_cat_str_to_begin(s1, s2) (__str_cat_str_to(NULL, &s1, 0, &s2))
#define str_cat_str_to_end(s1, s2) (__str_cat_str_to(NULL, &s1, s1.len, &s2))
#define str_cat_str_to(s1, i, s2) (__str_cat_str_to(NULL, &s1, i, &s2))

#define str_cat_str_to_begin_in(arena, s1, s2)                                 \
  (__str_cat_str_to(&arena, &s1, 0, &s2))
#define str_cat_str_to_end_in(arena, s1, s2)                                   \
  (__str_cat_str_to(&arena, &s1, s1.len, &s2))
#define str_cat_str_in(arena, s1, i, s2) (__str_cat_str_to(&arena, &s1, i, &s2))

#define str_free(x) (__str_free(&x))
#define str_list_free(x) (__str_list_free(&x));

#define str_dup(x) (__str_dup(NULL, &x))
#define str_dup_in(arena, x) (__str_dup(arena, &x))

#define str_clear(x) (__str_clear(&x))

#define str_reset(x) (__str_reset(&x))

#define str_print(x) (__str_print(&x))

#define str_substr(x, s, e) (__str_substr(NULL, &x, s, e))
#define str_substr_in(arena, x, s, e) (__str_sub_str(arena, x, s, e))

#define str_trim(x) (__str_trim(NULL, x))
#define str_trim_in(arena, x) (__str_trim(&arena, x))
#endif
