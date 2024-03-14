#pragma once

#define ANSI_RED "31"
#define ANSI_RED_BOLD "31;1"
#define ANSI_GREEN "32"
#define ANSI_GREEN_BOLD "32;1"
#define ANSI_YELLOW "33"
#define ANSI_YELLOW_BOLD "33;1"
#define ANSI_BLUE "34"
#define ANSI_BLUE_BOLD "34;1"
#define ANSI_MAGENTA "35"
#define ANSI_MAGENTA_BOLD "35;1"
#define ANSI_CYAN "36"
#define ANSI_CYAN_BOLD "36;1"
#define ANSI_WHITE "37"
#define ANSI_WHITE_BOLD "37;1"

#define MAX_CHARS_IN_REF_SHORTHAND 32

#define PS_ENOTAREPO -16

typedef struct file_triplet {
  int modified;
  int added;
  int deleted;
} file_triplet;

typedef struct ps_state {
  struct file_triplet staged, unstaged;
  int ahead_by, behind_by, has_upstream;
  const char* branch_name;
  int is_hash;
  const char* conflict_type;
  int conflicted;
  int stashes;
} ps_state;

int compute_repo_state(const char* path, ps_state* state);

void debug_print_repo_state(ps_state* state);
