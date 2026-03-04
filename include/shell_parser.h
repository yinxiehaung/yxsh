#ifndef SHELL_PARSER
#define SHELL_PARSER
#include "my_arena.h"
#include "my_string.h"

#define INIT_TOKEN_LIST
#define INIT_AST

typedef struct shell_token_s shell_token_t;
typedef struct shell_AST_s shell_AST_t;
typedef struct shell_token_list_s shell_token_list_t;

void shell_tokenize(mem_arena_t *, shell_token_list_t *, string_t *);
void shell_parser(mem_arena_t *, shell_AST_t *, shell_token_list_t *);

#endif
