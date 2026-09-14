#ifndef CETRA_TEST_PRIVATE_FILES_H
#define CETRA_TEST_PRIVATE_FILES_H
#include <stdarg.h>

/* Filesystem seams for mocked owners; never allow production runtime paths. */
static inline void private_path(const char *path) {
  const char *root = getenv("XDG_RUNTIME_DIR");
  if (!root || strncmp(root, "/tmp/opencode/", 14) || strstr(path, "/../")
      || strncmp(path, root, strlen(root))
      || (path[strlen(root)] && path[strlen(root)] != '/')) abort();
}
static inline void private_fd(int fd) {
  char proc[64], path[1024];
  snprintf(proc, sizeof(proc), "/proc/self/fd/%d", fd);
  ssize_t size = readlink(proc, path, sizeof(path) - 1);
  if (size < 0 || (size_t)size >= sizeof(path) - 1) abort();
  path[size] = '\0';
  private_path(path);
}
static inline int private_open(const char *path, int flags, ...) {
  private_path(path);
  mode_t mode = 0;
  if (flags & O_CREAT) {
    va_list args;
    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);
  }
  return open(path, flags, mode);
}
static inline int private_openat(int fd, const char *name, int flags, mode_t mode) {
  private_fd(fd);
  if (strchr(name, '/') || !strcmp(name, "..")) abort();
  return openat(fd, name, flags, mode);
}
static inline int private_mkostemp(char *path, int flags) {
  private_path(path);
  return mkostemp(path, flags);
}
static inline FILE *private_fdopen(int fd, const char *mode) {
  private_fd(fd);
  return fdopen(fd, mode);
}
static inline int private_close(int fd) {
  private_fd(fd);
  return close(fd);
}
#endif
