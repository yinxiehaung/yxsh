#ifndef YXSH_INTERNAL_H
#define YXSH_INTERNAL_H
#include "../include/yxsh_core.h"
#include <ctype.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHAR_IN_STR(c, s) ((c) - (*(s)).str < ((*(s)).len))

typedef struct shell_token_s shell_token_t;
typedef struct shell_AST_s shell_AST_t;
typedef struct shell_token_list_s shell_token_list_t;
typedef struct lexer_ctx_s lexer_ctx_t;
typedef struct parser_ctx_s parser_ctx_t;
typedef struct exp_ctx_s exp_ctx_t;
typedef struct exe_ctx_s exe_ctx_t;
typedef bool (*lex_rule_fnp_t)(char c);

enum shell_token_type_e {
  TOKEN_START,
  TOKEN_SYMBOL,
  TOKEN_WORD,
  TOKEN_SINGLE_QUOTE,
  TOKEN_DOUBLE_QUOTE,
  TOKEN_COMMENT,
  TOKEN_ILLEGAL
};

enum shell_AST_type_e {
  AST_NODE_COMMAND,
  AST_NODE_PIPE,            // |
  AST_NODE_NUMBER_PIPE,     // |n
  AST_NODE_SEQUENCE,        // ;
  AST_NODE_REDIRECT,        // > <
  AST_NODE_DOUBLE_REDIRECT, // >> <<
  AST_NODE_AND,             // &
  AST_NODE_OR,              // ||
  AST_NODE_DOUBLE_AND       // &&
};

typedef enum shell_token_type_e shell_token_type_t;
typedef enum shell_AST_type_e shell_AST_type_t;

struct shell_token_s {
  string_t key;
  shell_token_type_t token_state;
  shell_token_t *next;
};

struct shell_token_list_s {
  shell_token_t *head;
  shell_token_t *tail;
};

struct shell_AST_s {
  shell_AST_type_t state;
  shell_AST_t *left;
  shell_AST_t *right;
  string_t *argv;
  ui64 argc;
  ui64 pipe_num;
  string_t file_in;
  string_t file_out;
  string_t heredoc;
  bool is_append;
};

struct lexer_ctx_s {
  string_t *source;
  char *cursor;
  char *token_start;
  shell_token_type_t state;
  shell_token_list_t *list;
  mem_arena_t *arena;
};

struct parser_ctx_s {
  shell_token_t *curr_token;
  mem_arena_t *arena;
};

struct exp_ctx_s {
  mem_arena_t *arena;
  int exit_status;
  char *cursor;
  bool in_double_quote;
  bool in_single_quote;
  string_t *orig_str;
  string_t res;
};

extern char **environ;
shell_token_list_t *shell_tokenize(mem_arena_t *, string_t *);
shell_AST_t *shell_parser(mem_arena_t *, shell_token_list_t *);
string_t shell_expand(shell_ctx_t *, string_t *);
int shell_executor(shell_AST_t *, shell_ctx_t *);
#endif
