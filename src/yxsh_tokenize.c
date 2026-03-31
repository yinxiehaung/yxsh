#include "yxsh_internal.h"

// #define DEBUG_MODE_TOKENIZE

static bool is_white_space(char c) {
  return c == '\t' || c == '\r' || c == ' ' || c == '\n' || c == '\0';
}

static bool is_vaild_comment(char c) { return c == '#'; }

static bool is_vaild_symbol(char c) {
  return c == '>' || c == '<' || c == '!' || c == ';' || c == '|' || c == '&';
}

static bool is_single_quote(char c) { return (c) == '\''; }

static bool is_double_quote(char c) { return (c) == '\"'; }

static bool is_vaild_word(char c) {
  return !(is_vaild_symbol(c)) && !(is_white_space(c)) &&
         !(is_vaild_comment(c)) && !(is_single_quote(c)) &&
         !(is_double_quote(c));
}

static void token_list_init(lexer_ctx_t *ctx) {
  ctx->list = arena_push_type(*ctx->arena, shell_token_list_t, 1, NULL);
  ctx->list->tail = NULL;
  ctx->list->head = NULL;
  return;
}

#ifdef DEBUG_MODE_TOKENIZE
static void token_list_helper(shell_token_list_t *list) {
  shell_token_t *curr = list->head;
  while (curr != NULL) {
    switch (curr->token_state) {
    case TOKEN_WORD:
      printf("[TOKEN WORD]\n");
      str_print(curr->key);
      printf("\n");
      break;
    case TOKEN_SYMBOL:
      printf("[TOKEN_SYMBOL]\n");
      str_print(curr->key);
      printf("\n");
      break;
    case TOKEN_COMMENT:
      printf("[TOKEN_COMMENT]\n");
      str_print(curr->key);
      printf("\n");
      break;
    default:
      break;
    }
    curr = curr->next;
  }
}
#endif

static void token_list_add(lexer_ctx_t *ctx, string_t *str) {
  shell_token_t *token = arena_push_type(*ctx->arena, shell_token_t, 1, NULL);
  token->key = *str;
  token->token_state = ctx->state;
  token->next = NULL;
  shell_token_list_t *tokens_list = ctx->list;
  if (tokens_list->head == NULL) {
    tokens_list->tail = token;
    tokens_list->head = token;
  } else {
    tokens_list->tail->next = token;
    tokens_list->tail = token;
  }
  return;
}

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
  } else {
    ctx->state = TOKEN_ILLEGAL;
  }
}

static void lex_symbol_state(lexer_ctx_t *ctx) {
  char *tmp = ctx->cursor;
  ui64 symbol_len = 1;
  switch (*tmp) {
  case '>':
    if (*(tmp + 1) == '>') {
      symbol_len = 2;
    }
    break;
  case '<':
    if (*(tmp + 1) == '<') {
      symbol_len = 2;
    }
    break;
  case '|':
    if (*(tmp + 1) == '|') {
      symbol_len = 2;
    } else {
      while (isdigit(*(tmp + 1))) {
        symbol_len++;
        tmp++;
      }
    }
    break;
  case '&':
    if (*(tmp + 1) == '&') {
      symbol_len = 2;
    }
    break;
  case ';':
    if (*(tmp + 1) == ';') {
      symbol_len = 2;
    }
    break;
  default:
    ctx->state = TOKEN_ILLEGAL;
    break;
  }
  string_t str = {
      .str = ctx->token_start, .alloc = 0, .len = symbol_len, .is_arena = 0};
  token_list_add(ctx, &str);
  ctx->state = TOKEN_START;
  ctx->cursor = tmp + symbol_len;
  return;
}

static void lex_single_quote_state(lexer_ctx_t *ctx) {
  char *tmp = ctx->cursor + 1;
  while (CHAR_IN_STR(tmp, ctx->source) && !(is_single_quote(*tmp))) {
    tmp++;
  }
  if (CHAR_IN_STR(tmp, ctx->source)) {
    tmp++;
  } else {
    ctx->state = TOKEN_ILLEGAL;
    return;
  }
  ctx->state = TOKEN_WORD;
  ctx->cursor = tmp;
  return;
}

static void lex_double_quote_state(lexer_ctx_t *ctx) {
  char *tmp = ctx->cursor + 1;
  while (CHAR_IN_STR(tmp, ctx->source) && !(is_double_quote(*tmp))) {
    tmp++;
  }
  if (CHAR_IN_STR(tmp, ctx->source)) {
    tmp++;
  } else {
    ctx->state = TOKEN_ILLEGAL;
    return;
  }
  ctx->state = TOKEN_WORD;
  ctx->cursor = tmp;
  return;
}

static void lex_word_state(lexer_ctx_t *ctx) {
  char *tmp = ctx->cursor;
  while (CHAR_IN_STR(tmp, ctx->source) && is_vaild_word(*tmp)) {
    if (*tmp == '\\') {
      tmp++;
      if (!CHAR_IN_STR(tmp, ctx->source) || *tmp == '\0') {
        break;
      }
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
  token_list_add(ctx, &str);
  ctx->state = TOKEN_START;
  ctx->cursor = tmp;
  return;
}

static void lex_comment_state(lexer_ctx_t *ctx) {
  char *tmp = ctx->token_start;
  while (CHAR_IN_STR(tmp, ctx->source)) {
    tmp++;
  }
  ctx->state = TOKEN_START;
  ctx->cursor = tmp;
  return;
}

static void lex_illegal_state(lexer_ctx_t *ctx) {
  char *tmp = ctx->token_start;
  while (CHAR_IN_STR(tmp, ctx->source)) {
    tmp++;
  }
  string_t str = {.str = ctx->token_start,
                  .alloc = 0,
                  .len = tmp - ctx->token_start,
                  .is_arena = false};
  token_list_add(ctx, &str);
  ctx->state = TOKEN_START;
  ctx->cursor = tmp;
  return;
}

shell_token_list_t *shell_tokenize(mem_arena_t *arena, string_t *command) {
  lexer_ctx_t ctx = {.source = command,
                     .cursor = command->str,
                     .token_start = command->str,
                     .state = TOKEN_START,
                     .list = NULL,
                     .arena = arena};
  token_list_init(&ctx);
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
    case TOKEN_COMMENT:
      lex_comment_state(&ctx);
      break;
    default:
      lex_illegal_state(&ctx);
      break;
    }
  }
  if (ctx.state == TOKEN_WORD || ctx.state == TOKEN_SYMBOL) {
    string_t str = {.str = ctx.token_start,
                    .alloc = 0,
                    .len = ctx.cursor - ctx.token_start,
                    .is_arena = false};
    token_list_add(&ctx, &str);
  }
#ifdef DEBUG_MODE_TOKENIZE
  token_list_helper(ctx.list);
#endif
  return ctx.list;
}
