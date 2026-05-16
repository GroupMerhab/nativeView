/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "nv_util.h"

static int test_err_writes_stderr(void) {
  int fds[2];
  if (pipe(fds) != 0) return 1;
  fflush(stderr);
  int old_stderr = dup(STDERR_FILENO);
  if (dup2(fds[1], STDERR_FILENO) < 0) return 1;
  close(fds[1]);
  NV_ERR("hello %s", "world");
  fflush(stderr);
  char buf[256];
  ssize_t n = read(fds[0], buf, sizeof(buf)-1);
  buf[n > 0 ? n : 0] = 0;
  dup2(old_stderr, STDERR_FILENO);
  close(old_stderr);
  close(fds[0]);
  return (n > 0 && strstr(buf, "[NV ERROR") && strstr(buf, "hello world")) ? 0 : 1;
}

static int test_assert_true_does_not_abort(void) {
  NV_ASSERT(1 == 1);
  return 0;
}

static int test_assert_fail_aborts_posix(void) {
#ifdef _WIN32
  return 0;
#else
  pid_t pid = fork();
  if (pid < 0) return 1;
  if (pid == 0) {
    nv_assert_fail("0", __FILE__, __LINE__);
    _exit(0);
  }
  int status = 0;
  if (waitpid(pid, &status, 0) < 0) return 1;
  if (WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT) return 0;
  return 1;
#endif
}

int main(void) {
  int fails = 0;
  fails += test_err_writes_stderr();
  fails += test_assert_true_does_not_abort();
  fails += test_assert_fail_aborts_posix();
  if (fails) {
    fprintf(stderr, "test_log: %d failure(s)\n", fails);
    return 1;
  }
  printf("[PASS] test_log\n");
  return 0;
}
