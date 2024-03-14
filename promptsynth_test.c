#include "promptsynth.h"

#include <assert.h>
#include <git2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LEN(arr) (sizeof(arr) / sizeof(arr[0]))

#define TEMP_DIR_NAME "temp_dir_test"

const int TEST_SUCCESS = 0;
const int SETUP_FAILURE = -1;
const int TEST_FAILURE = -2;

#define PR_RED "\e[31m"
#define PR_GREEN "\e[32m"
#define PR_YELLOW "\e[33m"
#define PR_BLUE "\e[34m"
#define PR_RESET "\e[0m"

const char* push_temp_dir() {
  // TODO: Support windows
  // TODO: Use proper tempdir
  system("mkdir " TEMP_DIR_NAME);
  chdir(TEMP_DIR_NAME);
  return TEMP_DIR_NAME;
}

void pop_tmp_dir(const char* tmpdir) {
  // TODO: Support windows
  chdir("..");
  system("rm -rf " TEMP_DIR_NAME);
}

int run_all_commands(const char* commands[], int n) {
  for (int i = 0; i < n; i++) {
    int status = system(commands[i]);
    if (status != 0) {
      fprintf(stderr, "command failed: %s\n", commands[i]);
      return -1;
    }
  }
  return 0;
}

int compare_file_triplets(const char* field_name,
                          file_triplet expected,
                          file_triplet actual) {
  if (expected.added == actual.added && expected.modified == actual.modified &&
      expected.deleted == actual.deleted) {
    return 0;
  }
  fprintf(stderr, "%s: expected {+%d ~%d -%d}, got {+%d ~%d -%d}\n", field_name,
          expected.added, expected.modified, expected.deleted, actual.added,
          actual.modified, actual.deleted);
  return -1;
}

// -------------------------------------------------------
int test_staged_file_stats() {
  const char* commands[] = {
      "git init",
      "echo \'ABCD EFGH\' > test_1.txt",
      "echo \'ABCD EFGH\' > test_2.txt",
      "git add test_1.txt test_2.txt",
      "git commit -m \'Initial commit\'",
      "echo \'1234 651\' > other_1.txt",
      "echo \'1234 652\' > other_2.txt",
      "git rm test_1.txt",
      "echo \'ABCD EFHG\' > test_2.txt",
      "git add test_2.txt other_1.txt other_2.txt",
  };
  int result = TEST_SUCCESS;

  // setup
  const char* tmp_dir = push_temp_dir();
  int run_res = run_all_commands(commands, LEN(commands));
  if (run_res != 0) {
    result = SETUP_FAILURE;
    goto finish;
  }

  // test
  file_triplet expected = {.added = 2, .modified = 1, .deleted = 1};
  ps_state state = {0};
  compute_repo_state(".", &state);
  int cmp_res = compare_file_triplets("staged", expected, state.staged);
  if (cmp_res != 0) {
    result = TEST_FAILURE;
    goto finish;
  }

  // cleanup
finish:
  pop_tmp_dir(tmp_dir);
  return result;
}

// -------------------------------------------------------
int test_unstaged_file_stats() {
  const char* commands[] = {
      "git init",
      "echo \'ABCD EFGH\' > test_1.txt",
      "echo \'ABCD EFGH\' > test_2.txt",
      "echo \'ABCD EFGH\' > test_3.txt",
      "git add test_1.txt test_2.txt test_3.txt",
      "git commit -m \'Initial commit\'",
      "echo \'1234 651\' > other_1.txt",
      "rm test_1.txt",
      "echo \'ABCD EFHG\' > test_2.txt",
      "echo \'ABCD EFHG\' > test_3.txt",
  };
  int result = TEST_SUCCESS;

  // setup
  const char* tmp_dir = push_temp_dir();
  int run_res = run_all_commands(commands, LEN(commands));
  if (run_res != 0) {
    result = SETUP_FAILURE;
    goto finish;
  }

  // test
  file_triplet expected = {.added = 1, .modified = 2, .deleted = 1};
  ps_state state = {0};
  compute_repo_state(".", &state);
  int cmp_res = compare_file_triplets("unstaged", expected, state.unstaged);
  if (cmp_res != 0) {
    result = TEST_FAILURE;
    goto finish;
  }

  // cleanup
finish:
  pop_tmp_dir(tmp_dir);
  return result;
}

// -------------------------------------------------------
int test_branch_name() {
  const char* commands[] = {
      "git init",
      "git commit --allow-empty -m \'Initial commit\'",
      "git checkout -b other-branch",
  };
  int result = TEST_SUCCESS;

  // setup
  const char* tmp_dir = push_temp_dir();
  int run_res = run_all_commands(commands, LEN(commands));
  if (run_res != 0) {
    result = SETUP_FAILURE;
    goto finish;
  }

  // test
  ps_state state = {0};
  compute_repo_state(".", &state);
  if (strcmp(state.branch_name, "other-branch") != 0) {
    fprintf(stderr, "Expected branch_name = %s, got %s\n", "other-branch",
            state.branch_name);
    result = TEST_FAILURE;
    goto finish;
  }

  // cleanup
finish:
  pop_tmp_dir(tmp_dir);
  return result;
}

// -------------------------------------------------------
int test_unborn_branch_name() {
  const char* commands[] = {
      "git init",
      "git checkout -b other-branch",
  };
  int result = TEST_SUCCESS;

  // setup
  const char* tmp_dir = push_temp_dir();
  int run_res = run_all_commands(commands, LEN(commands));
  if (run_res != 0) {
    result = SETUP_FAILURE;
    goto finish;
  }

  // test
  ps_state state = {0};
  compute_repo_state(".", &state);
  if (strcmp(state.branch_name, "other-branch") != 0) {
    fprintf(stderr, "Expected branch_name = %s, got %s\n", "other-branch",
            state.branch_name);
    result = TEST_FAILURE;
    goto finish;
  }

  // cleanup
finish:
  pop_tmp_dir(tmp_dir);
  return result;
}

// -------------------------------------------------------
int test_count_stash_entries() {
  const char* commands[] = {
      "git init",        "git commit --allow-empty -m Commit1-For-Stash-Test",
      "echo ABC > abc1", "git add abc1 && git stash",
      "echo XYZ > xyz1", "git add xyz1 && git stash",
  };
  int result = TEST_SUCCESS;

  // setup
  const char* tmp_dir = push_temp_dir();
  int run_res = run_all_commands(commands, LEN(commands));
  if (run_res != 0) {
    result = SETUP_FAILURE;
    goto finish;
  }

  // test
  ps_state state = {0};
  compute_repo_state(".", &state);
  if (state.stashes != 2) {
    fprintf(stderr, "Expected stashes = %d, got %d\n", 2, state.stashes);
    result = TEST_FAILURE;
    goto finish;
  }

  // cleanup
finish:
  pop_tmp_dir(tmp_dir);
  return result;
}

// -------------------------------------------------------
int test_detached_head() {
  const char* commands[] = {
      "git init",
      "git commit --allow-empty -m Commit1",
      "git checkout $(git rev-parse HEAD)",
  };
  int result = TEST_SUCCESS;

  // setup
  const char* tmp_dir = push_temp_dir();
  int run_res = run_all_commands(commands, LEN(commands));
  if (run_res != 0) {
    result = SETUP_FAILURE;
    goto finish;
  }

  // test
  ps_state state = {0};
  compute_repo_state(".", &state);
  if (state.branch_name[0] != ':') {
    fprintf(stderr, "Expected hash prefixed with \':\'!");
    result = TEST_FAILURE;
    goto finish;
  }

  if (strlen(state.branch_name) != 8) {
    fprintf(stderr, "Expected strlen(branch_name) = %d, got branch_name = %s\n",
            8, state.branch_name);
    result = TEST_FAILURE;
    goto finish;
  }

  // cleanup
finish:
  pop_tmp_dir(tmp_dir);
  return result;
}

// -------------------------------------------------------
int test_rename_detection() {
  const char* commands[] = {
      "git init",
      "echo \'ABCD EFGH\' > abc.txt",
      "echo \'PQRS TUVW\' > pqr.txt",
      "git add abc.txt pqr.txt",
      "git commit -m Commit1",
      "git mv pqr.txt PQR",
      "cat abc.txt > ABC",
      "rm abc.txt",
      "git add .",
  };
  int result = TEST_SUCCESS;

  // setup
  const char* tmp_dir = push_temp_dir();
  int run_res = run_all_commands(commands, LEN(commands));
  if (run_res != 0) {
    result = SETUP_FAILURE;
    goto finish;
  }

  // test
  file_triplet expected_staged = {.added = 0, .modified = 2, .deleted = 0};
  ps_state state = {0};
  compute_repo_state(".", &state);
  int cmp_res = compare_file_triplets("staged", expected_staged, state.staged);
  if (cmp_res != 0) {
    result = TEST_FAILURE;
    goto finish;
  }

  // cleanup
finish:
  pop_tmp_dir(tmp_dir);
  return result;
}

// -------------------------------------------------------
int test_non_git_dir() {
  int result = TEST_SUCCESS;

  // setup
  char* wdir = getcwd(NULL, 512);
  chdir("/tmp");

  // test
  ps_state state = {0};
  int ps_result = compute_repo_state(".", &state);
  if (ps_result != PS_ENOTAREPO) {
    fprintf(stderr, "Expected PS_ENOTAREPO, got %d\n", result);
    goto finish;
  }

  // cleanup
finish:
  chdir(wdir);
  free(wdir);
  return result;
}

typedef int (*test_func)();

typedef struct test_case {
  test_func func;
  const char* name;
  int result;
} test_case;

int main(int argc, char** argv) {
  test_case test_cases[] = {
      {.func = test_staged_file_stats, .name = "staged file stats"},
      {.func = test_unstaged_file_stats, .name = "unstaged file stats"},
      {.func = test_branch_name, .name = "branch name"},
      {.func = test_unborn_branch_name, .name = "Unborn branch name"},
      {.func = test_count_stash_entries, .name = "Count stash entries"},
      {.func = test_detached_head, .name = "Test detached head"},
      {.func = test_non_git_dir, .name = "Test non-git dir"},
      {.func = test_rename_detection, .name = "Test rename detection"},
  };
  const char* legend[] = {"Passed", "Setup failure", "Test failure"};
  int counts[] = {0, 0, 0};
  git_libgit2_init();
  for (int i = 0; i < LEN(test_cases); i++) {
    test_case* tc = &test_cases[i];
    fprintf(stderr, "\e[33m%d. %s\e[0m:\n", i + 1, tc->name);
    tc->result = -tc->func();
    assert(tc->result >= 0);
    assert(tc->result < LEN(legend));
    counts[tc->result]++;
    fprintf(stderr, "%s\n", legend[tc->result]);
    fprintf(stderr, "-------------------------------------\n");
  }
  fprintf(stderr,
          PR_BLUE "Summary: Passed=" PR_RESET PR_GREEN "%d" PR_RESET PR_BLUE
                  ", SetupFailed=" PR_RESET PR_YELLOW "%d" PR_RESET PR_BLUE
                  ", TestFailed=" PR_RESET PR_RED "%d" PR_RESET "\n",
          counts[0], counts[1], counts[2]);
  git_libgit2_shutdown();
}
