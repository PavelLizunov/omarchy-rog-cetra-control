// Private state shared by the single owner translation unit.
#define _GNU_SOURCE
#include <hidapi/hidapi.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define VENDOR_ID 0x0b05
#define PRODUCT_ID 0x1ad3
#define HID_INTERFACE 3
#define MAX_CLIENTS 8
#define COMMAND_BUFFER_SIZE 256
#define STATUS_BUFFER_SIZE 1024
#define DEVICE_REPORT_FRESH_MS 30000L

struct observed_report {
  bool seen;
  unsigned char raw;
  unsigned char extra;
  long received_ms;
};

struct device_state {
  bool receiver;
  bool connected;
  int left;
  int right;
  int case_level;
  int left_missing;
  int right_missing;
  int mode;
  int mode_desired;
  int mode_queries_left;
  long mode_query_at;
  long mode_query_deadline;
  int anc_level;
  bool anc_adaptive;
  int voice_prompt;
  int proximity;
  struct observed_report anc_level_report;
  struct observed_report anc_adaptive_report;
  struct observed_report voice_prompt_report;
  struct observed_report proximity_report;
  int lighting;
  int lighting_r;
  int lighting_g;
  int lighting_b;
  bool lighting_desired_valid;
  bool call_context;
  int tap_seq;
  struct observed_report presence_report;
  struct observed_report charging_report;
};

struct command_source {
  int fd;
  bool call_requested;
  bool overflow;
  char buffer[COMMAND_BUFFER_SIZE];
  size_t length;
};

static volatile sig_atomic_t keep_running = 1;
static void stop_running(int signal_number) {
  (void)signal_number;
  keep_running = 0;
}
static long monotonic_ms(void) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return now.tv_sec * 1000L + now.tv_nsec / 1000000L;
}
