#define _GNU_SOURCE
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/socket.h>

#ifndef CETRA_SOURCE
#error CETRA_SOURCE must name a private source snapshot.
#endif

static _Noreturn void forbidden(const char *name) {
  fprintf(stderr, "FAIL: forbidden I/O: %s\n", name);
  abort();
}

static FILE *forbidden_fdopen(int fd, const char *mode) {
  (void)fd; (void)mode;
  forbidden("fdopen");
}
static int forbidden_mkostemp(char *path, int flags) {
  (void)path; (void)flags;
  forbidden("mkostemp");
}
static int forbidden_open(const char *path, int flags, ...) {
  (void)path; (void)flags;
  forbidden("open");
}
static int forbidden_socket(int domain, int type, int protocol) {
  (void)domain; (void)type; (void)protocol;
  forbidden("socket");
}

#define main cetra_main_never_called
#define fdopen forbidden_fdopen
#define mkostemp forbidden_mkostemp
#define open forbidden_open
#define socket forbidden_socket
#include CETRA_SOURCE
#undef main
#undef fdopen
#undef mkostemp
#undef open
#undef socket

/* Implement every declaration in lighting-safety/hidapi/hidapi.h locally. */
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

static void record(const char *event, const struct device_state *state) {
  char json[1024];
  format_state(json, sizeof(json), state);
  size_t length = strlen(json);
  if (!length || json[length - 1] != '\n') abort();
  json[length - 1] = '\0';
  printf("{\"event\":\"%s\",\"int_max\":%d,\"state\":%s}\n", event, INT_MAX, json);
}

static void packet_event(const char *event, struct device_state *state,
                         const unsigned char *packet, int size) {
  /* Exact allocations expose reads past the reported length under ASan. */
  unsigned char *exact = size ? malloc((size_t)size) : NULL;
  if (size && !exact) abort();
  if (size) memcpy(exact, packet, (size_t)size);
  apply_packet(NULL, state, exact, size);
  free(exact);
  record(event, state);
}

int main(void) {
  logging_enabled = false;
  const struct device_state initial = {
    .left = -1, .right = -1, .case_level = -1, .mode = -1,
    .anc_level = 3, .voice_prompt = 1, .proximity = 1,
    .lighting = -1, .lighting_r = 255,
  };
  struct device_state state = initial;
  const unsigned char gestures[][8] = {
    {0xcc, 0x70, 0, 0, 0, 0, 1, 0},
    {0xcc, 0x70, 0, 0, 0, 0, 2, 0},
    {0xcc, 0x70, 0, 0, 0, 0, 3, 0},
    {0xcc, 0x70, 0, 0, 0, 0, 0, 1},
    {0xcc, 0x70, 0, 0, 0, 1, 1, 0},
    {0xcc, 0x70, 0, 0, 0, 1, 2, 0},
    {0xcc, 0x70, 0, 0, 0, 1, 3, 0},
    {0xcc, 0x70, 0, 0, 0, 1, 0, 1},
  };
  const unsigned char battery[] = {0xcc, 0x12, 7, 0, 0, 5, 91, 98, 100};
  const unsigned char missing[] = {0xcc, 0x12, 7, 0, 0, 0, 255, 255, 100};
  const unsigned char consumer[] = {0x0c, 0x08};
  const unsigned char telephony[] = {0x05, 0x01};
  char event[80];

  record("startup", &state);
  for (int i = 0; i < 8; i++) {
    snprintf(event, sizeof(event), "media_gesture_%d", i);
    packet_event(event, &state, gestures[i], 8);
  }
  packet_event("media_consumer", &state, consumer, sizeof(consumer));
  packet_event("telephony_not_readback", &state, telephony, sizeof(telephony));
  state.call_context = true;
  record("call_on", &state);
  for (int i = 0; i < 8; i++) {
    if (i == 4) continue;
    snprintf(event, sizeof(event), "call_other_gesture_%d", i);
    packet_event(event, &state, gestures[i], 8);
  }
  packet_event("right_call", &state, gestures[4], 8);
  packet_event("right_duplicate", &state, gestures[4], 8);
  packet_event("reordered_double", &state, gestures[5], 8);
  packet_event("reordered_right", &state, gestures[4], 8);
  packet_event("reordered_left", &state, gestures[0], 8);
  /* A dropped physical edge has no report to feed and cannot advance tap_seq. */
  record("missing_report", &state);
  packet_event("after_missing", &state, gestures[4], 8);
  state.call_context = false;
  record("call_off", &state);
  packet_event("right_media_after_call", &state, gestures[4], 8);
  state.call_context = true;
  record("call_on_again", &state);
  reset_receiver_state(&state);
  record("receiver_reset", &state);
  packet_event("reconnect_battery", &state, battery, sizeof(battery));
  packet_event("missing_battery_1", &state, missing, sizeof(missing));
  packet_event("missing_battery_2", &state, missing, sizeof(missing));
  packet_event("return_battery", &state, battery, sizeof(battery));

  struct device_state restarted = initial;
  record("restart_new_struct", &restarted);
  restarted.call_context = true;
  record("restart_call_on", &restarted);
  packet_event("restart_right_call", &restarted, gestures[4], 8);

  /* Non-NULL is an inert sentinel, never a real opened handle. */
  hid_device sentinel = {0};
  const char *chunks[] = {
    "mic_state muted\n", "mic_state live\n",
    "mic_state muted\r\nmic_state live\n", "mic_sta", "te muted\n",
  };
  for (int context = 0; context < 2; context++) {
    for (int handle = 0; handle < 2; handle++) {
      struct device_state ignored = initial;
      ignored.call_context = context != 0;
      ignored.tap_seq = 11 + context;
      struct command_source source = {.fd = -1, .call_requested = context != 0};
      snprintf(event, sizeof(event), "obsolete_%d_%d_before", context, handle);
      record(event, &ignored);
      for (int i = 0; i < 5; i++) {
        if (!consume_commands(handle ? &sentinel : NULL, &ignored, &source,
                              chunks[i], strlen(chunks[i]))) abort();
        if (source.call_requested != (context != 0) || source.overflow ||
            source.length != (i == 3 ? strlen(chunks[i]) : 0)) abort();
        snprintf(event, sizeof(event), "obsolete_%d_%d_chunk_%d", context, handle, i);
        record(event, &ignored);
      }
    }
  }

  const int short_gestures[] = {4, 1, 7, 0};
  for (int fixture = 0; fixture < 4; fixture++) {
    for (int size = 0; size <= 8; size++) {
      struct device_state short_state = initial;
      short_state.call_context = true;
      short_state.tap_seq = 7;
      snprintf(event, sizeof(event), "short_%d_%d", fixture, size);
      packet_event(event, &short_state, gestures[short_gestures[fixture]], size);
    }
  }
  state = initial;
  state.call_context = true;
  state.tap_seq = INT_MAX - 1;
  record("saturation_before", &state);
  packet_event("saturation_reach", &state, gestures[4], 7);
  packet_event("saturation_repeat", &state, gestures[4], 7);
  packet_event("saturation_full", &state, gestures[4], 8);
  return 0;
}
