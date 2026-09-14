#define _GNU_SOURCE
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../private-files.h"

static int test_flock(int, int);
static FILE *test_fdopen(int, const char *);
static int test_fputs(const char *, FILE *);
static int test_fclose(FILE *);
static int test_rename(const char *, const char *);
static int test_fstat(int, struct stat *);
static int test_fchmod(int, mode_t);
static int test_mkostemp(char *, int);
#define main daemon_main
#define open private_open
#define openat private_openat
#define mkostemp test_mkostemp
#define fdopen test_fdopen
#define fputs test_fputs
#define fclose test_fclose
#define rename test_rename
#define fstat test_fstat
#define fchmod test_fchmod
#define flock test_flock
#include CETRA_SOURCE
#undef main
#undef open
#undef openat
#undef mkostemp
#undef fdopen
#undef fputs
#undef fclose
#undef rename
#undef fstat
#undef fchmod
#undef flock
#include "../security-hid.h"

static bool mirror_only, fail_stream, fail_write, fail_close, fail_rename, foreign_file;
static bool fail_chmod, fail_temp;
static int test_flock(int fd, int operation) {
  assert(!logging_enabled && (umask(0077) & 0777) == 0077);
  if (mirror_only) { errno = EWOULDBLOCK; return -1; }
  return flock(fd, operation);
}
static FILE *test_fdopen(int fd, const char *mode) {
  if (fail_stream) { errno = EMFILE; return NULL; }
  return private_fdopen(fd, mode);
}
static int test_fputs(const char *text, FILE *file) {
  if (fail_write) { errno = ENOSPC; return EOF; }
  return fputs(text, file);
}
static int test_fclose(FILE *file) {
  int result = fclose(file);
  if (fail_close) { errno = EIO; return EOF; }
  return result;
}
static int test_rename(const char *from, const char *to) {
  private_path(from); private_path(to);
  if (fail_rename) { errno = EACCES; return -1; }
  return rename(from, to);
}
static int test_fstat(int fd, struct stat *st) {
  int result = fstat(fd, st);
  if (!result && foreign_file && S_ISREG(st->st_mode)) st->st_uid = geteuid() + 1;
  return result;
}
static int test_fchmod(int fd, mode_t mode) {
  if (fail_chmod) { errno = EPERM; return -1; }
  return fchmod(fd, mode);
}
static int test_mkostemp(char *path, int flags) {
  if (fail_temp) { errno = EACCES; return -1; }
  return private_mkostemp(path, flags);
}

static void contents(const char *path, const char *expected) {
  char buffer[2048];
  FILE *file = fopen(path, "r");
  assert(file);
  size_t count = fread(buffer, 1, sizeof(buffer), file);
  assert(!ferror(file) && !fclose(file));
  assert(count == strlen(expected) && !memcmp(buffer, expected, count));
}
static void seed(const char *path, const char *text) {
  FILE *file = fopen(path, "w");
  assert(file && fputs(text, file) >= 0 && !fclose(file));
}
static void mode_is(const char *path, mode_t mode) {
  struct stat st;
  assert(!lstat(path, &st) && (st.st_mode & 0777) == mode);
}
static void absent(const char *path) { assert(access(path, F_OK) == -1 && errno == ENOENT); }
static void no_cache_temps(void) {
  DIR *dir = opendir(".");
  assert(dir);
  struct dirent *entry;
  while ((entry = readdir(dir))) {
    if (!strncmp(entry->d_name, "rog-cetra-control.status.", 25)) abort();
  }
  assert(!closedir(dir));
}

int main(void) {
  const char *root = getenv("XDG_RUNTIME_DIR");
  private_path(root);
  mode_is(root, 0700);
  char bad_state[512];
  assert(snprintf(bad_state, sizeof(bad_state), "%s/missing/parent/state", root) < (int)sizeof(bad_state));
  assert(!mkdir("omarchy", 0700));
  assert(!setenv("XDG_STATE_HOME", root, 1));

  char *unknown[] = {"cetra-watch", "--unsolicited", NULL};
  assert(!setenv("CETRA_STATUS_FIXTURE", "must-not-run", 1));
  assert(daemon_main(2, unknown) == 1 && !logging_enabled && !hid_inits);
  absent("omarchy/rog-cetra-control.log");
  absent("rog-cetra-control.owner.lock");
  assert(!unsetenv("CETRA_STATUS_FIXTURE"));
  char *normal[] = {"cetra-watch", NULL};
  mirror_only = true;
  assert(daemon_main(1, normal) == 1 && !logging_enabled && !hid_inits);
  absent("omarchy/rog-cetra-control.log");
  mirror_only = false;
  assert(!setenv("CETRA_DIAGNOSTICS", "0", 1));
  assert(daemon_main(1, normal) == 1 && !logging_enabled && hid_inits == 1);
  absent("omarchy/rog-cetra-control.log");
  hid_inits = 0;
  assert(!unsetenv("CETRA_DIAGNOSTICS"));
  assert(daemon_main(1, normal) == 1 && logging_enabled && hid_inits == 1);
  mode_is("omarchy/rog-cetra-control.log", 0600);
  assert(!unlink("omarchy/rog-cetra-control.log"));

  umask(0022);
  seed("omarchy/rog-cetra-control.log", "existing\n");
  assert(!chmod("omarchy/rog-cetra-control.log", 0644));
  log_event("owner append");
  mode_is("omarchy/rog-cetra-control.log", 0600);
  FILE *file = open_log_file();
  assert(file);
  int fd = fileno(file);
  assert((fcntl(fd, F_GETFD) & FD_CLOEXEC) && (fcntl(fd, F_GETFL) & O_NONBLOCK)
         && (fcntl(fd, F_GETFL) & O_APPEND));
  assert(!fclose(file));
  seed("victim", "untouched\n");
  assert(!chmod("victim", 0644));

  /* Disable fallback with a non-private (but otherwise trusted) runtime root. */
  assert(!chmod(root, 0755));
  assert(!unlink("omarchy/rog-cetra-control.log"));
  assert(!symlink("../victim", "omarchy/rog-cetra-control.log"));
  assert(!open_log_file());
  assert(!unlink("omarchy/rog-cetra-control.log"));
  assert(!link("victim", "omarchy/rog-cetra-control.log"));
  assert(!open_log_file());
  assert(!unlink("omarchy/rog-cetra-control.log"));
  assert(!mkfifo("omarchy/rog-cetra-control.log", 0600));
  assert(!open_log_file());
  assert(!unlink("omarchy/rog-cetra-control.log"));
  assert(!mkdir("omarchy/rog-cetra-control.log", 0700));
  assert(!open_log_file());
  assert(!rmdir("omarchy/rog-cetra-control.log"));
  seed("omarchy/rog-cetra-control.log", "foreign\n");
  foreign_file = true;
  assert(!open_log_file());
  foreign_file = false;
  contents("omarchy/rog-cetra-control.log", "foreign\n");
  fail_chmod = true;
  assert(!open_log_file());
  fail_chmod = false;
  contents("omarchy/rog-cetra-control.log", "foreign\n");
  assert(!unlink("omarchy/rog-cetra-control.log"));
  for (int mode = 0770; mode <= 0777; mode += 7) {
    assert(!chmod("omarchy", mode));
    assert(!open_log_file());
    mode_is("omarchy", mode);
  }
  assert(!chmod("omarchy", 0700));
  assert(!rename("omarchy", "real-parent"));
  assert(!symlink("real-parent", "omarchy"));
  assert(!open_log_file());
  assert(!unlink("omarchy") && !rename("real-parent", "omarchy"));
  contents("victim", "untouched\n");
  mode_is("victim", 0644);
  absent("rog-cetra-control.log");

  /* Rotation must not follow the old destination, or discard it on failure. */
  seed("omarchy/rog-cetra-control.log", "rotate\n");
  assert(!truncate("omarchy/rog-cetra-control.log", 5 * 1024 * 1024 + 1));
  assert(!chmod("omarchy/rog-cetra-control.log", 0644));
  assert(!symlink("../victim", "omarchy/rog-cetra-control.log.old"));
  file = open_log_file();
  assert(file && !fclose(file));
  mode_is("omarchy/rog-cetra-control.log.old", 0600);
  mode_is("omarchy/rog-cetra-control.log", 0600);
  contents("victim", "untouched\n");
  assert(!unlink("omarchy/rog-cetra-control.log.old"));
  assert(!mkdir("omarchy/rog-cetra-control.log.old", 0700));
  assert(!truncate("omarchy/rog-cetra-control.log", 5 * 1024 * 1024 + 1));
  assert(!open_log_file());
  struct stat st;
  assert(!stat("omarchy/rog-cetra-control.log", &st) && st.st_size == 5 * 1024 * 1024 + 1);
  assert(!rmdir("omarchy/rog-cetra-control.log.old"));
  assert(!unlink("omarchy/rog-cetra-control.log"));

  assert(!setenv("XDG_STATE_HOME", bad_state, 1));
  assert(!open_log_file());
  assert(!chmod(root, 0700));
  file = open_log_file();
  assert(file && !fclose(file));
  mode_is("rog-cetra-control.log", 0600);
  assert(!unlink("rog-cetra-control.log"));
  assert(!symlink("omarchy", "runtime-link"));
  char linked_runtime[512];
  assert(snprintf(linked_runtime, sizeof(linked_runtime), "%s/runtime-link/", root) < (int)sizeof(linked_runtime));
  assert(!setenv("XDG_RUNTIME_DIR", linked_runtime, 1));
  assert(!open_log_file());
  assert(!setenv("XDG_RUNTIME_DIR", root, 1));
  assert(!unlink("runtime-link"));
  /* Missing leaf and state directory may be created, existing parents stay intact. */
  assert(!setenv("XDG_STATE_HOME", root, 1));
  assert(!rmdir("omarchy"));
  file = open_log_file();
  assert(file && !fclose(file));
  mode_is("omarchy", 0700);
  assert(!unlink("omarchy/rog-cetra-control.log"));
  assert(!rmdir("omarchy"));
  char nested_state[512];
  assert(snprintf(nested_state, sizeof(nested_state), "%s/new-state", root) < (int)sizeof(nested_state));
  assert(!setenv("XDG_STATE_HOME", nested_state, 1));
  file = open_log_file();
  assert(file && !fclose(file));
  mode_is("new-state", 0700);
  mode_is("new-state/omarchy", 0700);
  char long_path[1024];
  memset(long_path, 'x', sizeof(long_path) - 1); long_path[sizeof(long_path) - 1] = 0;
  assert(!setenv("XDG_STATE_HOME", long_path, 1));
  char path[512];
  assert(!log_path(path, sizeof(path)));
  assert(!setenv("XDG_STATE_HOME", root, 1));

  /* The cache replaces the name, never opens a pre-existing predictable temp. */
  char predictable[128];
  snprintf(predictable, sizeof(predictable), "rog-cetra-control.status.%ld", (long)getpid());
  assert(!symlink("victim", predictable));
  assert(!symlink("victim", "rog-cetra-control.status"));
  write_state_cache("{\"old\":true}\n");
  contents("rog-cetra-control.status", "{\"old\":true}\n");
  mode_is("rog-cetra-control.status", 0600);
  contents("victim", "untouched\n");
  assert(!unlink(predictable));
  bool *failures[] = {&fail_temp, &fail_chmod, &fail_stream, &fail_write, &fail_close, &fail_rename};
  for (size_t i = 0; i < sizeof(failures) / sizeof(failures[0]); i++) {
    *failures[i] = true;
    write_state_cache("{\"new\":true}\n");
    *failures[i] = false;
    contents("rog-cetra-control.status", "{\"old\":true}\n");
    no_cache_temps();
  }
  write_state_cache("{\"new\":true}\n");
  contents("rog-cetra-control.status", "{\"new\":true}\n");
  assert(!setenv("XDG_RUNTIME_DIR", long_path, 1));
  assert(!runtime_path(path, sizeof(path), "rog-cetra-control.status"));
  write_state_cache("truncated path must not write");
  assert(!setenv("XDG_RUNTIME_DIR", root, 1));
  contents("rog-cetra-control.status", "{\"new\":true}\n");
  no_cache_temps();
  assert(!hid_outputs && !hid_queries);
  puts("Log/cache safety: ownership gate, private files, rejected links/FIFO/directories, rotation and cache failure preservation passed");
  return 0;
}
