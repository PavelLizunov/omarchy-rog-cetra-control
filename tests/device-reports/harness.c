#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#ifndef CETRA_SOURCE
#error CETRA_SOURCE must name a private source snapshot.
#endif

static long test_ms;
static int emitting;

static _Noreturn void forbidden(const char *name) {
  fprintf(stderr, "FAIL: forbidden I/O: %s\n", name);
  abort();
}

static int test_clock_gettime(clockid_t clock, struct timespec *ts) {
  if (clock != CLOCK_MONOTONIC) forbidden("non-monotonic clock");
  ts->tv_sec = test_ms / 1000;
  ts->tv_nsec = (test_ms % 1000) * 1000000L;
  return 0;
}

static int test_mkostemp(char *path, int flags) {
  (void)path; (void)flags;
  /* Exercise emit_state's real buffer without writing a runtime cache. */
  if (emitting) { errno = EACCES; return -1; }
  forbidden("mkostemp");
}
static FILE *test_fdopen(int fd, const char *mode) {
  (void)fd; (void)mode;
  forbidden("fdopen");
}
static int test_open(const char *path, int flags, ...) {
  (void)path; (void)flags;
  forbidden("open");
}
static int test_socket(int domain, int type, int protocol) {
  (void)domain; (void)type; (void)protocol;
  forbidden("socket");
}

#define main cetra_main_never_called
#define clock_gettime test_clock_gettime
#define mkostemp test_mkostemp
#define fdopen test_fdopen
#define open test_open
#define socket test_socket
#include CETRA_SOURCE
#undef main
#undef clock_gettime
#undef mkostemp
#undef fdopen
#undef open
#undef socket

int hid_init(void) { forbidden("hid_init"); }
int hid_exit(void) { forbidden("hid_exit"); }
struct hid_device_info *hid_enumerate(unsigned short vendor, unsigned short product) {
  (void)vendor; (void)product;
  forbidden("hid_enumerate");
}
void hid_free_enumeration(struct hid_device_info *devices) {
  (void)devices;
  forbidden("hid_free_enumeration");
}
hid_device *hid_open_path(const char *path) {
  (void)path;
  forbidden("hid_open_path");
}
void hid_close(hid_device *device) {
  (void)device;
  forbidden("hid_close");
}
int hid_write(hid_device *device, const unsigned char *data, size_t length) {
  (void)device; (void)data; (void)length;
  forbidden("hid_write");
}
int hid_send_output_report(hid_device *device, const unsigned char *data, size_t length) {
  (void)device; (void)data; (void)length;
  forbidden("hid_send_output_report");
}
int hid_read_timeout(hid_device *device, unsigned char *data, size_t length, int timeout) {
  (void)device; (void)data; (void)length; (void)timeout;
  forbidden("hid_read_timeout");
}

static void record(const char *event, const struct device_state *state, int emit) {
  char json[1024];
  memset(json, 0xa5, sizeof(json));
  format_state(json, sizeof(json), state);
  size_t length = strnlen(json, sizeof(json));
  if (!length || length == sizeof(json) || json[length - 1] != '\n') {
    fprintf(stderr, "FAIL: %s: format_state truncated or unterminated\n", event);
    exit(1);
  }
  if (emit) {
    FILE *capture = fopen("emit-stdout.tmp", "w+");
    if (!capture || fflush(stdout)) exit(1);
    int saved = dup(STDOUT_FILENO);
    if (saved < 0 || dup2(fileno(capture), STDOUT_FILENO) < 0) exit(1);
    struct command_source clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) clients[i] = (struct command_source){.fd = -1};
    char last[1024] = {0};
    emitting = 1;
    emit_state(state, clients, last, sizeof(last));
    emit_state(state, clients, last, sizeof(last));
    emitting = 0;
    if (fflush(stdout) || dup2(saved, STDOUT_FILENO) < 0) exit(1);
    close(saved);
    if (fseek(capture, 0, SEEK_SET)) exit(1);
    char actual[2048];
    size_t count = fread(actual, 1, sizeof(actual), capture);
    if (ferror(capture) || count != length || memcmp(actual, json, length) ||
        strcmp(last, json)) {
      fprintf(stderr, "FAIL: %s: emit_state truncated, changed or duplicated JSON\n", event);
      exit(1);
    }
    fclose(capture);
  }
  json[length - 1] = '\0';
  printf("{\"event\":\"%s\",\"line_bytes\":%zu,\"int_max\":%d,\"state\":%s}\n",
         event, length, INT_MAX, json);
}

int main(void) {
  logging_enabled = false;
  struct device_state state = {0};
  hid_device sentinel = {0};
  char input[256], event[80], action[16], value[129];
  while (fgets(input, sizeof(input), stdin)) {
    value[0] = '\0';
    if (sscanf(input, "%79s %ld %15s %128s", event, &test_ms, action, value) < 3) return 2;
    if (!strcmp(action, "start")) {
      /* New observations must use aggregate zero initialization only. */
      state = (struct device_state){0};
      state.left = 37; state.right = 82; state.case_level = 100;
      state.mode = 2; state.anc_level = 3; state.voice_prompt = 1;
      state.proximity = 1; state.lighting = -1;
      state.call_context = !strcmp(value, "call");
      state.tap_seq = 19;
      if (!strcmp(value, "max")) {
        state.left = -1; state.right = -1; state.case_level = -1;
        state.mode = -1; state.anc_level = INT_MIN; state.proximity = 0;
        state.tap_seq = INT_MAX;
      }
    } else if (!strcmp(action, "reset")) {
      reset_receiver_state(&state);
    } else if (!strcmp(action, "packet")) {
      size_t size = !strcmp(value, "-") ? 0 : strlen(value) / 2;
      if (size > 64 || (size && strlen(value) != size * 2)) return 2;
      /* ASan sees the exact reported boundary, not a padded 64-byte array. */
      unsigned char *packet = size ? malloc(size) : NULL;
      if (size && !packet) return 2;
      for (size_t i = 0; i < size; i++) {
        unsigned int byte;
        if (sscanf(value + i * 2, "%2x", &byte) != 1) return 2;
        packet[i] = (unsigned char)byte;
      }
      apply_packet(&sentinel, &state, packet, (int)size);
      free(packet);
    } else if (strcmp(action, "record") && strcmp(action, "emit")) {
      return 2;
    }
    record(event, &state, !strcmp(action, "emit"));
  }
  return ferror(stdin) ? 2 : 0;
}
