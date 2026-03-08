#include "../include/shell.h"

// #define DEBUG_MODE_TOKENIZE
// #define DEBUG_MODE_PARSING

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
    }
    break;
  case '&':
    if (*(tmp + 1) == '&') {
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

static shell_AST_t *parsing_command(parser_ctx_t *ctx) {
  shell_token_t *cursor = ctx->curr_token;

  if (str_cmp_char(&cursor->key, "&&") || str_cmp_char(&cursor->key, "||") ||
      str_cmp_char(&cursor->key, "|") || str_cmp_char(&cursor->key, "&")) {
    fprintf(stderr, "yxsh: syntax error near unexpected token `%s`\n",
            cursor->key.str);
    return NULL;
  }

  shell_AST_t *ast = arena_push_type(*ctx->arena, shell_AST_t, 1, NULL);
  while ((cursor != NULL)) {
    enum shell_token_type_e state = cursor->token_state;
    if (state == TOKEN_SYMBOL && str_cmp_char(&cursor->key, ">")) {
      cursor = cursor->next;
      if (cursor == NULL) {
        fprintf(stderr, "yxsh: syntax error: expected filename after >\n");
        return NULL;
      }
      ast->file_out = cursor->key;
      cursor = cursor->next;
    } else if (state == TOKEN_SYMBOL && str_cmp_char(&cursor->key, ">>")) {
      cursor = cursor->next;
      ast->file_out = cursor->key;
      ast->is_append = true;
      cursor = cursor->next;
    } else if (state == TOKEN_SYMBOL && str_cmp_char(&cursor->key, "<")) {
      cursor = cursor->next;
      ast->file_in = cursor->key;
      ast->is_append = false;
      cursor = cursor->next;
    } else if (state == TOKEN_SYMBOL && str_cmp_char(&cursor->key, "<<")) {
      cursor = cursor->next;
      ast->heredoc.str = cursor->key.str;
      cursor = cursor->next;
    } else if (state == TOKEN_WORD) {
      ast->argc++;
      cursor = cursor->next;
    } else if (state == TOKEN_ILLEGAL) {
      fprintf(stderr, "yxsh: syntax error near unexpected token `%s`\n",
              cursor->key.str);
      return NULL;
    } else {
      // other symbol
      break;
    }
  }
  if (ast->argc > 0) {
    ast->argv = arena_push_arr(*ctx->arena, string_t, ast->argc, 1, NULL);
    cursor = ctx->curr_token;
    ui64 arg_idx = 0;
    while (cursor != NULL && arg_idx < ast->argc) {
      shell_token_type_t state = cursor->token_state;
      if (state == TOKEN_SYMBOL && (str_cmp_char(&cursor->key, ">") ||
                                    str_cmp_char(&cursor->key, ">>") ||
                                    str_cmp_char(&cursor->key, "<") ||
                                    str_cmp_char(&cursor->key, "<<"))) {
        cursor = cursor->next;
        if (cursor != NULL)
          cursor = cursor->next;
      } else if (state == TOKEN_WORD) {
        ast->argv[arg_idx] = cursor->key;
        arg_idx++;
        cursor = cursor->next;
      } else {
        // other symbol
        break;
      }
    }
  }
  ast->state = AST_NODE_COMMAND;
  ctx->curr_token = cursor;
  return ast;
}

static shell_AST_t *parsing_pipe(parser_ctx_t *ctx) {
  shell_AST_t *root = parsing_command(ctx);
  if (root == NULL)
    return NULL;
  while (ctx->curr_token != NULL &&
         ctx->curr_token->token_state == TOKEN_SYMBOL &&
         str_cmp_char(&ctx->curr_token->key, "|")) {
    ctx->curr_token = ctx->curr_token->next;
    shell_AST_t *right = parsing_command(ctx);
    shell_AST_t *pipe_ast = arena_push_type(*ctx->arena, shell_AST_t, 1, NULL);
    pipe_ast->state = AST_NODE_PIPE;
    pipe_ast->left = root;
    pipe_ast->right = right;
    root = pipe_ast;
  }
  return root;
}

static shell_AST_t *parsing_and_or(parser_ctx_t *ctx) {
  shell_AST_t *root = parsing_pipe(ctx);
  if (root == NULL)
    return NULL;
  while (ctx->curr_token != NULL &&
         ctx->curr_token->token_state == TOKEN_SYMBOL &&
         (str_cmp_char(&ctx->curr_token->key, "&&") ||
          str_cmp_char(&ctx->curr_token->key, "||"))) {
    shell_AST_t *and_or_ast =
        arena_push_type(*ctx->arena, shell_AST_t, 1, NULL);

    if (str_cmp_char(&ctx->curr_token->key, "&&")) {
      and_or_ast->state = AST_NODE_DOUBLE_AND;
    } else {
      and_or_ast->state = AST_NODE_OR;
    }
    ctx->curr_token = ctx->curr_token->next;
    shell_AST_t *right = parsing_pipe(ctx);
    and_or_ast->left = root;
    and_or_ast->right = right;
    root = and_or_ast;
  }
  return root;
}

static shell_AST_t *parsing_list(parser_ctx_t *ctx) {
  shell_AST_t *root = parsing_and_or(ctx);
  if (root == NULL)
    return NULL;
  while (ctx->curr_token != NULL &&
         ctx->curr_token->token_state == TOKEN_SYMBOL &&
         (str_cmp_char(&ctx->curr_token->key, "&") ||
          str_cmp_char(&ctx->curr_token->key, ";"))) {
    shell_AST_t *list = arena_push_type(*ctx->arena, shell_AST_t, 1, NULL);
    if (str_cmp_char(&ctx->curr_token->key, "&")) {
      list->state = AST_NODE_AND;
    } else if (str_cmp_char(&ctx->curr_token->key, ";")) {
      list->state = AST_NODE_SEQUENCE;
    }
    ctx->curr_token = ctx->curr_token->next;
    if (ctx->curr_token != NULL) {
      list->right = parsing_and_or(ctx);
    } else {
      list->right = NULL;
    }
    list->left = root;
    root = list;
  }
  return root;
}

#ifdef DEBUG_MODE_PARSING
static void shell_AST_helper(shell_AST_t *ast) {
  switch (ast->state) {
  case AST_NODE_COMMAND:
    printf("[AST NODE COMMAND]\n");
    break;
  case AST_NODE_PIPE:
    printf("[AST NODE PIPE]\n");
    break;
  case AST_NODE_AND:
    printf("[AST NODE AND]\n");
    break;
  case AST_NODE_DOUBLE_AND:
    printf("[AST NODE DOUBLE AND]\n");
    break;
  case AST_NODE_DOUBLE_REDIRECT:
    printf("[AST NODE DOUBLE REDIRECT]\n");
    break;
  case AST_NODE_REDIRECT:
    printf("[AST NODE REDIRECT]\n");
    break;
  case AST_NODE_SEQUENCE:
    printf("[AST NODE SEQUENCE]\n");
    break;
  case AST_NODE_EXCLAMATION:
    printf("[AST NODE EXCLAMATIN]\n");
    break;
  case AST_NODE_OR:
    printf("[AST NODE OR]\n");
    break;
  }
  if (ast->state == AST_NODE_COMMAND) {
    for (int i = 0; i < ast->argc; i++) {
      str_print(ast->argv[i]);
      printf("\n");
    }
  } else {
    if (ast->left != NULL) {
      printf("[LEFT NODE]\n");
      shell_AST_helper(ast->left);
    }
    if (ast->right != NULL) {
      printf("[RIGHT NODE]\n");
      shell_AST_helper(ast->right);
    }
  }

  if (ast->file_out.str != NULL) {
    printf("  [REDIRECT OUT]: ");
    str_print(ast->file_out);
    printf(ast->is_append ? " (APPEND)\n" : " (TRUNCATE)\n");
  }
  if (ast->file_in.str != NULL) {
    printf("  [REDIRECT IN]: ");
    str_print(ast->file_in);
    printf("\n");
  }
  if (ast->heredoc.str != NULL) {
    printf("  [HEREDOC DELIM]: ");
    str_print(ast->heredoc);
    printf("\n");
  }
  return;
}
#endif

shell_AST_t *shell_parser(mem_arena_t *arena, shell_token_list_t *list) {
  parser_ctx_t ctx = {.curr_token = list->head, .arena = arena};
  shell_AST_t *root = parsing_list(&ctx);
#ifdef DEBUG_MODE_PARSING
  shell_AST_helper(root);
#endif
  return root;
}
