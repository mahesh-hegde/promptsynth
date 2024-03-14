#include <git2.h>
#include <stdio.h>

#include "promptsynth.h"

#define COLOR_PARAM "\e[%sm"
#define COLOR_RESET "\e[39;49m"
#define ALL_RESET "\e[0m"

#define UP_ARROW "\u2191"
#define DOWN_ARROW "\u2193"
#define CONGRUNET "\u2261"
#define STASH_FLAG "\u2691"

typedef struct ps_options {
  int use_bold_colors, show_stash;
  const char* branchname_color;
  const char* hash_color;
  const char* staged_color;
  const char* unstaged_color;
  const char* stash_color;
  const char* conflict_color;
  const char* remote_status_color;
  const char *stash_symbol, *conflict_symbol;
  const char *prompt_prefix, *prompt_suffix, *seperator;
} ps_options;

// optional configuration
const char* get_env_str(const char* env_name, const char* fallback) {
  const char* env_val;
  env_val = getenv(env_name);
  if (env_val == NULL) {
    return fallback;
  }
  return env_val;
}

int get_env_int(const char* env_name, int fallback) {
  const char* env_val;
  int result;
  env_val = getenv(env_name);
  if (env_val == NULL) {
    return fallback;
  }
  if (sscanf(env_val, "%d", &result) == 1) {
    return result;
  }
  return fallback;
}

void init_options_from_env(ps_options* options) {
  options->use_bold_colors = get_env_int("PROMPTSYNTH_BOLD_COLORS", 0);
  options->show_stash = get_env_int("PROMPTSYNTH_SHOW_STASH", 0);
  options->branchname_color =
      get_env_str("PROMPTSYNTH_BRANCHNAME_COLOR", ANSI_CYAN);
  options->hash_color = get_env_str("PROMPTSYNTH_HASH_COLOR", ANSI_CYAN);
  options->staged_color = get_env_str("PROMPTSYNTH_STAGED_COLOR", ANSI_GREEN);
  options->unstaged_color =
      get_env_str("PROMPTSYNTH_UNSTAGED_COLOR", ANSI_YELLOW);
  options->stash_color = get_env_str("PROMPTSYNTH_STASH_COLOR", ANSI_MAGENTA);
  options->conflict_color = get_env_str("PROMPTSYNTH_CONFLICT_COLOR", ANSI_RED);
  options->remote_status_color =
      get_env_str("PROMPTSYNTH_REMOTE_STATUS_COLOR", ANSI_WHITE);
  options->prompt_prefix = get_env_str("PROMPTSYNTH_PROMPT_PREFIX", "[");
  options->prompt_suffix = get_env_str("PROMPTSYNTH_PROMPT_SUFFIX", "]");
  options->seperator = get_env_str("PROMPTSYNTH_SEPERATOR", "|");
  options->conflict_symbol = get_env_str("PROMPTSYNTH_CONFLICT_SYMBOL", "?");
  options->stash_symbol = get_env_str("PROMPTSYNTH_STASH_SYMBOL", STASH_FLAG);
}

void ps_print(ps_options* options, ps_state* state) {
  // TODO: Remote status
  // For ease of formatting
  const char* staged_string = NULL;
  const char* unstaged_string = NULL;
  const char* conflicted_string = NULL;
  printf("%s%s" COLOR_PARAM "%s" COLOR_RESET,
         options->use_bold_colors ? "\e[1m" : "", options->prompt_prefix,
         state->is_hash ? options->hash_color : options->branchname_color,
         state->branch_name);

  // if not local branch, print ahead-behind info
  if (state->has_upstream) {
    if (state->ahead_by == 0 && state->behind_by == 0) {
      printf(" " CONGRUNET);
    } else {
      printf(" ");
      if (state->ahead_by != 0) {
        printf(UP_ARROW "%d", state->ahead_by);
      }
      if (state->behind_by != 0) {
        printf(DOWN_ARROW "%d", state->behind_by);
      }
    }
  }

  if (state->staged.added != 0 || state->staged.modified != 0 ||
      state->staged.deleted != 0) {
    printf(" %s " COLOR_PARAM "+%d ~%d -%d" COLOR_RESET, options->seperator,
           options->staged_color, state->staged.added, state->staged.modified,
           state->staged.deleted);
  }
  if (state->unstaged.added != 0 || state->unstaged.modified != 0 ||
      state->unstaged.deleted != 0) {
    printf(" %s " COLOR_PARAM "+%d ~%d -%d" COLOR_RESET, options->seperator,
           options->unstaged_color, state->unstaged.added,
           state->unstaged.modified, state->unstaged.deleted);
  }
  if (state->conflicted != 0) {
    printf(" %s " COLOR_PARAM "%s%d" COLOR_RESET, options->seperator,
           options->conflict_color, options->conflict_symbol,
           state->conflicted);
  }
  if (state->stashes != 0 && options->show_stash) {
    printf(" %s " COLOR_PARAM
           "%s"
           "%d" COLOR_RESET,
           options->seperator, options->stash_color, options->stash_symbol,
           state->stashes);
  }
  printf("%s" ALL_RESET, options->prompt_suffix);
  fflush(stdout);
}

int main(int argc, char** argv) {
  ps_state state = {0};
  ps_options options = {0};
  init_options_from_env(&options);
  git_libgit2_init();
  int result = compute_repo_state(".", &state);
  if (result == 0) {
    ps_print(&options, &state);
  }
  git_libgit2_shutdown();
}