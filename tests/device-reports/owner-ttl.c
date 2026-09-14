#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "../private-files.h"

#ifndef CETRA_SOURCE
#error CETRA_SOURCE must name the isolated source snapshot.
#endif

static int forbidden_io(void);
static int mock_poll(struct pollfd *, nfds_t, int);
static ssize_t mock_read(int, void *, size_t);
static int mock_clock_gettime(clockid_t, struct timespec *);
static FILE *guarded_fopen(const char *, const char *);
static int mock_accept4(int, struct sockaddr *, socklen_t *, int);
static ssize_t mock_send(int, const void *, size_t, int);
static int mock_close(int);

/* No daemon main, device access, real socket, or real stdin is used. */
#define main cetra_main_never_called
#define poll mock_poll
#define read mock_read
#define clock_gettime mock_clock_gettime
#define fopen guarded_fopen
#define open private_open
#define openat private_openat
#define mkostemp private_mkostemp
#define fdopen private_fdopen
#define socket(...) forbidden_io()
#define connect(...) forbidden_io()
#define bind(...) forbidden_io()
#define listen(...) forbidden_io()
#define recv(...) forbidden_io()
#define accept4 mock_accept4
#define send mock_send
#define close mock_close
#include CETRA_SOURCE
#undef main
#undef poll
#undef read
#undef clock_gettime
#undef fopen
#undef open
#undef openat
#undef mkostemp
#undef fdopen
#undef socket
#undef connect
#undef bind
#undef listen
#undef recv
#undef accept4
#undef send
#undef close

#define CHECK(test, message) do { \
  if (!(test)) { fprintf(stderr, "FAIL: %s (line %d)\n", message, __LINE__); exit(1); } \
} while (0)

static const long ttl_times[] = {0, 0, 1000, 30999, 31000, 31001, 40998, 40999, 41000, 41001};
static const long schedule_times[] = {0, 0, 499, 500, 999, 1000, 9999, 10000,
                                    10000, 19999, 20000, 55000, 55000, 64999, 65000, 65001};
static const long startup_times[] = {0, 0, 999, 1000, 1001, 10999, 11000, 11001};
static const long periodic_times[] = {0, 0, 9999, 10000, 10001, 10999, 11000,
                                    11001, 20999, 21000, 21001};
static const long *times;
static int stop_tick, presence_queries, failed;
static bool ttl, startup_failure, periodic_failure, short_write, failure_pending;
static int tick, delivered, accepted, opens, closes, queries, call_off;
static bool live, client_live[2];
static const char *root;
static hid_device handle;
static struct hid_device_info info = {"mock://owner-ttl", 3, NULL};

static int forbidden_io(void) {
  CHECK(false, "unexpected I/O: real device/socket access is forbidden");
  return -1;
}

static FILE *guarded_fopen(const char *path, const char *mode) {
  size_t length = strlen(root);
  CHECK(strncmp(path, root, length) == 0 && path[length] == '/' &&
        strstr(path, "/../") == NULL, "filesystem access escaped private root");
  return fopen(path, mode);
}

static int mock_clock_gettime(clockid_t clock, struct timespec *time) {
  CHECK(clock == CLOCK_MONOTONIC, "unexpected clock");
  time->tv_sec = times[tick] / 1000;
  time->tv_nsec = (times[tick] % 1000) * 1000000L;
  return 0;
}

static int mock_poll(struct pollfd *fds, nfds_t count, int timeout) {
  CHECK(count == 2 + MAX_CLIENTS && timeout == 50 && tick < stop_tick,
        "unexpected or unbounded owner poll");
  char path[1024], line[STATUS_BUFFER_SIZE];
  snprintf(path, sizeof(path), "%s/rog-cetra-control.status", root);
  FILE *cache = guarded_fopen(path, "r");
  CHECK(cache && fgets(line, sizeof(line), cache), "missing status cache");
  CHECK(fclose(cache) == 0, "cannot close cache");
  fprintf(stderr, "CACHE %d %s", tick, line);
  CHECK(fds[0].fd == 1000 && fds[1].fd == STDIN_FILENO, "unexpected owner fds");
  for (int i = 0; i < MAX_CLIENTS; i++)
    CHECK(fds[2 + i].fd == (i < accepted ? 1001 + i : -1), "unexpected client fd");
  for (nfds_t i = 0; i < count; i++) fds[i].revents = 0;
  tick++;
  if (ttl && (tick == 1 || tick == 4)) { fds[0].revents = POLLIN; return 1; }
  if (tick == stop_tick) { fds[1].revents = POLLIN; return 1; }
  return 0;
}

static ssize_t mock_read(int fd, void *buffer, size_t size) {
  (void)buffer;
  CHECK(fd == STDIN_FILENO && tick == stop_tick && size > 0, "unexpected stdin read");
  return 0;
}

static int mock_accept4(int fd, struct sockaddr *address, socklen_t *length, int flags) {
  CHECK(fd == 1000 && !address && !length &&
        flags == (SOCK_NONBLOCK | SOCK_CLOEXEC) && (tick == 1 || tick == 4),
        "unexpected accept");
  if (accepted == (tick == 1 ? 1 : 2)) { errno = EAGAIN; return -1; }
  client_live[accepted] = true;
  return 1001 + accepted++;
}

static ssize_t mock_send(int fd, const void *buffer, size_t size, int flags) {
  CHECK(fd >= 1001 && fd <= 1002 && client_live[fd - 1001] &&
        flags == (MSG_DONTWAIT | MSG_NOSIGNAL), "unexpected client send");
  CHECK(size > 0 && size < STATUS_BUFFER_SIZE &&
        ((const char *)buffer)[size - 1] == '\n', "invalid JSON line framing");
  fprintf(stderr, "CLIENT %d %d %.*s", tick, fd, (int)size, (const char *)buffer);
  return (ssize_t)size;
}

static int mock_close(int fd) {
  if (fd < 1000) return private_close(fd);
  CHECK(fd >= 1001 && fd <= 1002 && client_live[fd - 1001], "unexpected close");
  client_live[fd - 1001] = false;
  return 0;
}

static void valid_handle(hid_device *device) {
  CHECK(live && device == &handle, "invalid mock HID handle");
}

/* All APIs from the existing test-local hidapi header are implemented here. */
int hid_init(void) { return 0; }
int hid_exit(void) { return 0; }
struct hid_device_info *hid_enumerate(unsigned short vendor, unsigned short product) {
  CHECK(vendor == 0x0b05 && product == 0x1ad3 && !live, "unexpected enumeration");
  return &info;
}
void hid_free_enumeration(struct hid_device_info *devices) {
  CHECK(devices == &info, "unexpected enumeration free");
}
hid_device *hid_open_path(const char *path) {
  CHECK(!live && strcmp(path, info.path) == 0, "unexpected HID open");
  CHECK(opens < (startup_failure || periodic_failure ? 2 : 1), "too many opens");
  fprintf(stderr, "OPEN %ld\n", times[tick]);
  opens++;
  live = true;
  return &handle;
}
void hid_close(hid_device *device) {
  valid_handle(device);
  fprintf(stderr, "CLOSE %ld\n", times[tick]);
  failure_pending = false;
  live = false;
  closes++;
}
int hid_write(hid_device *device, const unsigned char *data, size_t size) {
  valid_handle(device);
  CHECK(!failure_pending, "query after transport failure before close");
  CHECK(size == 17 && data[0] == 0 && data[1] == 0xcc && data[2] == 0x12 &&
        (data[3] == 0x07 || data[3] == 0x25 || data[3] == 0x01), "unexpected query");
  for (size_t i = 4; i < size; i++) CHECK(data[i] == 0, "nonzero query padding");
  fprintf(stderr, "QUERY %d %ld %02x\n", tick, times[tick], data[3]);
  queries++;
  if (data[3] == 0x01) {
    presence_queries++;
    if (!failed && ((startup_failure && presence_queries == 1) ||
                    (periodic_failure && presence_queries == 2))) {
      failed++;
      failure_pending = true;
      return short_write ? (int)size - 1 : -1;
    }
  }
  return (int)size;
}
int hid_send_output_report(hid_device *device, const unsigned char *data, size_t size) {
  valid_handle(device);
  CHECK(!failure_pending && call_off == opens - 1 && size == 2 && data[0] == 5 && data[1] == 0,
        "only initial call-off is allowed; no lighting or reporter writes");
  call_off++;
  return (int)size;
}
int hid_read_timeout(hid_device *device, unsigned char *data, size_t size, int timeout) {
  valid_handle(device);
  CHECK(!failure_pending && size == 64 && timeout == 0, "unexpected HID read");
  if (!ttl) return 0;
  if (tick == 7 && delivered == 2) {
    CHECK(presence_queries == 3, "TTL recovery must follow the next presence query");
    const unsigned char recovered[] = {0xcc, 0x12, 0x01, 0, 0, 0x10};
    memcpy(data, recovered, sizeof(recovered));
    delivered++;
    return sizeof(recovered);
  }
  if (tick != 2 || delivered >= 2) return 0;
  const unsigned char presence[] = {0xcc, 0x12, 0x01, 0, 0, 0x11};
  const unsigned char charging[] = {0xcc, 0x12, 0x08, 0, 0, 0x01, 1};
  size_t length = delivered ? sizeof(charging) : sizeof(presence);
  memcpy(data, delivered ? charging : presence, length);
  delivered++;
  return (int)length;
}

int main(int argc, char **argv) {
  CHECK(argc == 3, "usage: owner-ttl PRIVATE_ROOT SCENARIO");
  root = argv[1];
  ttl = strcmp(argv[2], "ttl") == 0;
  startup_failure = strncmp(argv[2], "startup-", 8) == 0;
  periodic_failure = strncmp(argv[2], "periodic-", 9) == 0;
  short_write = strstr(argv[2], "short") != NULL;
  if (ttl) {
    times = ttl_times;
    stop_tick = sizeof(ttl_times) / sizeof(ttl_times[0]) - 1;
  } else if (startup_failure) {
    times = startup_times;
    stop_tick = sizeof(startup_times) / sizeof(startup_times[0]) - 1;
  } else if (periodic_failure) {
    times = periodic_times;
    stop_tick = sizeof(periodic_times) / sizeof(periodic_times[0]) - 1;
  } else {
    CHECK(strcmp(argv[2], "schedule") == 0, "unknown scenario");
    times = schedule_times;
    stop_tick = sizeof(schedule_times) / sizeof(schedule_times[0]) - 1;
  }
  struct stat st;
  CHECK(strncmp(root, "/tmp/opencode/", 14) == 0 && !strstr(root, "/../") &&
        stat(root, &st) == 0 && S_ISDIR(st.st_mode) && st.st_uid == getuid() &&
        (st.st_mode & 0777) == 0700, "unsafe test directory");
  CHECK(chdir(root) == 0 && setenv("HOME", root, 1) == 0 &&
        setenv("XDG_RUNTIME_DIR", root, 1) == 0 && setenv("XDG_STATE_HOME", root, 1) == 0,
        "cannot isolate filesystem paths");
  umask(0077);
  CHECK(DEVICE_REPORT_FRESH_MS == 30000L, "test requires 30000ms freshness contract");
  CHECK(owner(1000) == 0, "owner returned an error");
  CHECK(tick == stop_tick && delivered == (ttl ? 3 : 0) && accepted == (ttl ? 2 : 0) &&
        opens == (startup_failure || periodic_failure ? 2 : 1) && closes == opens &&
        !live && !client_live[0] && !client_live[1] && call_off == opens &&
        failed == (startup_failure || periodic_failure ? 1 : 0),
        "incomplete owner lifecycle");
  return 0;
}
