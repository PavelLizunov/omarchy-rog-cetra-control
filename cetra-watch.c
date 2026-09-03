#define _GNU_SOURCE

#include <hidapi/hidapi.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
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

struct device_state {
  bool receiver;
  bool connected;
  int left;
  int right;
  int case_level;
  int left_missing;
  int right_missing;
  int mode;
  int anc_level;
  bool anc_adaptive;
  int voice_prompt;
  int proximity;
  int lighting;
  int lighting_r;
  int lighting_g;
  int lighting_b;
  bool call_context;
  int tap_seq;
  bool mic_live;
};

struct command_source {
  int fd;
  bool call_requested;
  bool overflow;
  char buffer[COMMAND_BUFFER_SIZE];
  size_t length;
};

static volatile sig_atomic_t keep_running = 1;

static bool aggregate_call_requested(const struct command_source *owner_source, const struct command_source *clients);
static bool consume_commands(hid_device *device, struct device_state *state, struct command_source *source, const char *data, size_t size);

static void stop_running(int signal_number) {
  (void)signal_number;
  keep_running = 0;
}

static long monotonic_ms(void) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return now.tv_sec * 1000L + now.tv_nsec / 1000000L;
}

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

static const char *prompt_name(int val) {
  if (val == 1) return "english";
  if (val == 2) return "chinese";
  if (val == 0) return "sound";
  return "unknown";
}

static int parse_prompt(const char *name) {
  if (strcmp(name, "english") == 0) return 1;
  if (strcmp(name, "chinese") == 0) return 2;
  if (strcmp(name, "sound") == 0 || strcmp(name, "beeps") == 0) return 0;
  return -1;
}

static const char *lighting_name(int val) {
  if (val == 0) return "off";
  if (val == 1) return "static";
  if (val == 2) return "breathing";
  if (val == 3) return "strobing";
  if (val == 4) return "cycle";
  return "unknown";
}

static int parse_lighting(const char *name) {
  if (strcmp(name, "off") == 0) return 0;
  if (strcmp(name, "static") == 0) return 1;
  if (strcmp(name, "breathing") == 0) return 2;
  if (strcmp(name, "strobing") == 0) return 3;
  if (strcmp(name, "cycle") == 0 || strcmp(name, "colorcycle") == 0) return 4;
  return -1;
}

static void runtime_path(char *target, size_t size, const char *name) {
  const char *runtime = getenv("XDG_RUNTIME_DIR");
  if (!runtime || !*runtime) runtime = "/tmp";
  snprintf(target, size, "%s/%s", runtime, name);
}

static hid_device *open_receiver(void) {
  struct hid_device_info *devices = hid_enumerate(VENDOR_ID, PRODUCT_ID);
  hid_device *device = NULL;
  for (struct hid_device_info *item = devices; item; item = item->next) {
    if (item->interface_number == HID_INTERFACE) {
      device = hid_open_path(item->path);
      break;
    }
  }
  hid_free_enumeration(devices);
  return device;
}

static bool send_request(hid_device *device, unsigned char command_id) {
  unsigned char command[16] = {0xcc, 0x12, command_id};
  unsigned char report[17] = {0};
  memcpy(report + 1, command, sizeof(command));
  return hid_write(device, report, sizeof(report)) == (int)sizeof(report);
}

static bool set_mode(hid_device *device, int mode) {
  unsigned char report[64] = {0};
  report[0] = 0xcc;
  report[1] = 0x41;
  report[2] = 0x08;
  report[5] = (unsigned char)mode;
  return hid_send_output_report(device, report, sizeof(report)) == (int)sizeof(report);
}

static bool set_anc_level(hid_device *device, int level) {
  unsigned char report[64] = {0};
  report[0] = 0xcc;
  report[1] = 0x41;
  report[2] = 0x0c;
  report[5] = (unsigned char)level;
  return hid_send_output_report(device, report, sizeof(report)) == (int)sizeof(report);
}

static bool set_anc_adaptive(hid_device *device, bool enabled) {
  unsigned char report[64] = {0};
  report[0] = 0xcc;
  report[1] = 0x41;
  report[2] = 0x0d;
  report[5] = enabled ? 1 : 0;
  return hid_send_output_report(device, report, sizeof(report)) == (int)sizeof(report);
}

static bool set_voice_prompt(hid_device *device, int val) {
  unsigned char report[64] = {0};
  report[0] = 0xcc;
  report[1] = 0x41;
  report[2] = 0x0a;
  report[5] = (unsigned char)val;
  return hid_send_output_report(device, report, sizeof(report)) == (int)sizeof(report);
}

static bool set_proximity(hid_device *device, bool enabled) {
  unsigned char report[64] = {0};
  report[0] = 0xcc;
  report[1] = 0x41;
  report[2] = 0x09;
  report[5] = enabled ? 1 : 0;
  return hid_send_output_report(device, report, sizeof(report)) == (int)sizeof(report);
}

static bool set_lighting(hid_device *device, int effect, unsigned char r, unsigned char g, unsigned char b) {
  unsigned char report1[64] = {0};
  report1[0] = 0xcc;
  report1[1] = 0x51;
  report1[2] = 0x28;
  if (effect > 0) {
    report1[5] = 0x01;
    report1[6] = (unsigned char)effect;
    report1[7] = r;
    report1[8] = g;
    report1[9] = b;
  } else {
    report1[5] = 0x01;
    report1[6] = 0x01;
    report1[7] = 0x00;
    report1[8] = 0x00;
    report1[9] = 0x00;
  }
  if (hid_send_output_report(device, report1, sizeof(report1)) != (int)sizeof(report1)) return false;

  report1[5] = 0x00;
  if (hid_send_output_report(device, report1, sizeof(report1)) != (int)sizeof(report1)) return false;

  unsigned char report2[64] = {0};
  report2[0] = 0xcc;
  report2[1] = 0x50;
  report2[2] = 0x55;
  if (hid_send_output_report(device, report2, sizeof(report2)) != (int)sizeof(report2)) return false;
  return hid_send_output_report(device, report2, sizeof(report2)) == (int)sizeof(report2);
}

static bool set_call_context(hid_device *device, bool active) {
  unsigned char report[2] = {0x05, 0x00};
  if (active) report[1] = 0x31;
  return hid_send_output_report(device, report, sizeof(report)) == (int)sizeof(report);
}

static void reset_receiver_state(struct device_state *state) {
  state->receiver = false;
  state->connected = false;
  state->left = -1;
  state->right = -1;
  state->case_level = -1;
  state->left_missing = 0;
  state->right_missing = 0;
  state->mode = -1;
  state->anc_level = 3;
  state->anc_adaptive = false;
  state->voice_prompt = 1;
  state->proximity = 1;
  state->lighting = 0;
  state->lighting_r = 0xff;
  state->lighting_g = 0x00;
  state->lighting_b = 0x00;
  state->mic_live = true;
}

static void apply_packet(struct device_state *state, const unsigned char *packet, int size) {
  if (size >= 9 && packet[0] == 0xcc && packet[1] == 0x12 && packet[2] == 0x07) {
    state->receiver = true;
    if (packet[6] <= 100) {
      state->left = packet[6];
      state->left_missing = 0;
    } else {
      state->left_missing++;
      if (state->left_missing >= 2) state->left = -1;
    }
    if (packet[7] <= 100) {
      state->right = packet[7];
      state->right_missing = 0;
    } else {
      state->right_missing++;
      if (state->right_missing >= 2) state->right = -1;
    }
    state->case_level = packet[8] <= 100 ? packet[8] : -1;
    state->connected = state->left >= 0 || state->right >= 0;
  } else if (size >= 6 && packet[0] == 0xcc && packet[1] == 0x12 && packet[2] == 0x25) {
    if (packet[5] <= 2) state->mode = packet[5];
  } else if (size >= 6 && packet[0] == 0xcc && packet[1] == 0x12 && packet[2] == 0x2b) {
    if (packet[5] >= 1 && packet[5] <= 3) state->anc_level = packet[5];
  } else if (size >= 6 && packet[0] == 0xcc && packet[1] == 0x12 && packet[2] == 0x2c) {
    state->anc_adaptive = packet[5] != 0;
  } else if (size >= 6 && packet[0] == 0xcc && packet[1] == 0x12 && packet[2] == 0x28) {
    if (packet[5] <= 2) state->voice_prompt = packet[5];
  } else if (size >= 6 && packet[0] == 0xcc && packet[1] == 0x12 && packet[2] == 0x26) {
    state->proximity = packet[5] != 0 ? 1 : 0;
  } else if (size >= 7 && packet[0] == 0xcc && packet[1] == 0x70) {
    state->tap_seq++;
    state->mic_live = !state->mic_live;
  }
}

static int selftest(void) {
  struct device_state state = {
    .left = -1,
    .right = -1,
    .case_level = -1,
    .left_missing = 0,
    .right_missing = 0,
    .mode = -1,
    .anc_level = 3,
    .anc_adaptive = false,
    .voice_prompt = 1,
    .proximity = 1,
    .lighting = 0,
    .call_context = true,
    .tap_seq = 0,
    .mic_live = true,
  };
  const unsigned char battery[] = {0xcc, 0x12, 0x07, 0, 0, 5, 91, 98, 100};
  const unsigned char mode[] = {0xcc, 0x12, 0x25, 0, 0, 2};
  const unsigned char anc_lvl[] = {0xcc, 0x12, 0x2b, 0, 0, 2};
  const unsigned char anc_adp[] = {0xcc, 0x12, 0x2c, 0, 0, 1};
  const unsigned char prompt[] = {0xcc, 0x12, 0x28, 0, 0, 1};
  apply_packet(&state, battery, sizeof(battery));
  apply_packet(&state, mode, sizeof(mode));
  apply_packet(&state, anc_lvl, sizeof(anc_lvl));
  apply_packet(&state, anc_adp, sizeof(anc_adp));
  apply_packet(&state, prompt, sizeof(prompt));
  if (!state.receiver || !state.connected) return 1;
  if (state.left != 91 || state.right != 98 || state.case_level != 100) return 1;
  if (state.mode != 2 || state.anc_level != 2 || !state.anc_adaptive || state.voice_prompt != 1) return 1;

  const unsigned char in_case[] = {0xcc, 0x12, 0x07, 0, 0, 0, 255, 255, 100};
  for (int i = 0; i < 6; i++) apply_packet(&state, in_case, sizeof(in_case));
  if (state.connected) return 1;

  struct command_source owner_source = {.fd = STDIN_FILENO};
  struct command_source clients[MAX_CLIENTS];
  for (int i = 0; i < MAX_CLIENTS; i++) clients[i] = (struct command_source){.fd = -1};
  if (!consume_commands(NULL, &state, &owner_source, "ca", 2)) return 1;
  if (!consume_commands(NULL, &state, &owner_source, "ll on\n", 6)) return 1;
  clients[0] = (struct command_source){.fd = 42, .call_requested = true};
  if (!aggregate_call_requested(&owner_source, clients)) return 1;
  if (!consume_commands(NULL, &state, &owner_source, "call off\n", 9)) return 1;
  if (!aggregate_call_requested(&owner_source, clients)) return 1;
  clients[0].call_requested = false;
  if (aggregate_call_requested(&owner_source, clients)) return 1;
  puts("ok");
  return 0;
}

static void format_state(char *line, size_t size, const struct device_state *state) {
  char left[8];
  char right[8];
  char case_level[8];
  snprintf(left, sizeof(left), state->left >= 0 ? "%d" : "null", state->left);
  snprintf(right, sizeof(right), state->right >= 0 ? "%d" : "null", state->right);
  snprintf(case_level, sizeof(case_level), state->case_level >= 0 ? "%d" : "null", state->case_level);
  snprintf(line, size,
      "{\"status\":\"ok\",\"receiver\":%s,\"connected\":%s,"
      "\"left\":%s,\"right\":%s,\"case\":%s,\"mode\":\"%s\","
      "\"anc_level\":%d,\"anc_adaptive\":%s,\"voice_prompt\":\"%s\","
      "\"proximity\":%s,\"lighting\":\"%s\","
      "\"call_context\":%s,\"tap_seq\":%d,\"mic_live\":%s}\n",
      state->receiver ? "true" : "false", state->connected ? "true" : "false",
      left, right, case_level, mode_name(state->mode),
      state->anc_level, state->anc_adaptive ? "true" : "false",
      prompt_name(state->voice_prompt),
      state->proximity ? "true" : "false",
      lighting_name(state->lighting),
      state->call_context ? "true" : "false",
      state->tap_seq,
      state->mic_live ? "true" : "false");
}

static void write_state_cache(const char *line) {
  char path[512];
  char temp[1024];
  runtime_path(path, sizeof(path), "rog-cetra-control.status");
  snprintf(temp, sizeof(temp), "%s.%ld", path, (long)getpid());
  FILE *file = fopen(temp, "w");
  if (!file) return;
  fputs(line, file);
  if (fclose(file) != 0) {
    unlink(temp);
    return;
  }
  chmod(temp, 0600);
  if (rename(temp, path) != 0) unlink(temp);
}

static bool send_line(int fd, const char *line) {
  size_t length = strlen(line);
  ssize_t written = send(fd, line, length, MSG_DONTWAIT | MSG_NOSIGNAL);
  return written == (ssize_t)length;
}

static void emit_state(const struct device_state *state, struct command_source *clients, char *last, size_t last_size) {
  char line[512];
  format_state(line, sizeof(line), state);
  if (strcmp(line, last) == 0) return;
  snprintf(last, last_size, "%s", line);
  write_state_cache(line);
  fputs(line, stdout);
  fflush(stdout);
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].fd < 0) continue;
    if (!send_line(clients[i].fd, line)) {
      close(clients[i].fd);
      clients[i] = (struct command_source){.fd = -1};
    }
  }
}

static bool aggregate_call_requested(const struct command_source *owner_source, const struct command_source *clients) {
  if (owner_source->call_requested) return true;
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].fd >= 0 && clients[i].call_requested) return true;
  }
  return false;
}

static bool sync_call_context(hid_device *device, struct device_state *state, bool requested, bool force) {
  bool changed = state->call_context != requested;
  if (changed) state->call_context = requested;
  if (!device || (!changed && !force)) return true;
  return set_call_context(device, requested);
}

static bool handle_command(hid_device *device, struct device_state *state, struct command_source *source, const char *command) {
  char key[32] = {0};
  char value[32] = {0};
  if (sscanf(command, "%31s %31s", key, value) != 2) return true;
  if (strcmp(key, "mode") == 0) {
    int mode = parse_mode(value);
    if (mode >= 0 && device) {
      if (!set_mode(device, mode) || !send_request(device, 0x25)) return false;
      state->mode = -1;
    }
  } else if (strcmp(key, "call") == 0) {
    if (strcmp(value, "on") == 0) source->call_requested = true;
    else if (strcmp(value, "off") == 0) source->call_requested = false;
  } else if (strcmp(key, "anc_level") == 0) {
    int level = atoi(value);
    if (level >= 1 && level <= 3 && device) {
      if (!set_anc_level(device, level) || !send_request(device, 0x2b)) return false;
    }
  } else if (strcmp(key, "anc_adaptive") == 0) {
    bool enabled = strcmp(value, "on") == 0 || strcmp(value, "true") == 0;
    if (device) {
      if (!set_anc_adaptive(device, enabled) || !send_request(device, 0x2c)) return false;
    }
  } else if (strcmp(key, "voice_prompt") == 0) {
    int val = parse_prompt(value);
    if (val >= 0 && device) {
      if (!set_voice_prompt(device, val) || !send_request(device, 0x28)) return false;
    }
  } else if (strcmp(key, "proximity") == 0) {
    bool enabled = strcmp(value, "on") == 0 || strcmp(value, "true") == 0;
    if (device) {
      if (!set_proximity(device, enabled) || !send_request(device, 0x26)) return false;
    }
  } else if (strcmp(key, "lighting") == 0) {
    char effect_str[32] = {0};
    int r = 0xff, g = 0x00, b = 0x00;
    int parsed = sscanf(command, "lighting %31s %d %d %d", effect_str, &r, &g, &b);
    int effect = parse_lighting(effect_str);
    if (effect >= 0 && device) {
      if (parsed < 4) { r = 0xff; g = 0x00; b = 0x00; }
      set_lighting(device, effect, (unsigned char)r, (unsigned char)g, (unsigned char)b);
      state->lighting = effect;
      state->lighting_r = r;
      state->lighting_g = g;
      state->lighting_b = b;
    }
  } else if (strcmp(key, "mic_state") == 0) {
    if (strcmp(value, "live") == 0) state->mic_live = true;
    else if (strcmp(value, "muted") == 0) state->mic_live = false;
  }
  return true;
}

static bool consume_commands(hid_device *device, struct device_state *state, struct command_source *source, const char *data, size_t size) {
  for (size_t i = 0; i < size; i++) {
    char byte = data[i];
    if (byte == '\n') {
      if (!source->overflow) {
        source->buffer[source->length] = '\0';
        if (!handle_command(device, state, source, source->buffer)) return false;
      }
      source->length = 0;
      source->overflow = false;
    } else if (byte != '\r') {
      if (source->length + 1 < sizeof(source->buffer)) source->buffer[source->length++] = byte;
      else source->overflow = true;
    }
  }
  return true;
}

static int connect_socket(const char *path) {
  int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0) return -1;
  struct sockaddr_un address = {0};
  address.sun_family = AF_UNIX;
  if (strlen(path) >= sizeof(address.sun_path)) {
    close(fd);
    return -1;
  }
  strcpy(address.sun_path, path);
  if (connect(fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
    close(fd);
    return -1;
  }
  return fd;
}

static int mirror(const char *socket_path) {
  int socket_fd = -1;
  for (int attempt = 0; keep_running && attempt < 40; attempt++) {
    socket_fd = connect_socket(socket_path);
    if (socket_fd >= 0) break;
    usleep(50000);
  }
  if (socket_fd < 0) return 1;

  while (keep_running) {
    struct pollfd fds[2] = {
      {.fd = socket_fd, .events = POLLIN},
      {.fd = STDIN_FILENO, .events = POLLIN},
    };
    int ready = poll(fds, 2, -1);
    if (ready < 0) {
      if (errno == EINTR) continue;
      break;
    }
    char buffer[512];
    if (fds[0].revents & POLLIN) {
      ssize_t size = read(socket_fd, buffer, sizeof(buffer));
      if (size <= 0) break;
      fwrite(buffer, 1, (size_t)size, stdout);
      fflush(stdout);
    }
    if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) break;
    if (fds[1].revents & POLLIN) {
      ssize_t size = read(STDIN_FILENO, buffer, sizeof(buffer));
      if (size <= 0) break;
      ssize_t written = send(socket_fd, buffer, (size_t)size, MSG_NOSIGNAL);
      if (written != size) break;
    }
    if (fds[1].revents & (POLLERR | POLLHUP | POLLNVAL)) break;
  }
  close(socket_fd);
  return keep_running ? 1 : 0;
}

static void disconnect_receiver(hid_device **device, struct device_state *state) {
  if (*device) hid_close(*device);
  *device = NULL;
  reset_receiver_state(state);
}

static int make_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL);
  if (flags < 0) return -1;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int owner(int server_fd) {
  struct device_state state = {
    .receiver = false,
    .connected = false,
    .left = -1,
    .right = -1,
    .case_level = -1,
    .left_missing = 0,
    .right_missing = 0,
    .mode = -1,
    .anc_level = 3,
    .anc_adaptive = false,
    .voice_prompt = 1,
    .proximity = 1,
    .lighting = 0,
    .call_context = false,
    .tap_seq = 0,
    .mic_live = true,
  };
  struct command_source owner_source = {.fd = STDIN_FILENO};
  struct command_source clients[MAX_CLIENTS];
  for (int i = 0; i < MAX_CLIENTS; i++) clients[i] = (struct command_source){.fd = -1};
  char last[512] = {0};
  hid_device *device = NULL;
  long next_open = 0;
  long next_query = 0;
  int query_phase = 0;
  emit_state(&state, clients, last, sizeof(last));

  while (keep_running) {
    struct pollfd fds[2 + MAX_CLIENTS];
    fds[0] = (struct pollfd){.fd = server_fd, .events = POLLIN};
    fds[1] = (struct pollfd){.fd = STDIN_FILENO, .events = POLLIN};
    for (int i = 0; i < MAX_CLIENTS; i++) {
      fds[2 + i] = (struct pollfd){.fd = clients[i].fd, .events = clients[i].fd >= 0 ? POLLIN : 0};
    }

    int ready = poll(fds, 2 + MAX_CLIENTS, 50);
    if (ready < 0 && errno != EINTR) break;
    if (ready > 0 && (fds[0].revents & POLLIN)) {
      while (true) {
        int client = accept4(server_fd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (client < 0) break;
        int slot = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
          if (clients[i].fd < 0) {
            slot = i;
            break;
          }
        }
        if (slot < 0 || (last[0] && !send_line(client, last))) close(client);
        else clients[slot] = (struct command_source){.fd = client};
      }
    }

    bool receiver_ok = true;
    char input[512];
    if (ready > 0 && (fds[1].revents & POLLIN)) {
      ssize_t size = read(STDIN_FILENO, input, sizeof(input));
      if (size > 0) receiver_ok = consume_commands(device, &state, &owner_source, input, (size_t)size);
      else keep_running = 0;
    }
    if (ready > 0 && (fds[1].revents & (POLLERR | POLLHUP | POLLNVAL))) keep_running = 0;

    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (clients[i].fd < 0 || ready <= 0) continue;
      short events = fds[2 + i].revents;
      if (events & POLLIN) {
        ssize_t size = recv(clients[i].fd, input, sizeof(input), 0);
        if (size > 0) receiver_ok = receiver_ok && consume_commands(device, &state, &clients[i], input, (size_t)size);
        else events |= POLLHUP;
      }
      if (events & (POLLERR | POLLHUP | POLLNVAL)) {
        close(clients[i].fd);
        clients[i] = (struct command_source){.fd = -1};
      }
    }

    bool requested = aggregate_call_requested(&owner_source, clients);
    if (!sync_call_context(device, &state, requested, false)) receiver_ok = false;
    if (!receiver_ok) {
      disconnect_receiver(&device, &state);
      next_open = monotonic_ms() + 1000;
    }

    long now = monotonic_ms();
    if (!device && now >= next_open) {
      device = open_receiver();
      if (device) {
        state.receiver = true;
        if (!sync_call_context(device, &state, requested, true)
            || !send_request(device, 0x07)
            || !send_request(device, 0x25)) {
          disconnect_receiver(&device, &state);
          next_open = now + 1000;
        } else {
          next_query = now + 500;
          query_phase = 0;
        }
      } else {
        next_open = now + 1000;
      }
    }

    if (device && now >= next_query) {
      if (!send_request(device, query_phase == 0 ? 0x07 : 0x25)) {
        disconnect_receiver(&device, &state);
        next_open = now + 1000;
      } else {
        query_phase = (query_phase + 1) % 2;
        next_query = now + 500;
      }
    }

    if (device) {
      unsigned char packet[64];
      int size = hid_read_timeout(device, packet, sizeof(packet), 0);
      if (size < 0) {
        disconnect_receiver(&device, &state);
        next_open = now + 1000;
      } else if (size > 0) {
        bool was_connected = state.connected;
        apply_packet(&state, packet, size);
        if (!was_connected && state.connected) {
          state.mic_live = true;
          if (state.call_context && !set_call_context(device, true)) {
            disconnect_receiver(&device, &state);
            next_open = now + 1000;
          } else {
            set_lighting(device, state.lighting, (unsigned char)state.lighting_r, (unsigned char)state.lighting_g, (unsigned char)state.lighting_b);
          }
        }
      }
    }
    emit_state(&state, clients, last, sizeof(last));
  }

  if (device) {
    if (state.call_context) set_call_context(device, false);
    hid_close(device);
  }
  state.call_context = false;
  reset_receiver_state(&state);
  emit_state(&state, clients, last, sizeof(last));
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].fd >= 0) close(clients[i].fd);
  }
  return 0;
}

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "--selftest") == 0) return selftest();
  const char *fixture = getenv("CETRA_STATUS_FIXTURE");
  if (fixture && *fixture) {
    puts(fixture);
    fflush(stdout);
    while (true) sleep(3600);
  }

  umask(0077);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGINT, stop_running);
  signal(SIGTERM, stop_running);

  char socket_path[512];
  char lock_path[512];
  runtime_path(socket_path, sizeof(socket_path), "rog-cetra-control.sock");
  runtime_path(lock_path, sizeof(lock_path), "rog-cetra-control.owner.lock");
  int lock_fd = open(lock_path, O_CREAT | O_RDWR | O_CLOEXEC, 0600);
  if (lock_fd < 0) return 1;
  if (flock(lock_fd, LOCK_EX | LOCK_NB) != 0) {
    close(lock_fd);
    return mirror(socket_path);
  }

  unlink(socket_path);
  int server_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (server_fd < 0) {
    close(lock_fd);
    return 1;
  }
  struct sockaddr_un address = {0};
  address.sun_family = AF_UNIX;
  if (strlen(socket_path) >= sizeof(address.sun_path)) {
    close(server_fd);
    close(lock_fd);
    return 1;
  }
  strcpy(address.sun_path, socket_path);
  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) != 0
      || chmod(socket_path, 0600) != 0
      || listen(server_fd, MAX_CLIENTS) != 0
      || make_nonblocking(server_fd) != 0) {
    close(server_fd);
    unlink(socket_path);
    close(lock_fd);
    return 1;
  }

  if (hid_init() != 0) {
    close(server_fd);
    unlink(socket_path);
    close(lock_fd);
    return 1;
  }
  int result = owner(server_fd);
  hid_exit();
  close(server_fd);
  unlink(socket_path);
  close(lock_fd);
  return result;
}
