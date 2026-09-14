// Single translation unit preserves private linkage and test interception.
#include "daemon/types.h"
#include "daemon/protocol.h"
#include "daemon/files.h"
#include "daemon/reports.h"
#include "daemon/commands.h"
#include "daemon/ipc.h"
#include "daemon/selftest.h"

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
    .lighting = -1,
    .lighting_r = 0xff,
    .lighting_g = 0x00,
    .lighting_b = 0x00,
    .call_context = false,
    .tap_seq = 0,
  };
  struct command_source owner_source = {.fd = STDIN_FILENO};
  struct command_source clients[MAX_CLIENTS];
  for (int i = 0; i < MAX_CLIENTS; i++) clients[i] = (struct command_source){.fd = -1};
  char last[STATUS_BUFFER_SIZE] = {0};
  hid_device *device = NULL;
  long next_open = 0;
  long next_query = 0;
  long next_presence = 0;
  long next_settings = 0;
  int settings_phase = 0;
  bool settings_polling = false;
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
      for (int admission = 0; admission < MAX_CLIENTS; admission++) {
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
        if (size > 0) {
          bool client_ok = consume_commands(receiver_ok ? device : NULL, &state, &clients[i], input, (size_t)size);
          receiver_ok = receiver_ok && client_ok;
        } else events |= POLLHUP;
      }
      if (events & (POLLERR | POLLHUP | POLLNVAL)) {
        close(clients[i].fd);
        clients[i] = (struct command_source){.fd = -1};
      }
    }

    bool requested = aggregate_call_requested(&owner_source, clients);
    if (!sync_call_context(receiver_ok ? device : NULL, &state, requested, false)) receiver_ok = false;
    if (!receiver_ok) {
      disconnect_receiver(&device, &state);
      next_open = monotonic_ms() + 1000;
    }

    long now = monotonic_ms();
    if (!device && now >= next_open) {
      device = open_receiver();
      if (device) {
        state.receiver = true;
        log_event("RECEIVER: opened 0x%04x:0x%04x (iface %d)", VENDOR_ID, PRODUCT_ID, HID_INTERFACE);
        if (!sync_call_context(device, &state, requested, true)
             || !send_request(device, 0x07)
             || !send_request(device, 0x25)
             || !send_request(device, 0x01)) {
          disconnect_receiver(&device, &state);
          next_open = now + 1000;
        } else {
          next_query = now + 500;
          next_presence = now + 10000;
          query_phase = 0;
          settings_polling = false;
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

    // Presence readback (HAL mutex_getTwsExist) is independent of battery/mode.
    if (device && now >= next_presence) {
      if (!send_request(device, 0x01)) {
        disconnect_receiver(&device, &state);
        next_open = now + 1000;
      } else {
        next_presence = now + 10000;
      }
    }

    // Bound ready-report draining so stdin and clients still get a turn.
    for (int reports = 0; device && keep_running && reports < 16; reports++) {
      unsigned char packet[64];
      int size = hid_read_timeout(device, packet, sizeof(packet), 0);
      if (size < 0) {
        disconnect_receiver(&device, &state);
        next_open = now + 1000;
      } else if (size > 0) {
        bool was_available = state.connected
            && !(state.presence_report.seen && state.presence_report.raw == 0);
        apply_packet(device, &state, packet, size);
        if (!state.connected || (state.presence_report.seen && state.presence_report.raw == 0))
          settings_polling = false;
        if (!state.connected) state.mode_queries_left = 0;
        if (!was_available && state.connected
            && !(state.presence_report.seen && state.presence_report.raw == 0)) {
          log_event("LIFECYCLE: earbud telemetry available, microphone_state=unknown, restoring call_context=%s, lighting_desired=%s",
              state.call_context ? "active" : "inactive",
              lighting_name(state.lighting_desired_valid ? state.lighting : -1));
          if (state.call_context && !set_call_context(device, true)) {
            disconnect_receiver(&device, &state);
            next_open = now + 1000;
          } else if (state.lighting_desired_valid
              && !set_lighting(device, state.lighting, (unsigned char)state.lighting_r, (unsigned char)state.lighting_g, (unsigned char)state.lighting_b)) {
            disconnect_receiver(&device, &state);
            next_open = monotonic_ms() + 1000;
          }
        }
      } else {
        break;
      }
      emit_state(&state, clients, last, sizeof(last));
    }
    // A reported absence remains a veto until a new presence report, even if
    // cached battery percentages arrive later. Never replay a settings write.
    if (!device || !state.connected || (state.presence_report.seen && state.presence_report.raw == 0)) {
      settings_polling = false;
    } else if (keep_running) {
      if (!settings_polling) {
        settings_phase = 0;
        settings_polling = true;
      }
      long query_now = monotonic_ms();
      if (query_now >= next_settings) {
        static const unsigned char settings_queries[] = {0x2b, 0x2c, 0x28, 0x26};
        next_settings = query_now + 2500;
        if (!send_request(device, settings_queries[settings_phase])) {
          disconnect_receiver(&device, &state);
          next_open = query_now + 1000;
          settings_polling = false;
        } else {
          settings_phase = (settings_phase + 1) % 4;
        }
      }
    }
    // The immediate readback can precede the device applying a mode change.
    // Retry only the read, without delaying input or replaying the setting write.
    if (!state.connected) state.mode_queries_left = 0;
    if (device && keep_running && state.mode_queries_left > 0) {
      long query_now = monotonic_ms();
      if (query_now >= state.mode_query_deadline) {
        state.mode_queries_left = 0;
      } else if (query_now >= state.mode_query_at) {
        state.mode_queries_left--;
        state.mode_query_at = query_now + 200;
        if (!send_request(device, 0x25)) {
          disconnect_receiver(&device, &state);
          next_open = monotonic_ms() + 1000;
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
  umask(0077);
  if (argc == 2 && strcmp(argv[1], "--selftest") == 0) return selftest();
  if (argc != 1) return 1;
  const char *fixture = getenv("CETRA_STATUS_FIXTURE");
  if (fixture && *fixture) {
    puts(fixture);
    fflush(stdout);
    while (true) sleep(3600);
  }
  signal(SIGPIPE, SIG_IGN);
  signal(SIGINT, stop_running);
  signal(SIGTERM, stop_running);
  if (make_nonblocking(STDOUT_FILENO) != 0) return 1;
  char socket_path[512], lock_path[512];
  if (!runtime_path(socket_path, sizeof(socket_path), "rog-cetra-control.sock")
      || !runtime_path(lock_path, sizeof(lock_path), "rog-cetra-control.owner.lock")) return 1;
  const char *runtime = getenv("XDG_RUNTIME_DIR");
  struct stat runtime_stat;
  if (!runtime || !safe_ancestors(runtime) || lstat(runtime, &runtime_stat) != 0
      || !S_ISDIR(runtime_stat.st_mode) || runtime_stat.st_uid != geteuid()
      || (runtime_stat.st_mode & 0777) != 0700) return 1;
  int lock_fd = open(lock_path, O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK, 0600);
  if (lock_fd < 0) return 1;
  struct stat lock_stat;
  if (fstat(lock_fd, &lock_stat) != 0 || !S_ISREG(lock_stat.st_mode)
      || lock_stat.st_uid != geteuid() || lock_stat.st_nlink != 1 || fchmod(lock_fd, 0600) != 0) {
    close(lock_fd); return 1;
  }
  if (flock(lock_fd, LOCK_EX | LOCK_NB) != 0) {
    close(lock_fd);
    return mirror(socket_path);
  }
  const char *logging = getenv("CETRA_DIAGNOSTICS");
  logging_enabled = !logging || strcmp(logging, "0") != 0;
  log_event("DAEMON: starting (PID %ld)", (long)getpid());
  unlink(socket_path);
  int server_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (server_fd < 0) { close(lock_fd); return 1; }
  struct sockaddr_un address = {0};
  address.sun_family = AF_UNIX;
  if (strlen(socket_path) >= sizeof(address.sun_path)) {
    close(server_fd); close(lock_fd); return 1;
  }
  strcpy(address.sun_path, socket_path);
  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) != 0
      || chmod(socket_path, 0600) != 0 || listen(server_fd, MAX_CLIENTS) != 0
      || make_nonblocking(server_fd) != 0) {
    close(server_fd); unlink(socket_path); close(lock_fd); return 1;
  }
  if (hid_init() != 0) {
    close(server_fd); unlink(socket_path); close(lock_fd); return 1;
  }
  int result = owner(server_fd);
  log_event("DAEMON: exiting (PID %ld, result %d)", (long)getpid(), result);
  hid_exit();
  close(server_fd);
  unlink(socket_path);
  close(lock_fd);
  return result;
}
