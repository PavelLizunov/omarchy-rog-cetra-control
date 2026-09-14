// Private runtime paths, telemetry and atomic status-cache publication.
static bool runtime_path(char *target, size_t size, const char *name) {
  const char *runtime = getenv("XDG_RUNTIME_DIR");
  if (!runtime || runtime[0] != '/') return false;
  int length = snprintf(target, size, "%s/%s", runtime, name);
  return length >= 0 && (size_t)length < size;
}

static bool logging_enabled = false;

static bool safe_ancestors(const char *path) {
  char prefix[512];
  size_t length = strlen(path);
  if (!length || length >= sizeof(prefix) || path[0] != '/') return false;
  memcpy(prefix, path, length + 1);
  for (size_t i = 1; i < length; i++) {
    if (prefix[i] != '/') continue;
    prefix[i] = '\0';
    struct stat st;
    bool ok = lstat(prefix, &st) == 0 && S_ISDIR(st.st_mode)
      && (st.st_uid == 0 || st.st_uid == geteuid())
      && (!(st.st_mode & 0022) || (st.st_uid == 0 && (st.st_mode & S_ISVTX)));
    prefix[i] = '/';
    if (!ok) return false;
  }
  return true;
}

static bool log_path(char *target, size_t size) {
  const char *state = getenv("XDG_STATE_HOME");
  int length;
  if (state && *state) {
    length = snprintf(target, size, "%s/omarchy/rog-cetra-control.log", state);
  } else {
    const char *home = getenv("HOME");
    if (!home || !*home) return false;
    length = snprintf(target, size, "%s/.local/state/omarchy/rog-cetra-control.log", home);
  }
  return length >= 0 && (size_t)length < size;
}

static int open_log_directory(const char *path, int create_depth, bool private) {
  struct stat before, after;
  if (lstat(path, &before) != 0) {
    if (errno != ENOENT || create_depth <= 0) return -1;
    char parent[512];
    int length = snprintf(parent, sizeof(parent), "%s", path);
    if (length < 0 || (size_t)length >= sizeof(parent)) return -1;
    char *slash = strrchr(parent, '/');
    if (!slash || slash == parent || !slash[1]) return -1;
    *slash = '\0';
    int parent_fd = open_log_directory(parent, create_depth - 1, false);
    if (parent_fd < 0) return -1;
    int result = mkdirat(parent_fd, slash + 1, 0700);
    int error = errno;
    close(parent_fd);
    if (result != 0 && error != EEXIST) return -1;
    if (lstat(path, &before) != 0) return -1;
  }
  if (!safe_ancestors(path) || !S_ISDIR(before.st_mode) || before.st_uid != geteuid()
      || (before.st_mode & 0022) || (private && (before.st_mode & 0777) != 0700)) return -1;
  int fd = open(path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
  if (fd < 0) return -1;
  if (fstat(fd, &after) != 0 || !S_ISDIR(after.st_mode) || after.st_uid != geteuid()
      || (after.st_mode & 0022) || (private && (after.st_mode & 0777) != 0700)
      || before.st_dev != after.st_dev || before.st_ino != after.st_ino) {
    close(fd);
    return -1;
  }
  return fd;
}

static FILE *open_log_file(void) {
  for (int fallback = 0; fallback < 2; fallback++) {
    char path[512];
    if (fallback ? !runtime_path(path, sizeof(path), "rog-cetra-control.log")
                 : !log_path(path, sizeof(path))) continue;
    char *slash = strrchr(path, '/');
    if (!slash || slash == path) continue;
    *slash = '\0';
    char *end = slash;
    while (end > path + 1 && end[-1] == '/') *--end = '\0';
    int dir_fd = open_log_directory(path, fallback ? 0 : 2, fallback != 0);
    if (dir_fd < 0) continue;
    const char *name = slash + 1;
    int fd = -1;
    for (int rotated = 0; rotated < 2; rotated++) {
      fd = openat(dir_fd, name, O_WRONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK | O_APPEND | O_CREAT, 0600);
      if (fd < 0) break;
      struct stat st;
      if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_uid != geteuid()
          || st.st_nlink != 1 || fchmod(fd, 0600) != 0) {
        close(fd);
        fd = -1;
        break;
      }
      if (rotated || st.st_size <= 5 * 1024 * 1024) break;
      struct stat current;
      char old_name[64];
      int length = snprintf(old_name, sizeof(old_name), "%s.old", name);
      bool can_rotate = length >= 0 && (size_t)length < sizeof(old_name)
          && fstatat(dir_fd, name, &current, AT_SYMLINK_NOFOLLOW) == 0
          && current.st_dev == st.st_dev && current.st_ino == st.st_ino;
      close(fd);
      fd = -1;
      if (!can_rotate || renameat(dir_fd, name, dir_fd, old_name) != 0) break;
    }
    close(dir_fd);
    if (fd < 0) continue;
    FILE *file = fdopen(fd, "a");
    if (file) return file;
    close(fd);
  }
  return NULL;
}

static void log_event(const char *format, ...) {
  if (!logging_enabled) return;
  FILE *f = open_log_file();
  if (!f) return;
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  struct tm tm_info;
  localtime_r(&ts.tv_sec, &tm_info);
  char time_buf[32];
  strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm_info);
  fprintf(f, "[%s.%03ld] ", time_buf, ts.tv_nsec / 1000000L);
  va_list args;
  va_start(args, format);
  vfprintf(f, format, args);
  va_end(args);
  fputc('\n', f);
  fclose(f);
}

static void hex_dump(char *out, size_t out_size, const unsigned char *data, int len) {
  int max_bytes = (int)(out_size - 1) / 3;
  if (len > max_bytes) len = max_bytes;
  int pos = 0;
  for (int i = 0; i < len; i++) {
    pos += snprintf(out + pos, out_size - (size_t)pos, "%02x%s", data[i], (i + 1 < len) ? " " : "");
  }
  out[pos] = '\0';
}

static void write_state_cache(const char *line) {
  char path[512];
  char temp[1024];
  if (!runtime_path(path, sizeof(path), "rog-cetra-control.status")) return;
  int length = snprintf(temp, sizeof(temp), "%s.XXXXXX", path);
  if (length < 0 || (size_t)length >= sizeof(temp)) return;
  int fd = mkostemp(temp, O_CLOEXEC | O_NOFOLLOW);
  if (fd < 0) return;
  FILE *file = fchmod(fd, 0600) == 0 ? fdopen(fd, "w") : NULL;
  if (!file) {
    close(fd);
    unlink(temp);
    return;
  }
  bool failed = fputs(line, file) == EOF;
  if (fclose(file) != 0) failed = true;
  if (failed) {
    unlink(temp);
    return;
  }
  if (rename(temp, path) != 0) unlink(temp);
}
