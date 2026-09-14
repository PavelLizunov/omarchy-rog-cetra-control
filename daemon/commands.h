// Private command parser/framing. Only the owner supplies a live device handle.
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
  log_event("CALL_CONTEXT: requested=%s force=%s", requested ? "active" : "inactive", force ? "true" : "false");
  return set_call_context(device, requested);
}

static bool handle_command(hid_device *device, struct device_state *state, struct command_source *source, const char *command) {
  char key[32] = {0};
  char value[32] = {0};
  int arguments_at = 0;
  if (strchr(command, '\r') || sscanf(command, "%31s %31s%n", key, value, &arguments_at) != 2) return true;
  if (strcmp(key, "lighting") != 0
      && command[(size_t)arguments_at + strspn(command + arguments_at, " \t\n\v\f")]) return true;
  log_event("CMD: %s", command);
  if (strcmp(key, "mode") == 0) {
    int mode = parse_mode(value);
    if (mode >= 0 && device) {
      state->mode_queries_left = 0;
      if (!set_mode(device, mode) || !send_request(device, 0x25)) return false;
      state->mode = -1;
      if (state->connected) {
        long now = monotonic_ms();
        state->mode_desired = mode;
        state->mode_queries_left = 3;
        state->mode_query_at = now + 200;
        state->mode_query_deadline = now + 1000;
      }
    }
  } else if (strcmp(key, "call") == 0) {
    if (strcmp(value, "on") == 0) source->call_requested = true;
    else if (strcmp(value, "off") == 0) source->call_requested = false;
  } else if (strcmp(key, "anc_level") == 0) {
    if (strspn(value, "0123456789") != strlen(value)) return true;
    char *end;
    errno = 0;
    unsigned long level = strtoul(value, &end, 10);
    if (errno == ERANGE || *end || level < 1 || level > 3) return true;
    if (device) {
      if (!set_anc_level(device, (int)level)) return false;
      state->anc_level_report = (struct observed_report){0};
      if (!send_request(device, 0x2b)) return false;
    }
  } else if (strcmp(key, "anc_adaptive") == 0) {
    bool enabled = strcmp(value, "on") == 0 || strcmp(value, "true") == 0;
    if (!enabled && strcmp(value, "off") != 0 && strcmp(value, "false") != 0) return true;
    if (device) {
      if (!set_anc_adaptive(device, enabled)) return false;
      state->anc_adaptive_report = (struct observed_report){0};
      if (!send_request(device, 0x2c)) return false;
    }
  } else if (strcmp(key, "voice_prompt") == 0) {
    int val = parse_prompt(value);
    if (val >= 0 && device) {
      if (!set_voice_prompt(device, val)) return false;
      state->voice_prompt_report = (struct observed_report){0};
      if (!send_request(device, 0x28)) return false;
    }
  } else if (strcmp(key, "proximity") == 0) {
    bool enabled = strcmp(value, "on") == 0 || strcmp(value, "true") == 0;
    if (!enabled && strcmp(value, "off") != 0 && strcmp(value, "false") != 0) return true;
    if (device) {
      if (!set_proximity(device, enabled)) return false;
      state->proximity_report = (struct observed_report){0};
      if (!send_request(device, 0x26)) return false;
    }
  } else if (strcmp(key, "lighting") == 0) {
    int effect = parse_lighting(value);
    int rgb[3] = {255, 0, 0};
    const char *cursor = command + arguments_at;
    cursor += strspn(cursor, " \t\r\n\v\f");
    if (*cursor) {
      for (int i = 0; i < 3; i++) {
        if (*cursor < '0' || *cursor > '9') return true;
        char *end;
        errno = 0;
        long component = strtol(cursor, &end, 10);
        if (errno == ERANGE || component < 0 || component > 255) return true;
        rgb[i] = (int)component;
        size_t whitespace = strspn(end, " \t\r\n\v\f");
        if (*end && !whitespace) return true;
        cursor = end + whitespace;
      }
      if (*cursor) return true;
    }
    if (effect >= 0 && device) {
      if (!set_lighting(device, effect, (unsigned char)rgb[0], (unsigned char)rgb[1], (unsigned char)rgb[2])) return false;
      state->lighting = effect;
      state->lighting_r = rgb[0];
      state->lighting_g = rgb[1];
      state->lighting_b = rgb[2];
      state->lighting_desired_valid = true;
    }
  }
  return true;
}

static bool consume_commands(hid_device *device, struct device_state *state, struct command_source *source, const char *data, size_t size) {
  bool receiver_ok = true;
  for (size_t i = 0; i < size; i++) {
    char byte = data[i];
    if (byte == '\n') {
      if (!source->overflow) {
        if (source->length && source->buffer[source->length - 1] == '\r') source->length--;
        source->buffer[source->length] = '\0';
        if (!handle_command(receiver_ok ? device : NULL, state, source, source->buffer)) receiver_ok = false;
      }
      source->length = 0;
      source->overflow = false;
    } else if (byte == '\0') {
      source->overflow = true;
    } else if (!source->overflow) {
      if (source->length + 1 < sizeof(source->buffer)) source->buffer[source->length++] = byte;
      else source->overflow = true;
    }
  }
  return receiver_ok;
}
