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

static int blocked(void);
static int test_poll(struct pollfd *, nfds_t, int);
static ssize_t test_read(int, void *, size_t);
static int test_clock(clockid_t, struct timespec *);
static FILE *test_fopen(const char *, const char *);
static int test_accept(int, struct sockaddr *, socklen_t *, int);
static ssize_t test_send(int, const void *, size_t, int);
static int test_close(int);

#define main daemon_main_not_called
#define poll test_poll
#define read test_read
#define clock_gettime test_clock
#define fopen test_fopen
#define open private_open
#define openat private_openat
#define mkostemp private_mkostemp
#define fdopen private_fdopen
#define socket(...) blocked()
#define connect(...) blocked()
#define bind(...) blocked()
#define listen(...) blocked()
#define recv(...) blocked()
#define accept4 test_accept
#define send test_send
#define close test_close
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

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static int tick, phase, field, failure, failed, opens, closes, delivered_tick = -1, delivered;
static bool live, poisoned, client, accepted;
static const char *root, *scenario;
static hid_device handle;
static struct hid_device_info info = {"mock://settings-readback", 3, NULL};
static const unsigned char queries[] = {0x2b, 0x2c, 0x28, 0x26};
static const unsigned char writes[] = {0x0c, 0x0d, 0x0a, 0x09};
static const unsigned char values[] = {2, 1, 2, 1};
static const char *commands[] = {"anc_level 2\n", "anc_adaptive on\n", "voice_prompt chinese\n", "proximity on\n"};

static int blocked(void) { CHECK(false); return -1; }
static FILE *test_fopen(const char *path, const char *mode) {
  CHECK(!strncmp(path, root, strlen(root)) && path[strlen(root)] == '/' && !strstr(path, "/../"));
  return fopen(path, mode);
}
static int test_clock(clockid_t clock, struct timespec *ts) {
  CHECK(clock == CLOCK_MONOTONIC);
  ts->tv_sec = tick / 4;
  ts->tv_nsec = tick % 4 * 250000000L;
  return 0;
}
static int test_poll(struct pollfd *fds, nfds_t count, int timeout) {
  CHECK(count == 2 + MAX_CLIENTS && timeout == 50 && tick < 180);
  char path[1024], line[STATUS_BUFFER_SIZE];
  snprintf(path, sizeof(path), "%s/rog-cetra-control.status", root);
  FILE *cache = test_fopen(path, "r");
  CHECK(cache && fgets(line, sizeof(line), cache) && !fclose(cache));
  fprintf(stderr, "CACHE %d %s", tick, line);
  for (nfds_t i = 0; i < count; i++) fds[i].revents = 0;
  CHECK(fds[0].fd == 1000 && fds[1].fd == 0 && fds[2].fd == (client ? 1001 : -1));
  tick++;
  if (tick == 1) { fds[0].revents = POLLIN; return 1; }
  if (tick == 180 || (tick == 10 && (!strcmp(scenario, "command") || !strcmp(scenario, "write") || !strcmp(scenario, "readback")))) {
    fds[1].revents = POLLIN;
    return 1;
  }
  return 0;
}
static ssize_t test_read(int fd, void *buffer, size_t size) {
  CHECK(fd == 0);
  if (tick == 180) return 0;
  CHECK(tick == 10 && strlen(commands[field]) <= size);
  memcpy(buffer, commands[field], strlen(commands[field]));
  return (ssize_t)strlen(commands[field]);
}
static int test_accept(int fd, struct sockaddr *addr, socklen_t *length, int flags) {
  CHECK(fd == 1000 && tick == 1 && !addr && !length && flags == (SOCK_NONBLOCK | SOCK_CLOEXEC));
  if (accepted) { errno = EAGAIN; return -1; }
  accepted = client = true;
  return 1001;
}
static ssize_t test_send(int fd, const void *buffer, size_t size, int flags) {
  CHECK(fd == 1001 && client && flags == (MSG_DONTWAIT | MSG_NOSIGNAL));
  CHECK(size < STATUS_BUFFER_SIZE && ((const char *)buffer)[size - 1] == '\n');
  fprintf(stderr, "CLIENT %d %.*s", tick, (int)size, (const char *)buffer);
  return (ssize_t)size;
}
static int test_close(int fd) {
  if (fd < 1000) return private_close(fd);
  CHECK(fd == 1001 && client); client = false; return 0;
}
static void valid(hid_device *device) { CHECK(device == &handle && live && !poisoned); }
int hid_init(void) { return blocked(); }
int hid_exit(void) { return blocked(); }
struct hid_device_info *hid_enumerate(unsigned short vendor, unsigned short product) {
  CHECK(vendor == VENDOR_ID && product == PRODUCT_ID && !live);
  return &info;
}
void hid_free_enumeration(struct hid_device_info *devices) { CHECK(devices == &info); }
hid_device *hid_open_path(const char *path) {
  CHECK(!live && !strcmp(path, info.path));
  live = true; opens++;
  fprintf(stderr, "OPEN %d\n", tick);
  return &handle;
}
void hid_close(hid_device *device) {
  CHECK(device == &handle && live);
  poisoned = live = false; closes++;
  fprintf(stderr, "CLOSE %d\n", tick);
}
int hid_write(hid_device *device, const unsigned char *data, size_t size) {
  valid(device);
  CHECK(size == 17 && data[0] == 0 && data[1] == 0xcc && data[2] == 0x12);
  CHECK(data[3] == 7 || data[3] == 0x25 || data[3] == 1 ||
        data[3] == 0x2b || data[3] == 0x2c || data[3] == 0x28 || data[3] == 0x26);
  for (size_t i = 4; i < size; i++) CHECK(data[i] == 0);
  fprintf(stderr, "QUERY %d %u\n", tick, data[3]);
  if (!failed && data[3] == queries[field] &&
      (!strcmp(scenario, "query") || (!strcmp(scenario, "readback") && tick == 10))) {
    poisoned = true; failed++;
    return failure ? 16 : -1;
  }
  return (int)size;
}
int hid_send_output_report(hid_device *device, const unsigned char *data, size_t size) {
  valid(device);
  if (size == 2) {
    CHECK(data[0] == 5 && data[1] == 0);
    fprintf(stderr, "CALL %d\n", tick);
  } else {
    unsigned char expected[64] = {0xcc, 0x41, writes[field], 0, 0, values[field]};
    CHECK(size == 64 && tick == 10 && !memcmp(data, expected, size));
    fprintf(stderr, "WRITE %d %u\n", tick, data[2]);
    if (!strcmp(scenario, "write")) {
      poisoned = true; failed++;
      return failure ? 63 : -1;
    }
  }
  return (int)size;
}
int hid_read_timeout(hid_device *device, unsigned char *data, size_t size, int timeout) {
  valid(device);
  CHECK(size == 64 && timeout == 0);
  if (delivered_tick != tick) { delivered_tick = tick; delivered = 0; }
  bool absence = !strcmp(scenario, "absence");
  bool battery_loss = !strcmp(scenario, "battery");
  if (!strcmp(scenario, "usb") && tick == 30) return -1;
  unsigned char packet[9] = {0xcc, 0x12, 0x07, 0, 0, 5, 91, 98, 100};
  int length = 0;
  if (delivered == 0 && (tick == phase || (opens > 1 && tick == phase + 40) ||
      (absence && (tick == 26 || tick == 148)) || (battery_loss && (tick == 25 || tick == 26 || tick == 30)))) {
    if (battery_loss && (tick == 25 || tick == 26)) packet[6] = packet[7] = 255;
    length = 9;
  } else if (absence && delivered == 0 && (tick == 25 || tick == 149)) {
    packet[2] = 1; packet[5] = tick == 25 ? 0 : 0x11; length = 6;
  } else if (delivered < 4 && (tick == phase + 1 || (absence && tick == 27))) {
    packet[2] = queries[delivered]; packet[5] = values[delivered]; length = 6;
  } else if (!strcmp(scenario, "command") && tick == 11 && delivered == 0) {
    packet[2] = queries[field]; packet[5] = values[field]; length = 6;
  }
  if (!length) return 0;
  delivered++;
  memcpy(data, packet, (size_t)length);
  fprintf(stderr, "REPORT %d %u %u\n", tick, packet[2], packet[5]);
  return length;
}
int main(int argc, char **argv) {
  CHECK(argc == 6);
  root = argv[1]; scenario = argv[2]; phase = atoi(argv[3]); field = atoi(argv[4]); failure = atoi(argv[5]);
  struct stat st;
  CHECK(!strncmp(root, "/tmp/opencode/", 14) && !strstr(root, "/../") &&
        !stat(root, &st) && S_ISDIR(st.st_mode) && st.st_uid == getuid() && (st.st_mode & 0777) == 0700);
  CHECK(!setenv("HOME", root, 1) && !setenv("XDG_RUNTIME_DIR", root, 1) && !setenv("XDG_STATE_HOME", root, 1));
  umask(0077);
  CHECK(!owner(1000) && tick == 180 && opens == closes && !live && !client);
  CHECK(failed == (!strcmp(scenario, "query") || !strcmp(scenario, "write") || !strcmp(scenario, "readback")));
  return 0;
}
