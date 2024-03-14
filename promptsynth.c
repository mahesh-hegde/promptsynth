#include "promptsynth.h"

#include <assert.h>
#include <git2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/// libgit2 returns null if the branch is new and has no commits. In this case,
/// HEAD will point to a ref which does not exist as a file inside .git/. Since
/// I don't know a better API, I am directly reading the file to infer branch
/// name.
const char* read_unborn_branch_name(const char* repo_root, int is_bare_repo) {
  char buf[256];
  char* path_to_head;
  asprintf(&path_to_head, "%s/HEAD", repo_root);
  FILE* fp = fopen(path_to_head, "r");
  if (fp == NULL) {
    perror("cannot read name of unborn branch");
    exit(1);
  }
  int n_read = fscanf(fp, "ref: refs/heads/%255s\n", buf);
  fclose(fp);
  if (path_to_head != NULL) {
    free(path_to_head);
  }
  if (n_read == 0) {  // should not happen
    fprintf(stderr, "promptsynth: cannot read name of unborn branch\n");
  }
  // Technically a memory leak, but that's fine because program exits.
  return strdup(buf);
}

/// @brief Prints detailed info about last git error, along with given context
/// string.
void fatal_with_git_error(int error_code, const char* context) {
  const git_error* e = git_error_last();
  fprintf(stderr, "Error (%s) %d/%d: %s\n", context, error_code, e->klass,
          e->message);
  exit(error_code);
}

void handle_git_error(int error_code, const char* context) {
  if (error_code < 0) {
    fatal_with_git_error(error_code, context);
  }
}

void get_ahead_behind(git_repository* repo,
                      git_reference* head,
                      ps_state* state) {
  git_reference* upstream = NULL;
  if (head == NULL || !git_reference_is_branch(head)) {
    return;
  }
  int upstream_res = git_branch_upstream(&upstream, head);
  if (upstream_res == GIT_ENOTFOUND) {
    state->has_upstream = 0;
  } else if (upstream_res == 0) {
    size_t ahead = 0, behind = 0;
    git_reference *local_peeled, *upstream_peeled;
    const git_oid *local_oid, *upstream_oid;

    // get OIDs for comparison
    handle_git_error(git_reference_resolve(&local_peeled, head),
                     "resolve_local");
    handle_git_error(git_reference_resolve(&upstream_peeled, upstream),
                     "resolve_upstream");
    local_oid = git_reference_target(local_peeled);
    assert(local_oid != NULL);
    upstream_oid = git_reference_target(upstream_peeled);
    assert(upstream_oid != NULL);

    // Call the API and set values
    git_graph_ahead_behind(&ahead, &behind, repo, local_oid, upstream_oid);
    state->ahead_by = ahead;
    state->behind_by = behind;
    state->has_upstream = 1;
    git_reference_free(local_peeled);
    git_reference_free(upstream_peeled);
  } else {
    fatal_with_git_error(upstream_res, "get_upstream");
  }
  if (upstream != NULL)
    git_reference_free(upstream);
}

typedef struct callback_context {
  ps_state* state;
  git_index* index;
} callback_context;

int stash_callback(size_t index,
                   const char* message,
                   const git_oid* stash_id,
                   void* payload) {
  ps_state* state = (ps_state*)payload;
  state->stashes++;
  return 0;
}

int status_callback(const char* path,
                    unsigned int status_flags,
                    void* payload) {
  callback_context* context = (callback_context*)payload;
  ps_state* state = context->state;
  git_index* index = context->index;
  // collect staged changes
  if (status_flags & GIT_STATUS_INDEX_NEW) {
    state->staged.added += 1;
  } else if (status_flags &
             (GIT_STATUS_INDEX_MODIFIED | GIT_STATUS_INDEX_TYPECHANGE |
              GIT_STATUS_INDEX_RENAMED)) {
    state->staged.modified += 1;
  } else if (status_flags & GIT_STATUS_INDEX_DELETED) {
    state->staged.deleted += 1;
  }
  // collect unstaged changes - copy paste
  if (status_flags & GIT_STATUS_WT_NEW) {
    state->unstaged.added += 1;
  } else if (status_flags & (GIT_STATUS_WT_MODIFIED | GIT_STATUS_WT_TYPECHANGE |
                             GIT_STATUS_WT_RENAMED)) {
    state->unstaged.modified += 1;
  } else if (status_flags & GIT_STATUS_WT_DELETED) {
    state->unstaged.deleted += 1;
  }

  if (status_flags & GIT_STATUS_CONFLICTED) {
    // TODO: Get conflict type
    // Is it even possible using libgit2?
    state->conflicted += 1;
  }
  return 0;
}

/// compute_repo_state computes the state of the repo, returns PS_ENOTAREPO if
/// the path is not a git repo
int compute_repo_state(const char* path, ps_state* state) {
  memset((void*)state, 0, sizeof(ps_state));
  git_status_options opts = GIT_STATUS_OPTIONS_INIT;
  // TODO: Do we need GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR?
  // I believe `git` doesnt detect such renames, it will rather show up as a
  // delete and an add.
  opts.flags |= GIT_STATUS_OPT_INCLUDE_UNTRACKED |
                GIT_STATUS_OPT_EXCLUDE_SUBMODULES |
                GIT_STATUS_OPT_RENAMES_FROM_REWRITES |
                GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX;
  git_repository* repo = NULL;
  git_buf repo_root = {0};
  git_reference* head = NULL;
  git_index* index = NULL;
  callback_context context;
  char hash_buf[8] = {0};

  // find git repo
  int discover_result = git_repository_discover(&repo_root, path, 0, NULL);
  if (discover_result != 0) {
    return PS_ENOTAREPO;
  }

  handle_git_error(git_repository_open(&repo, repo_root.ptr), "open_repo");

  // get branch name
  int head_status = git_repository_head(&head, repo);
  if (head_status != 0 && head_status != GIT_EUNBORNBRANCH &&
      head_status != GIT_ENOTFOUND) {
    const git_error* e = git_error_last();
    fprintf(stderr, "Error (%s) %d/%d: %s\n", "read_head", head_status,
            e->klass, e->message);
    state->branch_name = NULL;
  } else if (head_status == GIT_ENOTFOUND) {
    state->branch_name = ":head_not_found";
  } else if (head_status == GIT_EUNBORNBRANCH) {
    state->branch_name =
        read_unborn_branch_name(repo_root.ptr, git_repository_is_bare(repo));
  } else if (git_reference_is_branch(head)) {
    state->branch_name =
        strndup(git_reference_shorthand(head), MAX_CHARS_IN_REF_SHORTHAND);
  } else {
    state->is_hash = 1;
    // compute branch name from oid
    git_oid oid = {0};
    handle_git_error(git_reference_name_to_id(&oid, repo, "HEAD"),
                     "git_reference_name_to_id");
    git_oid_tostr(hash_buf, 8, &oid);
    asprintf((char**)&state->branch_name, ":%s", hash_buf);
  }
  // get ahead-behind info
  get_ahead_behind(repo, head, state);

  // get number of stash entries
  git_stash_foreach(repo, stash_callback, (void*)state);

  // count the numbers
  context.state = state;
  context.index = index;
  handle_git_error(
      git_status_foreach_ext(repo, &opts, status_callback, &context),
      "status_foreach");
  git_index_free(index);
  git_reference_free(head);
  git_repository_free(repo);
  git_buf_dispose(&repo_root);

  return 0;
}

void debug_print_repo_state(ps_state* state) {
  printf("unstaged={+%d ~%d -%d}\n", state->unstaged.added,
         state->unstaged.modified, state->unstaged.deleted);
  printf("staged={+%d ~%d -%d}\n", state->staged.added, state->staged.modified,
         state->staged.deleted);
  printf("conflicted=%d\n", state->conflicted);
  printf("branchname=%s\n", state->branch_name);
  printf("is_hash=%d\n", state->is_hash);
}
