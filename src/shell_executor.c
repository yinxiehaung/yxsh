#include "../include/shell.h"
#include <ctype.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

typedef int (*buildin_func_t)(exe_ctx_t *ctx, shell_AST_t *ast);

static char *str_to_cstr(mem_arena_t *arena, string_t *str) {
  char *c_str = arena_push_arr(*arena, char, str->len + 1, 1, NULL);
  memcpy(c_str, str->str, str->len);
  c_str[str->len] = '\0';
  return c_str;
}

static int buildin_exit(exe_ctx_t *ctx, shell_AST_t *ast) { exit(0); }
static int buildin_setenv(exe_ctx_t *ctx, shell_AST_t *ast) {
  if (ast->argc >= 3) {
    setenv(ast->argv[0].str, ast->argv[2].str, 1);
  }
  return 0;
}

static int buildin_printenv(exe_ctx_t *ctx, shell_AST_t *ast) {
  if (ast->argc == 1) {
    char **env = environ;
    for (ui64 i = 0; env[i] != NULL; i++) {
      printf("%s\n", env[i]);
    }
  } else if (ast->argc >= 2) {
    mem_tmp_arena_t tmp = arena_begin_tmp(ctx->arena);
    char *key = str_to_cstr(ctx->arena, &ast->argv[1]);
    if (key == NULL)
      return 0;
    char *s = getenv(key);
    if (s)
      printf("%s\n", s);
    arena_end_tmp(&tmp);
  }
  return 0;
}

static int buildin_cd(exe_ctx_t *ctx, shell_AST_t *ast) {
  if (ast->argc >= 2) {
    mem_tmp_arena_t tmp = arena_begin_tmp(ctx->arena);
    char *path = str_to_cstr(ctx->arena, &ast->argv[1]);
    chdir(path);
    arena_end_tmp(&tmp);
  }
  return 0;
}

struct {
  const char *name;
  buildin_func_t func;
} buildin[] = {{"cd", buildin_cd},
               {"setenv", buildin_setenv},
               {"printenv", buildin_printenv},
               {"exit", buildin_exit},
               {"quit", buildin_exit},
               {NULL, NULL}};

static int execute_ast(exe_ctx_t *ctx, shell_AST_t *ast);

static int str_execvp(mem_arena_t *arena, string_t *argv, ui64 argc) {
  char **c_argv = arena_push_arr(*arena, char *, argc + 1, 1, NULL);
  for (ui64 i = 0; i < argc; i++) {
    char *c_str = str_to_cstr(arena, &argv[i]);
    c_argv[i] = c_str;
  }
  c_argv[argc] = NULL;
  return execvp(c_argv[0], c_argv);
}

static void expand_tilde(exp_ctx_t *ctx) {
  char *s = ctx->cursor;
  if (s[0] == '~') {
    if (ctx->orig_str->len == 1 || s[1] == '/') {
      char *home = getenv("HOME");
      if (home)
        ctx->res = str_new_variable_in(*ctx->arena, home);
      s++;
    } else if (ctx->orig_str->len >= 2 && s[1] == '+' &&
               (ctx->orig_str->len == 2 || s[2] == '/')) {
      char *pwd = getenv("PWD");
      if (pwd)
        ctx->res = str_new_variable_in(*ctx->arena, pwd);
      s += 2;
    } else if (ctx->orig_str->len >= 2 && s[1] == '-' &&
               (ctx->orig_str->len == 2 || s[2] == '/')) {
      char *oldpwd = getenv("OLDPWD");
      if (oldpwd)
        ctx->res = str_new_variable_in(*ctx->arena, oldpwd);
      s += 2;
    }
  }
  ctx->cursor = s;
}

static void expand_dollar(exp_ctx_t *ctx) {
  ctx->cursor++;
  if (!CHAR_IN_STR(ctx->cursor, ctx->orig_str)) {
    str_cat_char_to_end_in(*ctx->arena, ctx->res, "$");
    return;
  }

  if (*ctx->cursor == '?') {
    char buf[16];
    sprintf(buf, "%d", ctx->exit_status);
    str_cat_char_to_end_in(*ctx->arena, ctx->res, buf);
    ctx->cursor++;
  } else if (isalpha(*ctx->cursor) || *ctx->cursor == '_') {
    char *curr = ctx->cursor;
    while (CHAR_IN_STR(curr, ctx->orig_str) &&
           (isalnum(*curr) || *curr == '_')) {
      curr++;
    }
    string_t str = {.str = ctx->cursor,
                    .alloc = 0,
                    .len = curr - ctx->cursor,
                    .is_arena = false};
    char *key = str_to_cstr(ctx->arena, &str);
    if (key != NULL) {
      char *val = getenv(key);
      if (val)
        str_cat_char_to_end_in(*ctx->arena, ctx->res, val);
    }
    ctx->cursor = curr;
  } else {
    str_cat_char_to_end_in(*ctx->arena, ctx->res, "$");
    char c[2] = {*ctx->cursor, '\0'};
    str_cat_char_to_end_in(*ctx->arena, ctx->res, c);
    ctx->cursor++;
  }
}
static string_t shell_expand(exe_ctx_t *exe_ctx, string_t *orig_str) {
  if (orig_str->str == NULL || orig_str->len == 0) {
    return INIT_STRING;
  }
  exp_ctx_t ctx = {.arena = exe_ctx->arena,
                   .exit_status = exe_ctx->exit_status,
                   .cursor = orig_str->str,
                   .in_double_quote = false,
                   .in_single_quote = false,
                   .orig_str = orig_str,
                   .res = str_new_static_in(*exe_ctx->arena, "")};
  expand_tilde(&ctx);
  while (CHAR_IN_STR(ctx.cursor, ctx.orig_str)) {
    char c[2] = {*ctx.cursor, '\0'};
    if (c[0] == '\'' && !ctx.in_double_quote) {
      ctx.in_single_quote = !ctx.in_single_quote;
      ctx.cursor++;
      continue;
    } else if (c[0] == '\"' && !ctx.in_single_quote) {
      ctx.in_double_quote = !ctx.in_double_quote;
      ctx.cursor++;
      continue;
    }
    if (c[0] == '$' && !ctx.in_single_quote) {
      expand_dollar(&ctx);
      continue;
    }
    str_cat_char_to_end_in(*ctx.arena, ctx.res, c);
    ctx.cursor++;
  }
  return ctx.res;
}

static void proc_status(exe_ctx_t *ctx, int status) {
  if (WIFEXITED(status)) {
    ctx->exit_status = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    ctx->exit_status = 128 + WTERMSIG(status);
  } else {
    ctx->exit_status = -1;
  }
}

static void __exe_command(exe_ctx_t *ctx, shell_AST_t *ast) {
  if (ast->file_in.str != NULL) {
    char *in_file = str_to_cstr(ctx->arena, &ast->file_in);
    int fd = open(in_file, O_RDONLY);
    if (fd == -1) {
      perror("yxsh:input file error\n");
      exit(1);
    }
    dup2(fd, STDIN_FILENO);
    close(fd);
  }
  if (ast->file_out.str != NULL) {
    char *out_file = str_to_cstr(ctx->arena, &ast->file_out);
    int flag = O_CREAT | O_WRONLY;
    if (ast->is_append) {
      flag |= O_APPEND;
    } else {
      flag |= O_TRUNC;
    }
    int fd = open(out_file, flag, 0644);
    if (fd == -1) {
      perror("yxsh: output file error\n");
      exit(1);
    }
    dup2(fd, STDOUT_FILENO);
    close(fd);
  }
  str_execvp(ctx->arena, ast->argv, ast->argc);
  fprintf(stderr, "yxsh: command not found\n");
  exit(127);
}

static int check_and_run_internal_command(exe_ctx_t *ctx, shell_AST_t *ast) {
  for (int i = 0; buildin[i].name != NULL; i++) {
    if (str_cmp_char(&ast->argv[0], buildin[i].name)) {
      ctx->exit_status = buildin[i].func(ctx, ast);
      return ctx->exit_status;
    }
  }
  return -1;
}

static int exe_command(exe_ctx_t *ctx, shell_AST_t *ast) {
  for (ui64 i = 0; i < ast->argc; i++) {
    ast->argv[i] = shell_expand(ctx, &ast->argv[i]);
  }
  if (check_and_run_internal_command(ctx, ast) == 0) {
    return 0;
  }
  pid_t child_pid = fork();
  if (child_pid == 0) {
    __exe_command(ctx, ast);
  } else if (child_pid > 0) {
    int status;
    waitpid(child_pid, &status, 0);
    proc_status(ctx, status);
    return ctx->exit_status;
  }
  perror("fork error");
  return -1;
}

static int exe_pipe(exe_ctx_t *ctx, shell_AST_t *ast) {
  int pipefd[2];
  pipe(pipefd);

  pid_t left_pid = fork();
  if (left_pid == -1) {
    perror("fork error");
    return -1;
  } else if (left_pid == 0) {
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[0]);
    close(pipefd[1]);
    exit(execute_ast(ctx, ast->left));
  }
  pid_t right_pid = fork();
  if (right_pid == -1) {
    perror("fork error");
    return -1;
  } else if (right_pid == 0) {
    dup2(pipefd[0], STDIN_FILENO);
    close(pipefd[0]);
    close(pipefd[1]);
    exit(execute_ast(ctx, ast->right));
  }

  close(pipefd[0]);
  close(pipefd[1]);

  int left_status, right_status;
  waitpid(left_pid, &left_status, 0);
  waitpid(right_pid, &right_status, 0);
  proc_status(ctx, right_status);
  return ctx->exit_status;
}

static int exe_and_or(exe_ctx_t *ctx, shell_AST_t *ast) {
  int left_status = execute_ast(ctx, ast->left);
  if (ast->state == AST_NODE_DOUBLE_AND && left_status != 0) {
    return left_status;
  }
  if (ast->state == AST_NODE_OR && left_status == 0) {
    return left_status;
  }
  return execute_ast(ctx, ast->right);
}

static int exe_list(exe_ctx_t *ctx, shell_AST_t *ast) {
  if (ast->state == AST_NODE_SEQUENCE) {
    execute_ast(ctx, ast->left);
    if (ast->right != NULL) {
      return execute_ast(ctx, ast->right);
    }
    return ctx->exit_status;
  } else if (ast->state == AST_NODE_AND) {
    pid_t bg_pid = fork();
    if (bg_pid == 0) {
      if (ast->left->file_in.str == NULL) {
        ast->left->file_in = str_new_static_in(*ctx->arena, "/dev/null");
      }
      exit(execute_ast(ctx, ast->left));
    } else if (bg_pid > 0) {
      printf("[1] %d\n", bg_pid);
      ctx->exit_status = 0;
      return 0;
    }
  }
  return ctx->exit_status;
}

static int execute_ast(exe_ctx_t *ctx, shell_AST_t *root) {
  switch (root->state) {
  case AST_NODE_COMMAND:
    return exe_command(ctx, root);
  case AST_NODE_PIPE:
    return exe_pipe(ctx, root);
  case AST_NODE_OR:
  case AST_NODE_DOUBLE_AND:
    return exe_and_or(ctx, root);
  case AST_NODE_SEQUENCE:
  case AST_NODE_AND:
    return exe_list(ctx, root);
  default:
    break;
  }
  return ctx->exit_status;
}

int shell_executor(mem_arena_t *arena, shell_AST_t *root, int prev_status) {
  exe_ctx_t ctx = {.arena = arena, .exit_status = prev_status};
  if (root == NULL)
    return -1;
  return execute_ast(&ctx, root);
}
