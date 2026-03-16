#include "yxsh_internal.h"

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
string_t shell_expand(exe_ctx_t *exe_ctx, string_t *orig_str) {
  if (orig_str->str == NULL || orig_str->len == 0) {
    return INIT_STRING;
  }
  exp_ctx_t ctx = {.arena = exe_ctx->arena,
                   .exit_status = exe_ctx->status->exit_status,
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
