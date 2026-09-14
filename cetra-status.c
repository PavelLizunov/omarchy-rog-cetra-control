#define _GNU_SOURCE
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *mode_name(int mode) {
  if (mode == 0) return "off";
  if (mode == 1) return "anc";
  if (mode == 2) return "ambient";
  return "unknown";
}

static int parse_mode(const char *name) {
  if (strcmp(name, "off") == 0) return 0;
  if (strcmp(name, "anc") == 0) return 1;
  if (strcmp(name, "ambient") == 0) return 2;
  return -1;
}

static void build_mode_report(unsigned char *report, int mode) {
  memset(report, 0, 64);
  report[0] = 0xcc;
  report[1] = 0x41;
  report[2] = 0x08;
  report[5] = (unsigned char)mode;
}

static int selftest(void) {
  if (parse_mode("off") != 0 || parse_mode("anc") != 1 || parse_mode("ambient") != 2) return 1;
  if (strcmp(mode_name(1), "anc") != 0) return 1;

  unsigned char report[64];
  build_mode_report(report, 2);
  if (report[0] != 0xcc || report[1] != 0x41 || report[2] != 0x08 || report[5] != 2) return 1;
  for (size_t i = 0; i < sizeof(report); i++)
    if (i != 0 && i != 1 && i != 2 && i != 5 && report[i] != 0) return 1;

  puts("ok");
  return 0;
}

// Fixed user configuration path. This mode never opens a receiver or writes files.
static int read_settings(void) {
  const char *home = getenv("HOME");
  if (!home || home[0] != '/') return 1;
  char path[4096];
  int length = snprintf(path, sizeof(path), "%s/.config/omarchy/shell.json", home);
  if (length < 0 || (size_t)length >= sizeof(path)) return 1;
  alarm(3);
  int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
  if (fd < 0) return 1;
  struct stat before, after;
  const size_t limit = 1024 * 1024;
  if (fstat(fd, &before) != 0 || !S_ISREG(before.st_mode) || before.st_uid != geteuid()
      || before.st_size < 0 || before.st_size > (off_t)limit) { close(fd); return 1; }
  char *data = malloc(limit + 1);
  if (!data) { close(fd); return 1; }
  size_t used = 0;
  bool ok = true;
  while (used <= limit) {
    ssize_t n = read(fd, data + used, limit + 1 - used);
    if (n < 0) { ok = false; break; }
    if (n == 0) break;
    used += (size_t)n;
  }
  ok = ok && used <= limit && fstat(fd, &after) == 0 && before.st_size == after.st_size
    && before.st_mtim.tv_sec == after.st_mtim.tv_sec && before.st_mtim.tv_nsec == after.st_mtim.tv_nsec;
  close(fd);
  if (ok) ok = fwrite(data, 1, used, stdout) == used && fflush(stdout) == 0;
  free(data);
  return ok ? 0 : 1;
}

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "--selftest") == 0) return selftest();
  if (argc == 2 && strcmp(argv[1], "--read-settings") == 0) return read_settings();

  const char *fixture = getenv("CETRA_STATUS_FIXTURE");
  if (fixture && *fixture && argc == 1) {
    puts(fixture);
    return 0;
  }

  fprintf(stderr, "cetra-status is a test helper; run cetra-watch for device access\n");
  return 2;
}
