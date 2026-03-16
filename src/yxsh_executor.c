#include "yxsh_internal.h"
#include <sys/wait.h>
#include <unistd.h>

typedef int (*buildin_func_t)(exe_ctx_t *ctx, shell_AST_t *ast);

static int buildin_exit(exe_ctx_t *ctx, shell_AST_t *ast) { exit(0); }
static int buildin_setenv(exe_ctx_t *ctx, shell_AST_t *ast) {
  if (ast->argc >= 3) {
    setenv(ast->argv[1].str, ast->argv[2].str, 1);
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

static void proc_status(exe_ctx_t *ctx, int status) {
  if (WIFEXITED(status)) {
    ctx->status->exit_status = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    ctx->status->exit_status = 128 + WTERMSIG(status);
  } else {
    ctx->status->exit_status = -1;
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
      ctx->status->exit_status = buildin[i].func(ctx, ast);
      return ctx->status->exit_status;
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
    return ctx->status->exit_status;
  }
  perror("fork error");
  return -1;
}

static int exe_num_pipe(exe_ctx_t *ctx, shell_AST_t *ast) {
  int pipefd[2];
  pipe(pipefd);

  pid_t child_pid = fork();
  if (child_pid == -1) {
    perror("fork error");
    return -1;
  } else if (child_pid == 0) {
    if (ctx->status->pipe_buffer[ctx->status->command_counter % NUM_PIPE_MAX] ==
        -1) {
      dup2(pipefd[0],
           ctx->status
               ->pipe_buffer[ctx->status->command_counter % NUM_PIPE_MAX]);
    }
    dup2(pipefd[1], STDOUT_FILENO);
    ctx->status->pipe_buffer[ctx->status->command_counter % NUM_PIPE_MAX] =
        pipefd[1];
    close(pipefd[0]);
    exit(execute_ast(ctx, ast));
  }
  close(pipefd[0]);
  close(pipefd[1]);
  int child_status;
  waitpid(child_pid, &child_status, 0);
  proc_status(ctx, child_status);
  return ctx->status->exit_status;
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
  return ctx->status->exit_status;
}

static int exe_and_or(exe_ctx_t *ctx, shell_AST_t *ast) {
  int left_status = execute_ast(ctx, ast->left);
  if (ast->state == AST_NODE_DOUBLE_AND && left_status != 0) {
    return left_status;
  }
  if (ast->state == AST_NODE_OR && left_status == 0) {
    return left_status;
  }
  if (ast->right == NULL) {
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
    return ctx->status->exit_status;
  } else if (ast->state == AST_NODE_AND) {
    pid_t bg_pid = fork();
    if (bg_pid == 0) {
      if (ast->left->file_in.str == NULL) {
        ast->left->file_in = str_new_static_in(*ctx->arena, "/dev/null");
      }
      exit(execute_ast(ctx, ast->left));
    } else if (bg_pid > 0) {
      printf("[1] %d\n", bg_pid);
      ctx->status->exit_status = 0;
      return 0;
    }
  }
  return ctx->status->exit_status;
}

static int execute_ast(exe_ctx_t *ctx, shell_AST_t *root) {
  switch (root->state) {
  case AST_NODE_COMMAND:
    return exe_command(ctx, root);
  case AST_NODE_PIPE:
    return exe_pipe(ctx, root);
  case AST_NODE_NUMBER_PIPE:
    return exe_num_pipe(ctx, root);
  case AST_NODE_OR:
  case AST_NODE_DOUBLE_AND:
    return exe_and_or(ctx, root);
  case AST_NODE_SEQUENCE:
  case AST_NODE_AND:
    return exe_list(ctx, root);
  default:
    break;
  }
  return ctx->status->exit_status;
}

int shell_executor(mem_arena_t *arena, shell_AST_t *root,
                   command_status_t *prev_status) {
  exe_ctx_t ctx = {.arena = arena, .status = prev_status};
  if (root == NULL)
    return -1;
  return execute_ast(&ctx, root);
}
