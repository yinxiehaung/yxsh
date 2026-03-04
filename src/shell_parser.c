#include "../include/shell_parser.h"
#include <ctype.h>
#define CHAR_IN_STR(c, s) ((c) - (*(s)).str < ((*(s)).len))
#define DEBUG_MODE

enum shell_token_type_e {
  TOKEN_START,
  TOKEN_SYMBOL,
  TOKEN_WORD,
  TOKEN_SINGLE_QUOTE,
  TOKEN_DOUBLE_QUOTE,
  TOKEN_VAR,
  TOKEN_EQU,
  TOKEN_COMMENT,
  TOKEN_ILLEGAL
};

typedef enum shell_token_type_e shell_token_type_t;
typedef struct lexer_ctx_s lexer_ctx_t;
typedef bool (*lex_rule_fnp_t)(char c);

struct shell_token_s {
  string_t key;
  shell_token_type_t token_state;
};

struct shell_token_list_t {
  shell_token_t *tokens;
  ui64 capacity;
};

struct lexer_ctx_s {
  string_t *source;
  char *cursor;
  char *token_start;
  shell_token_type_t state;
  shell_token_list_t *list;
};

struct shell_AST_t {};

static bool is_white_space(char c) {
  return c == '\t' || c == '\r' || c == ' ' || c == '\n' || c == '\0';
}
static bool is_vaild_comment(char c) { return c == '#'; }

static bool is_vaild_symbol(char c) {
  return c == '>' || c == '<' || c == '!' || c == '$' || c == ';' || c == '|' ||
         c == '&';
}

static bool is_single_quote(char c) { return (c) == '\''; }

static bool is_double_quote(char c) { return (c) == '\"'; }

static bool is_equal(char c) { return (c) == '='; };

static bool is_vaild_word(char c) {
  return !(is_vaild_symbol(c)) && !(is_white_space(c)) &&
         (!is_vaild_comment(c)) && !(is_single_quote(c)) &&
         !(is_double_quote(c));
}

static bool is_vaild_var(char c) { return is_vaild_word(c); }

static void lex_start_state(lexer_ctx_t *ctx) {
  while (CHAR_IN_STR(ctx->cursor, ctx->source) &&
         is_white_space(*(ctx->cursor))) {
    ctx->cursor += 1;
  }
  ctx->token_start = ctx->cursor;
  if (is_vaild_word(*(ctx->cursor))) {
    ctx->state = TOKEN_WORD;
  } else if (is_vaild_symbol(*(ctx->cursor))) {
    ctx->state = TOKEN_SYMBOL;
  } else if (is_vaild_comment(*(ctx->cursor))) {
    ctx->state = TOKEN_COMMENT;
  } else if (is_single_quote(*(ctx->cursor))) {
    ctx->state = TOKEN_SINGLE_QUOTE;
  } else if (is_double_quote(*(ctx->cursor))) {
    ctx->state = TOKEN_DOUBLE_QUOTE;
  } else if (is_equal(*(ctx->cursor))) {
    ctx->state = TOKEN_EQU;
  } else {
    ctx->state = TOKEN_ILLEGAL;
  }
}

static void lex_symbol_state(lexer_ctx_t *ctx) {
  char *tmp = ctx->cursor;
  while (CHAR_IN_STR(tmp, ctx->source) && is_vaild_symbol(*tmp)) {
    tmp++;
  }
  string_t str = {.str = ctx->token_start,
                  .alloc = 0,
                  .len = tmp - ctx->token_start,
                  .is_arena = 0};
#ifdef DEBUG_MODE
  printf("[TOKEN_SYMBOL]\n");
  str_print(str);
  printf("\n");
#endif
  ctx->state = TOKEN_START;
  ctx->cursor = tmp;
  return;
}

static void lex_single_quote_state(lexer_ctx_t *ctx) {
  char *tmp = ctx->cursor + 1;
  while (CHAR_IN_STR(tmp, ctx->source) && !(is_single_quote(*tmp))) {
    tmp++;
  }
  tmp++; // 補上最後一個
  ctx->state = TOKEN_WORD;
  ctx->cursor = tmp;
  return;
}

static void lex_double_quote_state(lexer_ctx_t *ctx) {
  char *tmp = ctx->cursor + 1;
  while (CHAR_IN_STR(tmp, ctx->source) && !(is_double_quote(*tmp))) {
    tmp++;
  }
  tmp++;
  ctx->state = TOKEN_WORD;
  ctx->cursor = tmp;
  return;
}

static void lex_word_state(lexer_ctx_t *ctx) {
  char *tmp = ctx->cursor;
  while (CHAR_IN_STR(tmp, ctx->source) && is_vaild_word(*tmp)) {
    if (*tmp == '\\') {
      tmp++;
    }
    tmp++;
    if (is_single_quote(*tmp)) {
      ctx->cursor = tmp;
      lex_single_quote_state(ctx);
      tmp = ctx->cursor;
    } else if (is_double_quote(*tmp)) {
      ctx->cursor = tmp;
      lex_double_quote_state(ctx);
      tmp = ctx->cursor;
    }
  }
  string_t str = {.str = ctx->token_start,
                  .alloc = 0,
                  .len = tmp - ctx->token_start,
                  .is_arena = false};
#ifdef DEBUG_MODE
  printf("[TOKEN_WORD]\n");
  str_print(str);
  printf("\n");
#endif
  ctx->state = TOKEN_START;
  ctx->cursor = tmp;
  return;
}

static void lex_var_state(lexer_ctx_t *ctx) {
  char *tmp = ctx->token_start;
  while (CHAR_IN_STR(tmp, ctx->source) && is_vaild_var(*tmp)) {
    tmp++;
  }
  string_t str = {.str = ctx->token_start,
                  .alloc = 0,
                  .len = tmp - ctx->token_start,
                  .is_arena = false};
#ifdef DEBUG_MODE
  printf("[TOKEN VAR]\n");
  str_print(str);
#endif
  ctx->state = TOKEN_START;
  return;
}

static void lex_equ_state(lexer_ctx_t *ctx) {
  char *tmp = ctx->token_start;
  while (CHAR_IN_STR(tmp, ctx->source) && is_equal(*tmp)) {
    tmp++;
  }
  string_t str = {.str = ctx->token_start,
                  .alloc = 0,
                  .len = tmp - ctx->token_start,
                  .is_arena = false};
#ifdef DEBUG_MODE
  printf("[TOKEN EQULE]\n");
  str_print(str);
#endif
  ctx->state = TOKEN_START;
  return;
}

static void lex_comment_state(lexer_ctx_t *ctx) {
  char *tmp = ctx->token_start;
  while (CHAR_IN_STR(tmp, ctx->source)) {
    tmp++;
  }
  string_t str = {.str = ctx->token_start,
                  .alloc = 0,
                  .len = tmp - ctx->token_start,
                  .is_arena = false};
#ifdef DEBUG_MODE
  printf("[TOKEN COMMENT]\n");
  str_print(str);
#endif
  ctx->state = TOKEN_START;
  ctx->cursor = tmp;
  return;
}

static void lex_illegal_state(lexer_ctx_t *ctx) { return; }

void shell_tokenize(mem_arena_t *arena, shell_token_list_t *dst,
                    string_t *command) {
  lexer_ctx_t ctx = {.source = command,
                     .cursor = command->str,
                     .token_start = command->str,
                     .state = TOKEN_START,
                     .list = dst};
  while (CHAR_IN_STR(ctx.cursor, ctx.source)) {
    switch (ctx.state) {
    case TOKEN_START:
      lex_start_state(&ctx);
      break;
    case TOKEN_SYMBOL:
      lex_symbol_state(&ctx);
      break;
    case TOKEN_WORD:
      lex_word_state(&ctx);
      break;
    case TOKEN_SINGLE_QUOTE:
      lex_single_quote_state(&ctx);
      break;
    case TOKEN_DOUBLE_QUOTE:
      lex_double_quote_state(&ctx);
      break;
    case TOKEN_VAR:
      // lex_var_state(&ctx);
      break;
    case TOKEN_EQU:
      // lex_equ_state(&ctx);
      break;
    case TOKEN_COMMENT:
      lex_comment_state(&ctx);
      break;
    default:
      // lex_illegal_state(&ctx);
      break;
    }
  }
}
void shell_parser(mem_arena_t *arena, shell_AST_t *dst,
                  shell_token_list_t *src) {
  return;
}
