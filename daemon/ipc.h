// Private status fan-out and mirror-client transport; no second HID reader.
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
