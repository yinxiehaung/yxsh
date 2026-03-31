#include "yxsh_internal.h"

// #define DEBUG_MODE_PARSING

static shell_AST_t *parsing_command(parser_ctx_t *ctx) {
  shell_token_t *cursor = ctx->curr_token;
  if (cursor == NULL) {
    return NULL;
  }
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

static shell_AST_t *__parsing_number_pipe(parser_ctx_t *ctx,
                                          shell_AST_t *root) {
  char num[5] = {};
  memcpy(num, &ctx->curr_token->key.str[1], ctx->curr_token->key.len - 1);
  num[4] = '\0';
  root->pipe_num = atoi(num) % NUM_PIPE_MAX;
  root->state = AST_NODE_NUMBER_PIPE;
  return root;
}

static shell_AST_t *parsing_pipe(parser_ctx_t *ctx) {
  shell_AST_t *root = parsing_command(ctx);
  if (root == NULL)
    return NULL;
  if (ctx->curr_token != NULL && ctx->curr_token->token_state == TOKEN_SYMBOL) {
    if (ctx->curr_token->key.len > 1 && ctx->curr_token->key.str[0] == '|' &&
        isdigit(ctx->curr_token->key.str[1])) {
      return __parsing_number_pipe(ctx, root);
    }
    while ((str_cmp_char(&ctx->curr_token->key, "|"))) {
      ctx->curr_token = ctx->curr_token->next;
      shell_AST_t *right = parsing_command(ctx);
      shell_AST_t *pipe_ast =
          arena_push_type(*ctx->arena, shell_AST_t, 1, NULL);
      pipe_ast->state = AST_NODE_PIPE;
      pipe_ast->left = root;
      pipe_ast->right = right;
      root = pipe_ast;
    }
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
  case AST_NODE_NUMBER_PIPE:
    printf("[AST NODE NUMBER PIPE]\n");
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
      printf("  [LEFT NODE]\n");
      shell_AST_helper(ast->left);
    }
    if (ast->right != NULL) {
      printf("  [RIGHT NODE]\n");
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
  if (ast->pipe_num != 0) {
    printf("  [PIPE NUMBER]: %lu\n", ast->pipe_num);
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
