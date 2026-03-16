#include "../include/my_string.h"

/* **
 * In version 0.5 this string libary not use arena or tmep arena
 * In other wise maybe, feture version str_new like this
 *
 * str_new(arena *mem, char *s, ui64 s_size);
 *
 * and new function tmp str
 *
 * str_tmp(char *s, ui64 s_size) {
 * str_new(tmp_arena, s, s_size);
 * }
 * but maybe that can be cause other problem becuase tmp arena maybe can't
 * retrun alloc addr
 * **
 * @brief create new stirng object
 * @param
 *  s: char * standrand c string
 *  s_size: uint64_t len of s
 * @return string_t (have Alloc error or IN_ARG)
 */
string_t str_new_internal(mem_arena_t *arena, const char *s, ui64 s_len) {
  char *tmp = NULL;
  ui8 is_arena = false;
  if (arena == NULL) {
    tmp = STR_MALLOC(CHAR_SIZE * ((s_len) * 2 + 1));
  } else {
    tmp = arena_push_arr(*arena, char, ((s_len) * 2 + 1), 0, NULL);
    is_arena = true;
  }
  if (tmp == NULL) {
    errno = STR_ALLOC_ERR;
    return INIT_STRING;
  }

  string_t str = {
      .str = tmp, .alloc = s_len * 2 + 1, .len = s_len, .is_arena = is_arena};
  tmp = NULL;

  if (s != NULL && s_len != 0) {
    memcpy(str.str, s, s_len);
  }
  str.str[s_len] = '\0';
  return str;
}

/*
 * @brief create new string somoe s
 * @param
 *  s: string_t
 * @return
 *  string_t
 */
string_t __str_dup(mem_arena_t *arena, const string_t *s) {
  if (s->str == NULL) {
    return INIT_STRING;
  }
  return arena == NULL ? str_new(s->str, s->len)
                       : str_new_in(*arena, s->str, s->len);
}

/*
 * @brief repleace s.str[0] is empty;
 */
void __str_reset(string_t *s) {
  if (s->str == NULL)
    return;
  s->str[0] = '\0';
  s->len = 0;
}

/*
 * @brieft repleace the s.str all to be empty
 */
void __str_clear(string_t *s) {
  if (s->str == NULL)
    return;
  memset(s->str, '\0', CHAR_SIZE * s->alloc);
  s->len = 0;
}

/*
 * @brief free string
 */
void __str_free(string_t *s) {
  if (s->str == NULL || s->is_arena)
    return;
  STR_FREE(s->str);
  s->str = NULL;
}

void __str_print(const string_t *s) {
  if (s->str == NULL)
    return;
  fwrite(s->str, CHAR_SIZE, s->len, stdout);
}

/**
 * @brieft extend alloc size for s.str
 * @param
 * add_len: extend size
 * s: string_t
 */
void str_extend_room(mem_arena_t *arena, string_t *s, ui64 add_len) {
  if (s->str == NULL) {
    errno = STR_INVALARG;
    return;
  }
  if (s->str != NULL && s->alloc > add_len + s->len + 1) {
    return;
  }
  ui64 req = add_len + s->len + 1;

  if (req < (1 << 20)) {
    req = req * 2;
  } else {
    req += (1 << 20);
  }

  char *tmp_ptr = NULL;
  if (arena == NULL) {
    tmp_ptr = (char *)STR_REALLOC(s->str, req);
  } else {
    tmp_ptr = arena_push_arr(*arena, char, req, 1, NULL);
  }
  if (tmp_ptr == NULL) {
    // if not alloc new mem, old mem not to free so s.str is not daling pointer
    errno = STR_ALLOC_ERR;
    return;
  }
  if (arena != NULL) {
    memcpy(tmp_ptr, s->str, s->len);
  }
  s->str = tmp_ptr;
  s->alloc = req;
}

/**
 * @brief cat char string to string[index]
 * @param
 *  s: pointer of string_t
 *  index: index of string
 *  c: cat char string
 * @note
 *  the extend alloc size is O(log_2(c_len + s_len + 1 - s->alloc)
 */
void __str_cat_char_to(mem_arena_t *arena, string_t *s, ui64 index,
                       const char *c) {
  if (s == NULL || s->alloc == 0 || s->str == NULL || c == NULL) {
    errno = STR_INVALARG;
    return;
  }
  if (index > s->len)
    index = s->len;

  ui64 c_len = strlen(c);
  str_extend_room(arena, s, c_len);

  if (s->len > index) {
    memmove(s->str + index + c_len, s->str + index, s->len - index);
  }
  memcpy(s->str + index, c, c_len);
  s->len += c_len;
  s->str[s->len] = '\0';
}

/**
 * @brief cat char string to string[index]
 * @param
 *  s: pointer of string_t
 *  index: index of string
 *  c: cat char string
 * @note
 *  the extend alloc size is O(log_2(c_len + s_len + 1 - s->alloc)
 */
void __str_cat_str_to(mem_arena_t *arena, string_t *s1, ui64 index,
                      const string_t *s2) {
  if (s1 == NULL || s1->alloc == 0 || s1->str == NULL || s2 == NULL ||
      s2->alloc == 0 || s2->str == NULL) {
    errno = STR_INVALARG;
    return;
  }
  if (index > s1->len) {
    index = s1->len;
  }
  str_extend_room(arena, s1, s2->len);

  if (s1->len > index) {
    memmove(s1->str + index + s2->len, s1->str + index,
            CHAR_SIZE * s1->len - index);
  }
  memcpy(s1->str + index, s2->str, CHAR_SIZE * s2->len);

  s1->len += s2->len;
  s1->str[s1->len] = '\0';
}

bool str_cmp_char(const string_t *s, const char *c) {
  if (s == NULL || s->str == NULL)
    return false;
  int c_len = strlen(c);
  if (s->len != c_len) {
    return false;
  }
  return (memcmp(s->str, c, s->len) == 0);
}

bool str_cmp_str(const string_t *s1, const string_t *s2) {
  if (s1 == NULL || s1->str == NULL || s2 == NULL || s2->str == NULL)
    return false;
  if (s1->len != s2->len) {
    return false;
  }
  return (memcmp(s1->str, s2->str, s1->len) == 0);
}

string_t __str_substr(mem_arena_t *arena, const string_t *s, const ui64 start,
                      const ui64 end) {
  if (s == NULL || s->str == NULL || start > end) {
    errno = STR_INVALARG;
    return INIT_STRING;
  }
  int len = end - start;
  return arena == NULL ? str_new(s->str + start, len)
                       : str_new_in(*arena, s->str + start, len);
}

char *__str_find_brute(const string_t *s, const char *c, const ui64 c_len) {
  if (s->alloc == 0 || s->str == NULL || c_len > s->len) {
    errno = STR_INVALARG;
    return NULL;
  }

  for (int i = 0; i < s->len; i++) {
    bool miss = false;
    for (int j = 0; j < c_len; j++) {
      if (s->str[i + j] != c[j]) {
        miss = true;
        break;
      }
    }
    if (miss) {
      continue;
    } else {
      return s->str + i;
    }
  }
  return NULL;
}

char *__str_find_kmp(const string_t *s, const char *c, const ui64 c_len) {
  if (s->alloc == 0 || s->str == NULL || c_len > s->len) {
    errno = STR_INVALARG;
    return CHAR_STR_EMPTY;
  }
  int *fail = STR_MALLOC(sizeof(int) * c_len);
  if (fail == NULL) {
    return NULL;
  }

  fail[0] = 0;
  int i = 1, prev = 0, j = 0;
  while (i < c_len) {
    if (c[i] == c[prev]) {
      fail[i] = prev + 1;
      prev++;
      i++;
    } else if (prev == 0) {
      fail[i] = 0;
      i++;
    } else {
      prev = fail[prev - 1];
    }
  }

  i = 0;

  while (i < s->len) {
    if (s->str[i] == c[j]) {
      i++;
      j++;
    } else {
      if (j == 0) {
        i++;
      } else {
        j = fail[j - 1];
      }
    }
    if (j == c_len) {
      STR_FREE(fail);
      return s->str + i - c_len;
    }
  }
  STR_FREE(fail);
  return NULL;
}

char *__str_find_bmh(const string_t *s, const char *c, const ui64 c_len) {
  if (s->alloc == 0 || s->str == NULL || c_len > s->len) {
    errno = STR_INVALARG;
    return NULL;
  }
  int bad_char[256];
  for (int i = 0; i < 256; i++) {
    bad_char[i] = -1;
  }

  for (int i = 0; i < c_len; i++) {
    bad_char[(unsigned char)c[i]] = i;
  }

  int tmp = 0;
  while (tmp <= (s->len - c_len)) {
    int j = c_len - 1;
    while (j >= 0 && s->str[tmp + j] == c[j]) {
      j--;
    }
    if (j < 0) {
      return s->str + tmp;
    } else {
      int offset = j - bad_char[(unsigned char)s->str[tmp + j]];
      tmp += offset > 1 ? offset : 1;
    }
  }
  return NULL;
}

char *str_find(const string_t *haystack, const char *needle, str_find_e algo) {
  if (haystack == NULL || needle == NULL) {
    errno = STR_INVALARG;
    return NULL;
  }
  ui64 n_len = strlen(needle);
  switch (algo) {
  case STR_FIND_BRUTE:
    return __str_find_brute(haystack, needle, n_len);
  case STR_FIND_KMP:
    return __str_find_kmp(haystack, needle, n_len);
  case STR_FIND_AUTO:
    if (n_len < 32) {
      return __str_find_brute(haystack, needle, n_len);
    }
  case STR_FIND_BMH:
  default:
    return __str_find_bmh(haystack, needle, n_len);
  }
}

ssize_t str_index_of(const string_t *haystack, const char *needle,
                     str_find_e method) {
  if (haystack == NULL || needle == NULL) {
    errno = STR_INVALARG;
    return -1;
  }
  char *p = str_find(haystack, needle, method);
  if (p == NULL)
    return -1;
  return (ssize_t)((void *)p - (void *)haystack->str);
}

string_list_t str_split(mem_arena_t *arena, const string_t *s,
                        const char s_delim) {
  if (s == NULL || s->str == NULL) {
    errno = STR_INVALARG;
    return INIT_STR_LIST;
  }

  string_list_t list = INIT_STR_LIST;
  ui64 s_len = s->len;
  ui64 capacity = 0;
  ui64 last_comm = 0;

  for (int i = 0; i < s_len; i++) {
    if (s->str[i] == s_delim) {
      capacity++;
      last_comm = i;
    }
  }

  if (last_comm < s->len - 1) {
    capacity++;
  }
  capacity++;
  if (arena == NULL) {
    list.items = STR_MALLOC(STR_SIZE * capacity);
    list.is_arena = false;
  } else {
    list.items = arena_push_arr(*arena, string_t, capacity, 0, NULL);
    list.is_arena = true;
  }

  list.capacity = capacity;

  if (list.items == NULL) {
    errno = STR_ALLOC_ERR;
    return INIT_STR_LIST;
  }

  ui64 start = 0;

  for (int i = 0; i < s_len; i++) {
    if (s->str[i] == s_delim) {
      string_t part = __str_substr(arena, s, start, i);
      list.items[list.count++] = part;
      start = i + 1;
    }
  }

  if (start <= s->len) {
    string_t part = __str_substr(arena, s, start, s->len);
    list.items[list.count++] = part;
  }
  return list;
}

void __str_list_free(string_list_t *list) {
  if (list == NULL || list->items == NULL || list->is_arena) {
    return;
  }
  for (int i = 0; i < list->count; i++) {
    str_free(list->items[i]);
  }
  STR_FREE(list->items);
}

bool str_start_with(const string_t *s, const char c) {
  if (s == NULL || s->len == 0 || s->str == NULL) {
    errno = STR_INVALARG;
    return false;
  }
  return s->str[0] == c;
}

bool str_start_with_digit(const string_t *s) {
  if (s == NULL || s->len == 0 || s->str == NULL) {
    errno = STR_INVALARG;
    return false;
  }
  return isdigit((unsigned char)s->str[0]);
}

bool str_start_with_letter(const string_t *s) {
  if (s == NULL || s->len == 0 || s->str == NULL) {
    errno = STR_INVALARG;
    return false;
  }
  return islower((unsigned char)s->str[0]) || isupper((unsigned char)s->str[0]);
}

string_t __str_trim(mem_arena_t *arena, string_t *s) {
  if (s == NULL || s->str == NULL || s->len == 0) {
    errno = STR_INVALARG;
    return INIT_STRING;
  }
  int start = 0, end = s->len - 1;
  while (s->str[start] == ' ' && start < s->len) {
    start++;
  }
  if (s->len == start) {
    return str_new("", 0);
  }
  while (s->str[end] == ' ' && end > start) {
    end--;
  }
  return __str_substr(arena, s, start, end + 1);
}

char *str_to_cstr(mem_arena_t *arena, string_t *str) {
  char *c_str = arena_push_arr(*arena, char, str->len + 1, 1, NULL);
  memcpy(c_str, str->str, str->len);
  c_str[str->len] = '\0';
  return c_str;
}
