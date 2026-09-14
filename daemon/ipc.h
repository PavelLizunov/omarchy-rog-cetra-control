// Private status fan-out and mirror-client transport; no second HID reader.
// At most one partially written frame and one coalesced latest state.
static char stdout_frame[STATUS_BUFFER_SIZE], stdout_latest[STATUS_BUFFER_SIZE];
static size_t stdout_offset;

static void flush_status_output(void) {
  for (int attempts = 0; stdout_frame[0] && attempts < 4; attempts++) {
    size_t length = strlen(stdout_frame);
    ssize_t n = write(STDOUT_FILENO, stdout_frame + stdout_offset, length - stdout_offset);
    if (n < 0 && errno == EINTR) continue;
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
    if (n <= 0) { keep_running = 0; return; }
    stdout_offset += (size_t)n;
    if (stdout_offset == length) {
      memcpy(stdout_frame, stdout_latest, sizeof(stdout_frame));
      stdout_latest[0] = '\0';
      stdout_offset = 0;
    }
  }
}

static bool send_line(int fd, const char *line) {
  size_t length = strlen(line), offset = 0;
  int interrupted = 0;
  while (offset < length) {
    ssize_t written = send(fd, line + offset, length - offset, MSG_DONTWAIT | MSG_NOSIGNAL);
    if (written < 0 && errno == EINTR && interrupted++ < 3) continue;
    if (written <= 0) return false;
    offset += (size_t)written;
  }
  return true;
}

static void emit_state(const struct device_state *state, struct command_source *clients, char *last, size_t last_size) {
  char line[STATUS_BUFFER_SIZE];
  flush_status_output();
  format_state(line, sizeof(line), state);
  if (strcmp(line, last) == 0) return;
  snprintf(last, last_size, "%s", line);
  write_state_cache(line);
  snprintf(stdout_frame[0] ? stdout_latest : stdout_frame, STATUS_BUFFER_SIZE, "%s", line);
  flush_status_output();
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].fd < 0) continue;
    if (!send_line(clients[i].fd, line)) {
      close(clients[i].fd);
      clients[i] = (struct command_source){.fd = -1};
    }
  }
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
  struct timeval deadline = {.tv_sec = 1};
  if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &deadline, sizeof(deadline)) != 0
      || connect(fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
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
  int flags = fcntl(socket_fd, F_GETFL);
  if (flags < 0 || fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) < 0) { close(socket_fd); return 1; }
  char to_stdout[4096], to_socket[4096];
  size_t stdout_size = 0, socket_size = 0;
  bool input_open = true, socket_open = true;
  long drain_deadline = 0;
  while (keep_running) {
    if ((!input_open || !socket_open) && !stdout_size && !socket_size) break;
    if ((!input_open || !socket_open) && !drain_deadline) drain_deadline = monotonic_ms() + 2000;
    if (drain_deadline && monotonic_ms() >= drain_deadline) break;
    struct pollfd fds[3] = {
      {.fd = socket_open ? socket_fd : -1, .events = (stdout_size < sizeof(to_stdout) ? POLLIN : 0) | (socket_size ? POLLOUT : 0)},
      {.fd = input_open ? STDIN_FILENO : -1, .events = socket_size < sizeof(to_socket) ? POLLIN : 0},
      {.fd = STDOUT_FILENO, .events = stdout_size ? POLLOUT : 0},
    };
    long remaining = drain_deadline ? drain_deadline - monotonic_ms() : -1;
    int ready = poll(fds, 3, drain_deadline ? (int)(remaining > 0 ? remaining : 0) : -1);
    if (ready < 0) {
      if (errno == EINTR) continue;
      break;
    }
    if (!drain_deadline && ((fds[0].revents | fds[1].revents) & POLLHUP))
      drain_deadline = monotonic_ms() + 2000;
    // HUP is reported even without requested events. Do not busy-spin on a
    // closed peer while the output pipe is full; only wait for drain readiness.
    if ((fds[0].revents & POLLHUP) && stdout_size == sizeof(to_stdout)) {
      struct pollfd drain = {.fd = STDOUT_FILENO, .events = POLLOUT};
      long wait = drain_deadline - monotonic_ms();
      if (wait <= 0 || poll(&drain, 1, (int)wait) <= 0) break;
      fds[2].revents = drain.revents;
    }
    if ((fds[0].revents & (POLLIN | POLLHUP)) && stdout_size < sizeof(to_stdout)) {
      ssize_t size = read(socket_fd, to_stdout + stdout_size, sizeof(to_stdout) - stdout_size);
      if (size == 0) { socket_open = false; socket_size = 0; input_open = false; }
      else if (size > 0) stdout_size += (size_t)size;
      else if (errno != EINTR && errno != EAGAIN) break;
    }
    if ((fds[1].revents & (POLLIN | POLLHUP)) && socket_size < sizeof(to_socket)) {
      ssize_t size = read(STDIN_FILENO, to_socket + socket_size, sizeof(to_socket) - socket_size);
      if (size == 0) input_open = false;
      else if (size > 0) socket_size += (size_t)size;
      else if (errno != EINTR && errno != EAGAIN) break;
    }
    if ((fds[0].revents & POLLOUT) && socket_size) {
      ssize_t n = send(socket_fd, to_socket, socket_size, MSG_NOSIGNAL | MSG_DONTWAIT);
      if (n > 0) { socket_size -= (size_t)n; memmove(to_socket, to_socket + n, socket_size); }
      else if (n == 0 || (errno != EINTR && errno != EAGAIN)) break;
    }
    if ((fds[2].revents & POLLOUT) && stdout_size) {
      ssize_t n = write(STDOUT_FILENO, to_stdout, stdout_size);
      if (n > 0) { stdout_size -= (size_t)n; memmove(to_stdout, to_stdout + n, stdout_size); }
      else if (n == 0 || (errno != EINTR && errno != EAGAIN)) break;
    }
    if (fds[0].revents & (POLLERR | POLLNVAL)) break;
    if (fds[1].revents & (POLLERR | POLLNVAL)) break;
    if (fds[2].revents & (POLLERR | POLLHUP | POLLNVAL)) break;
  }
  close(socket_fd);
  return keep_running ? 1 : 0;
}

static void disconnect_receiver(hid_device **device, struct device_state *state) {
  if (*device) {
    log_event("RECEIVER: disconnected / closed handle");
    hid_close(*device);
  }
  *device = NULL;
  reset_receiver_state(state);
}

static int make_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL);
  if (flags < 0) return -1;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
