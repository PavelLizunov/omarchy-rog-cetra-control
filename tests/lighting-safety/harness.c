#define _GNU_SOURCE
#include <assert.h>
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
#error CETRA_SOURCE must name the source snapshot; never build the daemon main.
#endif

static int mock_poll(struct pollfd *, nfds_t, int);
static ssize_t mock_read(int, void *, size_t);
static int mock_clock_gettime(clockid_t, struct timespec *);
static FILE *guarded_fopen(const char *, const char *);
static int forbidden_io(void);
static int mock_accept4(int, struct sockaddr *, socklen_t *, int);
static ssize_t mock_recv(int, void *, size_t, int);
static ssize_t mock_send(int, const void *, size_t, int);
static int mock_close(int);

/* Headers are already included. No socket, real stdin, or device open is used. */
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
#define accept4 mock_accept4
#define send mock_send
#define recv mock_recv
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
#undef accept4
#undef send
#undef recv
#undef close

#define CHECK(test, message) do { \
  if (!(test)) { fprintf(stderr, "FAIL: %s (line %d)\n", message, __LINE__); exit(1); } \
} while (0)

static int tick, delivered_tick = -1, opens, closes, lighting_count;
static int fail_at, failed, failure_closed;
static bool live, usb, case_cycle, explicit_command, off_effect, restore_failure, short_write;
static bool owner_running, failure_pending;
static bool ipc, command_transport, replacement, call_active, call_failure;
static const char *input_at[16], *client_at[2][16];
static int accepted, client_reads[2], client_sends[2], client_closed;
static bool client_live[2];
static int call_count;
static int query_in_generation, opened_tick, next_query_tick, query_phase;
static bool settings_connected, settings_polling;
static bool presence_case, presence_delivered;
static bool presence_resume, presence_first, presence_batch, presence_only;
static int resume_reports;
static int settings_phase, next_settings_tick;
static struct call_output { int tick, generation, value; } calls[32];
static const char *root;
static hid_device handles[8];
static struct hid_device_info info = {"mock://lighting-safety", 3, NULL};
static struct output {
  unsigned char bytes[64];
  int tick, generation, preceding_calls;
} outputs[64];

static int forbidden_io(void) {
  CHECK(false, "unexpected device/socket API; real I/O is forbidden");
  return -1;
}

static FILE *guarded_fopen(const char *path, const char *mode) {
  size_t length = strlen(root);
  CHECK(strncmp(path, root, length) == 0 && path[length] == '/',
        "filesystem access escaped the private test directory");
  CHECK(strstr(path, "/../") == NULL, "parent traversal is forbidden");
  return fopen(path, mode);
}

static int mock_clock_gettime(clockid_t clock, struct timespec *time) {
  CHECK(clock == CLOCK_MONOTONIC || clock == CLOCK_REALTIME, "unexpected clock");
  time->tv_sec = 100 + tick / 4;
  time->tv_nsec = (tick % 4) * 250000000L;
  return 0;
}

static int mock_poll(struct pollfd *fds, nfds_t count, int timeout) {
  CHECK(owner_running && count == 2 + MAX_CLIENTS && timeout >= 0,
        "only the bounded actual owner loop may poll");
  CHECK(!settings_connected || tick < next_settings_tick,
        "connected owner omitted a due settings query");
  char path[1024], line[1024];
  snprintf(path, sizeof(path), "%s/rog-cetra-control.status", root);
  FILE *cache = guarded_fopen(path, "r");
  CHECK(cache && fgets(line, sizeof(line), cache), "missing owner status cache");
  CHECK(fclose(cache) == 0, "cannot close cache");
  fprintf(stderr, "STATE %d %s", tick, line);
  CHECK(++tick <= 16, "owner exceeded the deterministic iteration bound");
  for (nfds_t i = 0; i < count; i++) fds[i].revents = 0;
  CHECK(fds[0].fd == (ipc ? 1000 : -1) && fds[1].fd == STDIN_FILENO, "unexpected owner fds");
  for (nfds_t i = 2; i < count; i++) {
    int expected = ipc && tick > 1 && i < 4 ? 999 + (int)i : -1;
    CHECK(fds[i].fd == expected, "unexpected client fd/lifetime");
  }
  if (tick == 14) {
    fds[1].revents = POLLHUP;
    return 1;
  }
  int ready = 0;
  if (ipc && tick == 1) { fds[0].revents = POLLIN; ready++; }
  if (input_at[tick]) {
    fds[1].revents = POLLIN;
    ready++;
  }
  for (int i = 0; i < 2; i++) {
    if (client_at[i][tick]) { fds[2 + i].revents = POLLIN; ready++; }
  }
  return ready;
}

static ssize_t copy_input(const char *command, void *buffer, size_t size) {
  CHECK(command && strlen(command) <= size, "unexpected read or small input buffer");
  memcpy(buffer, command, strlen(command));
  return (ssize_t)strlen(command);
}
static ssize_t mock_read(int fd, void *buffer, size_t size) {
  CHECK(fd == STDIN_FILENO, "unexpected stdin fd");
  return copy_input(input_at[tick], buffer, size);
}
static int mock_accept4(int fd, struct sockaddr *address, socklen_t *length, int flags) {
  CHECK(ipc && tick == 1 && fd == 1000 && !address && !length &&
        flags == (SOCK_NONBLOCK | SOCK_CLOEXEC), "unexpected accept");
  if (accepted == 2) { errno = EAGAIN; return -1; }
  client_live[accepted] = true;
  return 1001 + accepted++;
}
static ssize_t mock_recv(int fd, void *buffer, size_t size, int flags) {
  CHECK(ipc && fd >= 1001 && fd <= 1002 && client_live[fd - 1001] && flags == 0,
        "unexpected client recv");
  client_reads[fd - 1001]++;
  return copy_input(client_at[fd - 1001][tick], buffer, size);
}
static ssize_t mock_send(int fd, const void *buffer, size_t size, int flags) {
  CHECK(ipc && fd >= 1001 && fd <= 1002 && client_live[fd - 1001] &&
        flags == (MSG_DONTWAIT | MSG_NOSIGNAL), "unexpected client send");
  CHECK(size > 0 && size < 512 && ((const char *)buffer)[0] == '{' &&
        ((const char *)buffer)[size - 1] == '\n', "invalid client state framing");
  client_sends[fd - 1001]++;
  return (ssize_t)size;
}
static int mock_close(int fd) {
  if (fd < 1000) return private_close(fd);
  CHECK(ipc && fd >= 1001 && fd <= 1002 && client_live[fd - 1001], "unexpected close");
  client_live[fd - 1001] = false;
  client_closed++;
  return 0;
}

static void valid_handle(hid_device *device) {
  CHECK(live && opens > 0 && device == &handles[opens - 1], "invalid mock HID handle");
}

/* Every API in the test-local hidapi/hidapi.h is implemented locally. */
int hid_init(void) { return 0; }
int hid_exit(void) { return 0; }
struct hid_device_info *hid_enumerate(unsigned short vendor, unsigned short product) {
  CHECK(vendor == 0x0b05 && product == 0x1ad3 && !live, "unexpected HID enumeration");
  return &info;
}
void hid_free_enumeration(struct hid_device_info *devices) {
  CHECK(devices == &info, "unexpected enumeration free");
}
hid_device *hid_open_path(const char *path) {
  CHECK(!live && opens < 8 && strcmp(path, info.path) == 0, "unexpected HID open");
  live = true;
  query_in_generation = 0;
  opened_tick = tick;
  next_query_tick = tick + 2;
  query_phase = 0;
  settings_connected = settings_polling = false;
  if (presence_resume) presence_delivered = false;
  return &handles[opens++];
}
void hid_close(hid_device *device) {
  valid_handle(device);
  if (failure_pending) {
    failure_closed++;
    failure_pending = false;
  }
  live = false;
  settings_connected = settings_polling = false;
  closes++;
}
int hid_write(hid_device *device, const unsigned char *data, size_t size) {
  valid_handle(device);
  CHECK(!failure_pending, "query written to a failed transport before close");
  CHECK(size == 17 && data[0] == 0 && data[1] == 0xcc && data[2] == 0x12 &&
        (data[3] == 0x07 || data[3] == 0x25 || data[3] == 0x01 ||
         data[3] == 0x2b || data[3] == 0x2c || data[3] == 0x28 || data[3] == 0x26), "unexpected query bytes");
  for (size_t i = 4; i < size; i++) CHECK(data[i] == 0, "nonzero query padding");
  if (data[3] == 0x2b || data[3] == 0x2c || data[3] == 0x28 || data[3] == 0x26) {
    const unsigned char settings[] = {0x2b, 0x2c, 0x28, 0x26};
    CHECK(settings_connected && delivered_tick == tick && tick >= next_settings_tick,
          "settings queried before connection/report drain or above the bounded rate");
    if (!settings_polling) settings_phase = 0;
    CHECK(data[3] == settings[settings_phase], "wrong settings round-robin phase");
    settings_polling = true;
    settings_phase = (settings_phase + 1) % 4;
    next_settings_tick = tick + 10;
    return (int)size;
  }
  if (query_in_generation < 3) {
    const unsigned char startup[] = {0x07, 0x25, 0x01};
    CHECK(tick == opened_tick && data[3] == startup[query_in_generation], "wrong startup queries");
  } else {
    CHECK(tick == next_query_tick && data[3] == (query_phase ? 0x25 : 0x07),
          "battery/mode schedule changed or presence queried before ten seconds");
    query_phase = !query_phase;
    next_query_tick = tick + 2;
  }
  query_in_generation++;
  return (int)size;
}
int hid_send_output_report(hid_device *device, const unsigned char *data, size_t size) {
  valid_handle(device);
  CHECK(!failure_pending, "output written to a failed transport before close");
  if (size == 2) {
    CHECK(data[0] == 5 && (data[1] == 0 || data[1] == 0x31), "unexpected call/mute output");
    CHECK(call_count < 32, "too many call outputs");
    calls[call_count++] = (struct call_output){tick, opens, data[1]};
    if (call_failure && !failed && data[1] == 0x31 && tick >= 5 && delivered_tick == tick) {
      failed++;
      failure_pending = true;
      return short_write ? 1 : -1;
    }
    return (int)size;
  }
  CHECK(size == 64 && data[0] == 0xcc &&
        ((data[1] == 0x51 && data[2] == 0x28) ||
         (data[1] == 0x50 && data[2] == 0x55)), "unexpected lighting opcode/length");
  CHECK(lighting_count < 64, "too many lighting reports");
  struct output *output = &outputs[lighting_count++];
  memcpy(output->bytes, data, size);
  output->tick = tick;
  output->generation = opens;
  output->preceding_calls = call_count;
  if (fail_at && lighting_count == fail_at) {
    failed++;
    failure_pending = true;
    return short_write ? (int)size - 1 : -1;
  }
  return (int)size;
}
int hid_read_timeout(hid_device *device, unsigned char *data, size_t size, int timeout) {
  valid_handle(device);
  CHECK(size == 64 && timeout == 0, "unexpected HID read arguments");
  CHECK(!failure_pending, "owner ignored lighting restore failure and kept reading");
  if (presence_resume && opens == 1) {
    if (delivered_tick != tick) { delivered_tick = tick; resume_reports = 0; }
    unsigned char packet[] = {0xcc, 0x12, 0x07, 0, 0, 5, 91, 98, 100};
    bool presence = false, missing = false;
    if (tick == 1 && resume_reports == 0) {
      /* Initial battery connection, before any explicit session intent. */
    } else if (tick == 3 && resume_reports == 0) {
      presence = true;
      packet[5] = 0;
    } else if (!presence_only && ((tick == 3 && resume_reports == 1) || (tick == 4 && resume_reports == 0))) {
      missing = true;
    } else if (tick == 5 && resume_reports < (presence_batch ? 2 : 1)) {
      presence = presence_only || (presence_batch ? (resume_reports == 0) == presence_first : presence_first);
      packet[5] = presence ? 0x11 : 5;
    } else if (tick == 6 && resume_reports == 0) {
      presence = presence_only || presence_batch || !presence_first;
      packet[5] = presence ? 0x11 : 5;
    } else if (tick == 7 && resume_reports < 2) {
      /* Duplicate positive presence and battery must not replay restoration. */
      presence = resume_reports == 0;
      packet[5] = presence ? 0x11 : 5;
    } else {
      return 0;
    }
    if (presence) {
      packet[2] = 1;
      presence_delivered = packet[5] == 0;
      if (presence_delivered) settings_connected = settings_polling = false;
      else if (presence_only || tick >= (presence_batch ? 5 : 6)) settings_connected = true;
    } else if (missing) {
      packet[6] = packet[7] = 255;
      if (tick == 4) settings_connected = settings_polling = false;
    } else if (!presence_delivered) {
      settings_connected = true;
    }
    resume_reports++;
    size_t length = presence ? 6 : sizeof(packet);
    memcpy(data, packet, length);
    return (int)length;
  }
  if (presence_case && tick == 3 && !presence_delivered) {
    const unsigned char absent[] = {0xcc, 0x12, 0x01, 0, 0, 0};
    presence_delivered = true;
    settings_connected = settings_polling = false;
    memcpy(data, absent, sizeof(absent));
    return sizeof(absent);
  }
  if (delivered_tick == tick) return 0;
  delivered_tick = tick;
  if (usb && tick == 3) return -1;
  /* Two missing batteries are needed to enter the case; a reopen gets a battery. */
  bool missing = case_cycle && (tick == 3 || tick == 4);
  bool battery = tick == 1 || missing || (case_cycle && tick == 5) ||
                 (opens > 1 && tick >= 7);
  if (!battery) return 0;
  if (missing && tick == 4) settings_connected = settings_polling = false;
  if (!missing && !presence_delivered) settings_connected = true;
  const unsigned char packet[] = {0xcc, 0x12, 0x07, 0, 0, 5,
                                 missing ? 255 : 91, missing ? 255 : 98, 100};
  memcpy(data, packet, sizeof(packet));
  return (int)sizeof(packet);
}

static void expect_reports(int start, int count, int effect, int r, int g, int b) {
  CHECK(start + count <= lighting_count && count <= 4, "missing lighting reports");
  for (int step = 0; step < count; step++) {
    unsigned char expected[64] = {0xcc, step < 2 ? 0x51 : 0x50,
                               step < 2 ? 0x28 : 0x55};
    if (step < 2) {
      expected[5] = step == 0 ? 1 : 0;
      expected[6] = effect ? effect : 1;
      expected[7] = effect ? r : 0;
      expected[8] = effect ? g : 0;
      expected[9] = effect ? b : 0;
    }
    CHECK(memcmp(outputs[start + step].bytes, expected, sizeof(expected)) == 0,
          "lighting hardware bytes differ (including side, RGB, commit or padding)");
  }
}

static void expect_desired(const struct device_state *state, bool valid,
                           int effect, int r, int g, int b) {
  CHECK(state->lighting == effect, "desired lighting enum was overwritten");
#if TEST_HAS_DESIRED_VALID
  CHECK(state->lighting_desired_valid == valid, "desired validity changed incorrectly");
#else
  (void)valid;
  CHECK(false, "agreed lighting_desired_valid flag is absent in historical source");
#endif
  CHECK(state->lighting_r == r && state->lighting_g == g && state->lighting_b == b,
        "desired RGB was overwritten");
}

static void command_failure(bool replacement, int step) {
  struct device_state state = {.receiver = true, .connected = true, .lighting = -1,
                              .lighting_r = 255, .lighting_g = 0, .lighting_b = 0};
  struct command_source source = {.fd = -1};
  hid_device *device = open_receiver();
  CHECK(device != NULL, "mock receiver not opened");
  if (replacement) {
    CHECK(handle_command(device, &state, &source, "lighting static 17 34 51"),
          "successful explicit command rejected");
    CHECK(lighting_count == 4, "explicit command must send exactly four reports");
    expect_reports(0, 4, 1, 17, 34, 51);
    expect_desired(&state, true, 1, 17, 34, 51);
  }
  int start = lighting_count;
  fail_at = start + step;
  /* Return policy belongs to production; desired must be transactional either way. */
  (void)handle_command(device, &state, &source, "lighting breathing 91 82 73");
  CHECK(failed == 1 && lighting_count == start + step, "write failure did not stop the sequence");
  expect_reports(start, step, 2, 91, 82, 73);
  expect_desired(&state, replacement, replacement ? 1 : -1,
                 replacement ? 17 : 255, replacement ? 34 : 0, replacement ? 51 : 0);
  disconnect_receiver(&device, &state);
  expect_desired(&state, replacement, replacement ? 1 : -1,
                 replacement ? 17 : 255, replacement ? 34 : 0, replacement ? 51 : 0);
}

static void consume_failure(const char *name, int step) {
  struct device_state state = {.lighting = -1, .lighting_r = 255};
  struct command_source source = {.fd = -1};
  hid_device *device = open_receiver();
  const char *initial = "call on\nlighting static 17 34 51\n";
  CHECK(consume_commands(device, &state, &source, initial, strlen(initial)),
        "initial command block failed");
  CHECK(source.call_requested && !source.length && !source.overflow,
        "initial call/framing state incorrect");
  expect_desired(&state, true, 1, 17, 34, 51);
  CHECK(lighting_count == 4, "initial static must send four reports");
  expect_reports(0, 4, 1, 17, 34, 51);
  bool same_block = strstr(name, "same") != NULL;
  bool fragment = strstr(name, "fragment") != NULL;
  const char *block = same_block ?
      "lighting breathing 91 82 73\nlighting off\ncall off\n" : fragment ?
      "lighting breathing 91 82 73\nlighting off\ncall o" :
      "lighting breathing 91 82 73\nlighting off\n";
  fail_at = 4 + step;
  CHECK(!consume_commands(device, &state, &source, block, strlen(block)),
        "failed command block reported success");
  CHECK(failed == 1 && lighting_count == 4 + step, "later command wrote after transport failure");
  CHECK(source.call_requested == !same_block && !source.overflow,
        "call off lost after failed command in the same block");
  CHECK(source.length == (fragment ? strlen("call o") : 0), "failed command poisoned framing");
  if (fragment) CHECK(memcmp(source.buffer, "call o", source.length) == 0, "fragment lost");
  expect_reports(4, step, 2, 91, 82, 73);
  expect_desired(&state, true, 1, 17, 34, 51);
  disconnect_receiver(&device, &state);
  device = open_receiver();
  const char *tail = fragment ? "ff\n" : "call off\n";
  CHECK(consume_commands(device, &state, &source, tail, strlen(tail)), "reconnected tail failed");
  CHECK(!source.call_requested && !source.length && !source.overflow,
        "call off after reconnect was not consumed with intact framing");
  CHECK(lighting_count == 4 + step, "failed lighting command replayed after reconnect");
  expect_desired(&state, true, 1, 17, 34, 51);
  disconnect_receiver(&device, &state);
}

static void numeric_commands(bool consume) {
  static const struct {
    const char *command;
    int effect, r, g, b;
  } valid[] = {
    {"lighting off", 0, 255, 0, 0},
    {"lighting static", 1, 255, 0, 0},
    {"lighting breathing", 2, 255, 0, 0},
    {"lighting strobing", 3, 255, 0, 0},
    {"lighting cycle", 4, 255, 0, 0},
    {"lighting colorcycle", 4, 255, 0, 0},
    {"lighting static 0 255 1", 1, 0, 255, 1},
    {"lighting breathing 255 0 255", 2, 255, 0, 255},
    {"lighting strobing 001 002 003", 3, 1, 2, 3},
    {"lighting cycle 17 34 51", 4, 17, 34, 51},
    {"lighting colorcycle 17 34 51", 4, 17, 34, 51},
    {"lighting off 17 34 51", 0, 17, 34, 51},
    {" \tlighting\tstatic\t0  128\t255 \t", 1, 0, 128, 255},
    {"lighting off \t", 0, 255, 0, 0},
  };
  static const char *invalid[] = {
    "lighting static 1", "lighting static 1 2", "lighting off 1",
    "lighting cycle garbage", "lighting invalid 1 2 3", "lighting staticgarbage 1 2 3",
    "lighting static -1 2 3", "lighting static 1 -2 3", "lighting static 1 2 -3",
    "lighting static +1 2 3", "lighting static 1 +2 3", "lighting static 1 2 +3",
    "lighting static 256 2 3", "lighting static 1 256 3", "lighting static 1 2 256",
    "lighting static 0x10 2 3", "lighting static 1 2 3.0", "lighting static 1e2 2 3",
    "lighting static 1 2 3junk", "lighting static 1 2 3 junk", "lighting static 1 2 3 4",
    "lighting static 1x 2 3", "lighting static 1 2x 3", "lighting off 0 0 0 junk",
    "lighting static 999999999999999999999999999999999999 0 0",
    "lighting static 0 999999999999999999999999999999999999 0",
    "lighting static 0 0 999999999999999999999999999999999999",
    "lighting static 4294967296 0 0", "lighting static 0 0 -4294967296",
    "lighting static 1,2,3", "lighting static --1 0 0", "lighting static 1 2 /",
  };
  hid_device *device = open_receiver();
  for (int seeded = 0; seeded < 2; seeded++) {
    for (size_t i = 0; i < sizeof(valid) / sizeof(valid[0]) + sizeof(invalid) / sizeof(invalid[0]); i++) {
      bool accepted_value = i < sizeof(valid) / sizeof(valid[0]);
      const char *command = accepted_value ? valid[i].command : invalid[i - sizeof(valid) / sizeof(valid[0])];
      struct device_state state = {.lighting = -1, .lighting_r = 255};
      struct command_source source = {.fd = -1, .call_requested = true};
      lighting_count = 0;
      if (seeded) CHECK(handle_command(device, &state, &source, "lighting breathing 91 82 73"), "seed failed");
      struct device_state before = state;
      int start = lighting_count;
      if (consume) {
        char block[COMMAND_BUFFER_SIZE];
        CHECK(snprintf(block, sizeof(block), "%s\ncall o", command) < (int)sizeof(block), "test too long");
        CHECK(consume_commands(device, &state, &source, block, strlen(block)), "numeric block failed");
        CHECK(source.length == 6 && source.call_requested && !source.overflow, "next call fragment lost");
      } else {
        CHECK(handle_command(device, &state, &source, command), "numeric command failed transport");
      }
      if (accepted_value) {
        CHECK(lighting_count == start + 4, "valid numeric command did not write exactly four reports");
        expect_reports(start, 4, valid[i].effect, valid[i].r, valid[i].g, valid[i].b);
        expect_desired(&state, true, valid[i].effect, valid[i].r, valid[i].g, valid[i].b);
      } else {
        CHECK(lighting_count == start && memcmp(&state, &before, sizeof(state)) == 0,
              "invalid numeric command wrote hardware or changed state");
      }
      int followup = lighting_count;
      const char *tail = consume ? "ff\nlighting off\n" : "call off\nlighting off\n";
      CHECK(consume_commands(device, &state, &source, tail, strlen(tail)), "followup failed");
      CHECK(!source.call_requested && !source.length && !source.overflow, "next call off frame damaged");
      CHECK(lighting_count == followup + 4, "following lighting off lost or duplicated");
      expect_reports(followup, 4, 0, 0, 0, 0);
      expect_desired(&state, true, 0, 255, 0, 0);
    }
  }
  hid_close(device);
}

static void run_owner_case(const char *name, int step) {
  usb = strstr(name, "usb") != NULL;
  case_cycle = strstr(name, "case") != NULL;
  presence_case = strstr(name, "presence") != NULL;
  presence_resume = strstr(name, "resume") != NULL;
  presence_first = strstr(name, "presence-first") != NULL;
  presence_batch = strstr(name, "batch") != NULL;
  presence_only = strstr(name, "presence-only") != NULL;
  explicit_command = strstr(name, "static") != NULL || strstr(name, "off") != NULL;
  off_effect = strstr(name, "off") != NULL;
  command_transport = strncmp(name, "owner-", 6) == 0;
  ipc = strncmp(name, "ipc-", 4) == 0;
  replacement = strstr(name, "replacement") != NULL || ipc;
  call_active = strncmp(name, "call-", 5) == 0;
  call_failure = strstr(name, "call-write-failure") != NULL;
  restore_failure = !command_transport && !ipc && strstr(name, "failure") != NULL;
  if (restore_failure && !call_failure) fail_at = 4 + step;
  if (explicit_command) input_at[2] = off_effect ? "lighting off\n" : "lighting static 17 34 51\n";
  if (call_active) input_at[2] = explicit_command ? "call on\nlighting static 17 34 51\n" : "call on\n";
  if (command_transport) {
    explicit_command = replacement;
    fail_at = (replacement ? 4 : 0) + step;
    input_at[2] = replacement ? "call on\nlighting static 17 34 51\n" : "call on\n";
    input_at[3] = "lighting breathing 91 82 73\nlighting off\ncall off\n";
  }
  if (ipc) {
    input_at[2] = NULL;
    explicit_command = true;
    fail_at = 4 + step;
    client_at[0][2] = "lighting static 17 34 51\n";
    client_at[1][2] = "call on\n";
    const char *first = "lighting breathing 91 82 73\nlighting off\n";
    if (strstr(name, "stdin")) input_at[3] = first;
    else client_at[0][3] = first;
    bool fragment = strstr(name, "fragment") != NULL;
    client_at[1][3] = fragment ? "lighting off\ncall o" : "lighting off\ncall off\n";
    if (fragment) client_at[1][8] = "ff\n";
  }
  owner_running = true;
  CHECK(owner(ipc ? 1000 : -1) == 0, "owner returned an error");
  owner_running = false;
  CHECK(tick == 14 && opens == closes && !live, "owner did not stop/close cleanly");
  if (presence_resume) {
    int eligible_tick = presence_batch || presence_only ? 5 : 6;
    int prefix = restore_failure && !call_failure ? step : 0;
    CHECK(opens == (restore_failure ? 2 : 1) && failed == (restore_failure ? 1 : 0)
          && failure_closed == failed, "deferred restore failure did not close/reopen exactly once");
    CHECK(lighting_count == (explicit_command ? 8 + prefix : 0),
          "eligible transition lost/duplicated restoration or invented lighting intent");
    if (explicit_command) {
      expect_reports(0, 4, 1, 17, 34, 51);
      for (int i = 0; i < 4; i++) CHECK(outputs[i].tick == 2, "lighting before explicit command");
      if (prefix) expect_reports(4, prefix, 1, 17, 34, 51);
      expect_reports(4 + prefix, 4, 1, 17, 34, 51);
      for (int i = 4; i < lighting_count; i++) {
        bool retry = restore_failure && i >= 4 + prefix;
        CHECK(outputs[i].tick == eligible_tick + (retry ? 4 : 0) &&
              outputs[i].generation == (retry ? 2 : 1), "restore ran before eligibility or repeated later");
        if (call_active) {
          int call = outputs[i].preceding_calls - 1;
          CHECK(call >= 0 && calls[call].tick == outputs[i].tick && calls[call].value == 0x31 &&
                calls[call].generation == outputs[i].generation, "call restore must precede lighting");
        }
      }
    }
    int restored_calls = 0;
    for (int i = 0; i < call_count; i++) {
      if (calls[i].generation == 1 && calls[i].tick >= 3 && calls[i].value == 0x31) {
        CHECK(calls[i].tick == eligible_tick, "call restored while unavailable or after duplicate report");
        restored_calls++;
      }
    }
    CHECK(restored_calls == (call_active ? 1 : 0), "call not restored exactly once on eligible transition");
    if (!restore_failure) CHECK(call_count == (call_active ? 4 : 1), "unexpected call writes");
    return;
  }
  if (presence_case) {
    CHECK(presence_delivered && lighting_count == 4 && opens == 1,
          "battery reconnect relit earbuds despite confirmed absence");
    expect_reports(0, 4, 1, 17, 34, 51);
    return;
  }
  if (command_transport || ipc) {
    int start = replacement ? 4 : 0;
    CHECK(failed == 1 && failure_closed == 1 && opens == 2, "failed owner command did not reconnect");
    CHECK(lighting_count == start + step + (replacement ? 4 : 0),
          "failed first/replacement command activated new desired or lost old desired");
    if (replacement) expect_reports(0, 4, 1, 17, 34, 51);
    expect_reports(start, step, 2, 91, 82, 73);
    CHECK(outputs[start].tick == 3 && outputs[start].generation == 1, "failure not in scheduled owner command");
    if (replacement) {
      expect_reports(start + step, 4, 1, 17, 34, 51);
      CHECK(outputs[start + step].generation == 2, "previous desired not restored after failed command");
    }
    bool fragment = strstr(name, "fragment") != NULL;
    bool saw_on = false, saw_off = false;
    for (int i = 0; i < call_count; i++) {
      if (calls[i].tick == 2 && calls[i].value == 0x31) saw_on = true;
      if (calls[i].generation == 2 && calls[i].tick < 14 && calls[i].value == 0) saw_off = true;
      if (calls[i].generation == 2 && (!fragment || calls[i].tick >= 8))
        CHECK(calls[i].value == 0, "call_requested remained true after call off");
    }
    CHECK(saw_on && saw_off, "call on/off did not cross the actual owner lifecycle");
    if (ipc) {
      CHECK(accepted == 2 && client_closed == 2 && client_sends[0] && client_sends[1],
            "mock clients did not complete their lifecycle");
      CHECK(client_reads[0] == (strstr(name, "stdin") ? 1 : 2) &&
            client_reads[1] == (fragment ? 3 : 2), "ready client lost after earlier source failure");
    }
    return;
  }
  if (!explicit_command) {
    CHECK(lighting_count == 0, "startup/case/reopen without explicit desired wrote lighting");
    CHECK(opens == (usb ? 2 : 1), "USB error/reopen path was not exercised");
    return;
  }
  int effect = off_effect ? 0 : 1;
  CHECK(lighting_count == (restore_failure && !call_failure ? 8 + step : 8),
        "explicit command and restore must each send exactly four reports (or failed prefix)");
  expect_reports(0, 4, effect, 17, 34, 51);
  CHECK(outputs[0].tick == 2 && outputs[3].tick == 2,
        "lighting was sent before the explicit stdin command");
  expect_reports(4, restore_failure && !call_failure ? step : 4, effect, 17, 34, 51);
  CHECK(outputs[4].tick >= (usb ? 7 : 5), "lighting restored before case/reopen transition");
  CHECK(outputs[4].generation == (usb ? 2 : 1) + (call_failure ? 1 : 0), "wrong restore handle generation");
  if (restore_failure) {
    CHECK(failed == 1 && failure_closed == 1, "restore failure was not handled as a transport error");
    int retry = call_failure ? 4 : 4 + step;
    expect_reports(retry, 4, effect, 17, 34, 51);
    CHECK(outputs[retry].generation == (usb ? 3 : 2),
          "failed restore must retain desired for the next receiver open");
  }
  if (call_active) {
    for (int i = 4; i < lighting_count; i++) {
      int call = outputs[i].preceding_calls - 1;
      CHECK(call >= 0 && calls[call].value == 0x31 && calls[call].tick == outputs[i].tick &&
            calls[call].generation == outputs[i].generation,
            "lighting restore was not preceded by successful active call-context restore");
    }
    CHECK(calls[1].tick == 2 && calls[1].value == 0x31, "call context was not enabled");
    bool restore_call = false;
    for (int i = 0; i < call_count; i++) {
      if (calls[i].tick == (usb ? 7 : 5) && calls[i].value == 0x31) restore_call = true;
    }
    CHECK(restore_call, "active call restore not exercised");
  }
  CHECK(opens == (usb ? 2 : 1) + (restore_failure ? 1 : 0), "unexpected receiver lifecycle");
}

int main(int argc, char **argv) {
  CHECK(argc == 5, "usage: harness CASE STEP negative|short PRIVATE_ROOT");
  root = argv[4];
  struct stat st;
  CHECK(root[0] == '/' && stat(root, &st) == 0 && S_ISDIR(st.st_mode) &&
        st.st_uid == getuid() && (st.st_mode & 0777) == 0700, "unsafe test directory");
  CHECK(chdir(root) == 0, "cannot enter private directory");
  CHECK(setenv("XDG_RUNTIME_DIR", root, 1) == 0 &&
        setenv("XDG_STATE_HOME", root, 1) == 0 && setenv("HOME", root, 1) == 0,
        "cannot isolate filesystem paths");
  umask(0077);
  logging_enabled = true;
  short_write = strcmp(argv[3], "short") == 0;
  int step = atoi(argv[2]);
  if (strncmp(argv[1], "numeric-", 8) == 0) {
    numeric_commands(strcmp(argv[1], "numeric-consume") == 0);
  } else if (strcmp(argv[1], "first-failure") == 0 || strcmp(argv[1], "replacement-failure") == 0) {
    CHECK(step >= 1 && step <= 4, "invalid failure step");
    command_failure(strcmp(argv[1], "replacement-failure") == 0, step);
  } else if (strncmp(argv[1], "consume-", 8) == 0) {
    CHECK(step >= 1 && step <= 4, "invalid failure step");
    consume_failure(argv[1], step);
  } else {
    run_owner_case(argv[1], step);
  }
  fprintf(stderr, "PASS: %s step=%d writes=%d opens=%d closes=%d\n",
          argv[1], step, lighting_count, opens, closes);
  return 0;
}
