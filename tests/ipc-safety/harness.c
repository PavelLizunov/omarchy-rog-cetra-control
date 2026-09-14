#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "../private-files.h"

static ssize_t test_send(int, const void *, size_t, int);
static int test_close(int);
#define main daemon_main_not_called
#define send test_send
#define close test_close
#define open private_open
#define openat private_openat
#define mkostemp private_mkostemp
#define fdopen private_fdopen
#include CETRA_SOURCE
#undef main
#undef send
#undef close
#undef open
#undef openat
#undef mkostemp
#undef fdopen
#include "../security-hid.h"

static int script[16], calls, closed;
static size_t sent;
static const char *message;
static ssize_t test_send(int fd, const void *data, size_t size, int flags) {
  assert(fd == 999 && flags == (MSG_DONTWAIT | MSG_NOSIGNAL) && calls < 16);
  assert(size == strlen(message) - sent && !memcmp(data, message + sent, size));
  int result = script[calls++];
  if (result < 0) { errno = -result; return -1; }
  size_t count = result == INT_MAX ? size : (size_t)result;
  assert(count <= size);
  sent += count;
  return (ssize_t)count;
}
static int test_close(int fd) {
  if (fd == 999) { closed++; return 0; }
  return private_close(fd);
}

static int checked;
static void frame(const char *data, size_t size, int output, int value, int query, int call) {
  for (size_t split = 0; split <= size; split++) {
    struct device_state state = {.mode = -1, .lighting = -1};
    struct command_source source = {.fd = -1, .call_requested = call <= 0};
    struct device_state before = state;
    hid_outputs = hid_queries = 0;
    assert(consume_commands(&test_device, &state, &source, data, split));
    assert(consume_commands(&test_device, &state, &source, data + split, size - split));
    assert(!source.length && !source.overflow);
    assert(source.call_requested == (call != 0));
    assert(hid_outputs == !!output && hid_queries == !!query);
    if (output) {
      unsigned char expected[64] = {0xcc, 0x41, output, 0, 0, value};
      unsigned char request[17] = {0, 0xcc, 0x12, query};
      assert(!memcmp(last_output, expected, sizeof(expected)));
      assert(!memcmp(last_query, request, sizeof(request)));
    } else assert(!memcmp(&state, &before, sizeof(state)));
    checked++;
  }
}

int main(void) {
  struct stat st;
  const char *root = getenv("XDG_RUNTIME_DIR");
  private_path(root);
  assert(!stat(root, &st) && (st.st_mode & 0777) == 0700);
  const struct { const char *text; int op, value, query, call; } valid[] = {
    {"mode anc\n", 8, 1, 0x25, -1}, {"mode ambient\r\n", 8, 2, 0x25, -1},
    {"mode off \t\v\f\n", 8, 0, 0x25, -1},
    {"anc_level 1\n", 12, 1, 0x2b, -1}, {"anc_level 002\n", 12, 2, 0x2b, -1},
    {" \tanc_level\t3 \t\r\n", 12, 3, 0x2b, -1},
    {"anc_adaptive on\n", 13, 1, 0x2c, -1}, {"anc_adaptive true\n", 13, 1, 0x2c, -1},
    {"anc_adaptive off\n", 13, 0, 0x2c, -1}, {"anc_adaptive false\n", 13, 0, 0x2c, -1},
    {"proximity on\n", 9, 1, 0x26, -1}, {"proximity true\n", 9, 1, 0x26, -1},
    {"proximity off\n", 9, 0, 0x26, -1}, {"proximity false\n", 9, 0, 0x26, -1},
    {"voice_prompt english\n", 10, 1, 0x28, -1}, {"voice_prompt chinese\n", 10, 2, 0x28, -1},
    {"voice_prompt sound\n", 10, 0, 0x28, -1}, {"voice_prompt beeps\n", 10, 0, 0x28, -1},
    {"call on\r\n", 0, 0, 0, 1}, {"call off\n", 0, 0, 0, 0},
  };
  for (size_t i = 0; i < sizeof(valid) / sizeof(valid[0]); i++)
    frame(valid[i].text, strlen(valid[i].text), valid[i].op, valid[i].value, valid[i].query, valid[i].call);
  const char *invalid[] = {
    "mode anc junk\n", "mode off on\n", "call off junk\n", "voice_prompt sound extra\n",
    "anc_level 2junk\n", "anc_level +2\n", "anc_level -1\n", "anc_level 0\n", "anc_level 4\n",
    "anc_level 0x2\n", "anc_level 2.0\n", "anc_level 2 extra\n", "anc_level 4294967297\n",
    "anc_level 18446744073709551617\n", "anc_level 999999999999999999999999999999999999\n",
    "anc_adaptive garbage\n", "anc_adaptive 1\n", "anc_adaptive true extra\n",
    "proximity yes\n", "proximity 0\n", "proximity off extra\n", "proximity FALSE\n",
    "call invalid\n", "mode invalid\n", "voice_prompt invalid\n", "unknown on\n", "\n", "mode\n",
    "mo\rde anc\n", "mode\ranc\n", "mode anc\rjunk\n", "mode anc\r\r\n",
    "call o\rff\n", "lighting off\r junk\n",
  };
  for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
    frame(invalid[i], strlen(invalid[i]), 0, 0, 0, -1);
  const char nul[] = "anc_level 2\0junk\ncall off\0\n";
  frame(nul, sizeof(nul) - 1, 0, 0, 0, -1);
  char overflow[512];
  memset(overflow, ' ', sizeof(overflow));
  memcpy(overflow, "call off", 8);
  overflow[sizeof(overflow) - 1] = '\n';
  frame(overflow, sizeof(overflow), 0, 0, 0, -1);
  const char recovery[] = "call off\0\ncall on\r\ncall off\n";
  frame(recovery, sizeof(recovery) - 1, 0, 0, 0, 0);
  struct command_source a = {.fd = -1}, b = {.fd = -1};
  struct device_state state = {0};
  assert(consume_commands(NULL, &state, &a, "call on\0", 8));
  assert(consume_commands(NULL, &state, &b, "call on\n", 8) && b.call_requested);
  assert(consume_commands(NULL, &state, &a, "\n", 1) && !a.call_requested);

  message = "abcdef\n";
  const struct { int steps[8]; bool ok; int count; } sends[] = {
    {{INT_MAX}, true, 1}, {{1, 2, INT_MAX}, true, 3},
    {{-EINTR, -EINTR, -EINTR, INT_MAX}, true, 4},
    {{-EINTR, -EINTR, -EINTR, -EINTR, INT_MAX}, false, 4},
    {{1, -EINTR, 2, INT_MAX}, true, 4}, {{1, -EAGAIN}, false, 2},
    {{-EPIPE}, false, 1}, {{0}, false, 1},
  };
  for (size_t i = 0; i < sizeof(sends) / sizeof(sends[0]); i++) {
    memcpy(script, sends[i].steps, sizeof(sends[i].steps));
    calls = 0; sent = 0;
    assert(send_line(999, message) == sends[i].ok && calls == sends[i].count);
    if (sends[i].ok) assert(sent == strlen(message));
  }
  struct command_source clients[MAX_CLIENTS];
  for (int i = 0; i < MAX_CLIENTS; i++) clients[i] = (struct command_source){.fd = -1};
  clients[0] = (struct command_source){.fd = 999, .call_requested = true};
  char line[STATUS_BUFFER_SIZE], last[STATUS_BUFFER_SIZE] = {0};
  format_state(line, sizeof(line), &state);
  message = line; calls = 0; sent = 0;
  script[0] = 1; script[1] = -EAGAIN;
  emit_state(&state, clients, last, sizeof(last));
  assert(closed == 1 && clients[0].fd == -1 && !clients[0].call_requested);
  assert(!hid_inits);
  printf("IPC safety: %d split-frame cases, 8 send cases, client disconnect passed\n", checked);
  return 0;
}
