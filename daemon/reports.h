// Private report decoding and nullable JSON contract; no inferred mute state.
static void invalidate_settings(struct device_state *state) {
  state->anc_level = 0;
  state->anc_adaptive = false;
  state->voice_prompt = 0;
  state->proximity = 0;
  state->anc_level_report = (struct observed_report){0};
  state->anc_adaptive_report = (struct observed_report){0};
  state->voice_prompt_report = (struct observed_report){0};
  state->proximity_report = (struct observed_report){0};
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
  state->mode_queries_left = 0;
  invalidate_settings(state);
  state->presence_report = (struct observed_report){0};
  state->charging_report = (struct observed_report){0};
  state->battery_report = (struct observed_report){0};
  state->mode_report = (struct observed_report){0};
  // An explicitly requested lighting preference belongs to this owner session.
  // Preserve it across transport resets, but never invent one on reconnect.
}

static void apply_packet(hid_device *device, struct device_state *state, const unsigned char *packet, int size) {
  if (size >= 6 && packet[0] == 0xcc && packet[1] == 0x12 && packet[2] == 0x01) {
    state->presence_report = (struct observed_report){
      .seen = true, .raw = packet[5], .received_ms = monotonic_ms(),
    };
    if (packet[5] == 0) invalidate_settings(state);
    log_event("PRESENCE_REPORT: raw=0x%02x", packet[5]);
  } else if (size >= 7 && packet[0] == 0xcc && packet[1] == 0x12 && packet[2] == 0x08) {
    state->charging_report = (struct observed_report){
      .seen = true, .raw = packet[5], .extra = packet[6], .received_ms = monotonic_ms(),
    };
    log_event("CHARGING_REPORT: raw=0x%02x case_raw=0x%02x", packet[5], packet[6]);
  } else if (size >= 9 && packet[0] == 0xcc && packet[1] == 0x12 && packet[2] == 0x07) {
    state->battery_report = (struct observed_report){.seen = true, .received_ms = monotonic_ms()};
    state->receiver = true;
    int prev_left = state->left;
    int prev_right = state->right;
    int prev_case = state->case_level;
    bool prev_connected = state->connected;
    if (packet[6] <= 100) {
      state->left = packet[6];
      state->left_missing = 0;
    } else {
      if (state->left_missing < 2) state->left_missing++;
      if (state->left_missing >= 2) state->left = -1;
    }
    if (packet[7] <= 100) {
      state->right = packet[7];
      state->right_missing = 0;
    } else {
      if (state->right_missing < 2) state->right_missing++;
      if (state->right_missing >= 2) state->right = -1;
    }
    state->case_level = packet[8] <= 100 ? packet[8] : -1;
    state->connected = state->left >= 0 || state->right >= 0;
    if (!state->connected) invalidate_settings(state);
    if (state->left != prev_left || state->right != prev_right || state->case_level != prev_case) {
      log_event("BATTERY: left=%d right=%d case=%d (raw: L=0x%02x R=0x%02x Case=0x%02x, mask=0x%02x)",
          state->left, state->right, state->case_level, packet[6], packet[7], packet[8], packet[5]);
    }
    if (prev_connected && !state->connected) {
      log_event("LIFECYCLE: earbud telemetry unavailable (both disconnected)");
    }
  } else if (size >= 6 && packet[0] == 0xcc && packet[1] == 0x12 && packet[2] == 0x25) {
    if (packet[5] <= 2) {
      state->mode_report = (struct observed_report){.seen = true, .received_ms = monotonic_ms()};
      int prev_mode = state->mode;
      state->mode = packet[5];
      if (state->mode == state->mode_desired) state->mode_queries_left = 0;
      if (state->mode != prev_mode) log_event("MODE_READBACK: mode=%s (%d)", mode_name(state->mode), state->mode);
    }
  } else if (size >= 6 && packet[0] == 0xcc && packet[1] == 0x12 && packet[2] == 0x2b) {
    state->anc_level_report = (struct observed_report){0};
    if (state->presence_report.seen && state->presence_report.raw == 0) return;
    if (packet[5] >= 1 && packet[5] <= 3) {
      state->anc_level_report = (struct observed_report){
        .seen = true, .raw = packet[5], .received_ms = monotonic_ms(),
      };
      int prev_lvl = state->anc_level;
      state->anc_level = packet[5];
      if (state->anc_level != prev_lvl) log_event("ANC_LEVEL_READBACK: level=%d", state->anc_level);
    }
  } else if (size >= 6 && packet[0] == 0xcc && packet[1] == 0x12 && packet[2] == 0x2c) {
    state->anc_adaptive_report = (struct observed_report){0};
    if (packet[5] > 1 || (state->presence_report.seen && state->presence_report.raw == 0)) return;
    state->anc_adaptive_report = (struct observed_report){
      .seen = true, .raw = packet[5], .received_ms = monotonic_ms(),
    };
    bool prev_adp = state->anc_adaptive;
    state->anc_adaptive = packet[5] == 1;
    if (state->anc_adaptive != prev_adp) log_event("ANC_ADAPTIVE_READBACK: enabled=%s", state->anc_adaptive ? "true" : "false");
  } else if (size >= 6 && packet[0] == 0xcc && packet[1] == 0x12 && packet[2] == 0x28) {
    state->voice_prompt_report = (struct observed_report){0};
    if (state->presence_report.seen && state->presence_report.raw == 0) return;
    if (packet[5] <= 2) {
      state->voice_prompt_report = (struct observed_report){
        .seen = true, .raw = packet[5], .received_ms = monotonic_ms(),
      };
      int prev_p = state->voice_prompt;
      state->voice_prompt = packet[5];
      if (state->voice_prompt != prev_p) log_event("VOICE_PROMPT_READBACK: prompt=%s (%d)", prompt_name(state->voice_prompt), state->voice_prompt);
    }
  } else if (size >= 6 && packet[0] == 0xcc && packet[1] == 0x12 && packet[2] == 0x26) {
    state->proximity_report = (struct observed_report){0};
    if (packet[5] > 1 || (state->presence_report.seen && state->presence_report.raw == 0)) return;
    state->proximity_report = (struct observed_report){
      .seen = true, .raw = packet[5], .received_ms = monotonic_ms(),
    };
    int prev_prox = state->proximity;
    state->proximity = packet[5];
    if (state->proximity != prev_prox) log_event("PROXIMITY_READBACK: in_ear=%d", state->proximity);
  } else if (size >= 7 && packet[0] == 0xcc && packet[1] == 0x70) {
    char hex[64];
    hex_dump(hex, sizeof(hex), packet, size > 16 ? 16 : size);
    int earbud = packet[5];
    int gesture = packet[6];
    int sub = size >= 8 ? packet[7] : -1;
    if (earbud == 0x00 && gesture == 0x02) {
      log_event("GESTURE: left earbud double-tap -> trigger immediate ANC readback");
      if (device) send_request(device, 0x25);
    } else if (earbud == 0x01 && gesture == 0x01) {
      if (state->call_context) {
        if (state->tap_seq < INT_MAX) state->tap_seq++;
        log_event("GESTURE: right earbud single-tap in call observed (seq=%d, microphone_state=unknown)", state->tap_seq);
      } else {
        log_event("GESTURE: right earbud single-tap observed outside requested call (media-key delivery unconfirmed, microphone_state=unknown)");
      }
    } else {
      log_event("GESTURE: earbud=%s (%d) gesture=%d sub=%d (raw: %s)",
          earbud == 0 ? "left" : (earbud == 1 ? "right" : "unknown"), earbud, gesture, sub, hex);
    }
  } else if (size >= 2 && packet[0] == 0x0c) {
    if (packet[1] != 0) log_event("CONSUMER_KEY: usage=0x%02x (0x08=PlayPause, 0x10=Next, 0x20=Prev)", packet[1]);
  } else if (size >= 2 && packet[0] == 0x05) {
    log_event("TELEPHONY_EVENT: state=0x%02x (0x01=OffHook, 0x00=OnHook)", packet[1]);
  } else if (size >= 9 && packet[0] == 0xcc && packet[1] == 0x12 && packet[2] == 0x09) {
    log_event("UNSOLICITED_BATTERY (0x09): L=0x%02x R=0x%02x Case=0x%02x", packet[6], packet[7], packet[8]);
  } else if (size >= 3 && packet[0] == 0xcc && packet[1] == 0x51 && packet[2] == 0x28) {
    // Lighting output report ACK
  } else if (size >= 3 && packet[0] == 0xcc && packet[1] == 0x50 && packet[2] == 0x55) {
    // Lighting commit report ACK
  } else {
    char hex[128];
    hex_dump(hex, sizeof(hex), packet, size > 32 ? 32 : size);
    log_event("PACKET_UNHANDLED: len=%d bytes=[%s]", size, hex);
  }
}

static bool report_fresh(const struct observed_report *report, long now) {
  return report->seen && now >= report->received_ms && now - report->received_ms < DEVICE_REPORT_FRESH_MS;
}

static void format_state(char *line, size_t size, const struct device_state *state) {
  char left[8], right[8], case_level[8];
  snprintf(left, sizeof(left), state->left >= 0 ? "%d" : "null", state->left);
  snprintf(right, sizeof(right), state->right >= 0 ? "%d" : "null", state->right);
  snprintf(case_level, sizeof(case_level), state->case_level >= 0 ? "%d" : "null", state->case_level);
  long now = monotonic_ms();
  char anc_level[8];
  snprintf(anc_level, sizeof(anc_level), report_fresh(&state->anc_level_report, now) ? "%d" : "null", state->anc_level);
  const struct observed_report *presence = &state->presence_report;
  const struct observed_report *charging = &state->charging_report;
  bool presence_fresh = presence->seen && now >= presence->received_ms && now - presence->received_ms < DEVICE_REPORT_FRESH_MS;
  bool charging_fresh = charging->seen && now >= charging->received_ms && now - charging->received_ms < DEVICE_REPORT_FRESH_MS;
  bool presence_valid = presence_fresh && (presence->raw & ~0x11) == 0;
  bool charging_valid = charging_fresh && ((charging->raw & ~0x11) == 0 || charging->raw == 0xff);
  bool case_valid = charging_fresh && charging->extra <= 1;
  char presence_raw[8], charging_raw[8], case_charging_raw[8];
  snprintf(presence_raw, sizeof(presence_raw), presence->seen ? "%u" : "null", (unsigned)presence->raw);
  snprintf(charging_raw, sizeof(charging_raw), charging->seen ? "%u" : "null", (unsigned)charging->raw);
  snprintf(case_charging_raw, sizeof(case_charging_raw), charging->seen ? "%u" : "null", (unsigned)charging->extra);
  snprintf(line, size,
      "{\"status\":\"ok\",\"receiver\":%s,\"connected\":%s,"
      "\"left\":%s,\"right\":%s,\"case\":%s,\"mode\":\"%s\","
      "\"anc_level\":%s,\"anc_adaptive\":%s,\"voice_prompt\":\"%s\","
      "\"proximity\":%s,\"lighting\":\"%s\","
      "\"call_context\":%s,\"tap_seq\":%d,\"microphone_state\":\"unknown\","
      "\"left_present\":%s,\"right_present\":%s,"
      "\"left_charging\":%s,\"right_charging\":%s,\"case_charging\":%s,"
      "\"presence_raw\":%s,\"charging_raw\":%s,\"case_charging_raw\":%s,"
      "\"battery_fresh\":%s,\"mode_fresh\":%s}\n",
      state->receiver ? "true" : "false", state->connected ? "true" : "false",
      left, right, case_level, mode_name(state->mode),
      anc_level, !report_fresh(&state->anc_adaptive_report, now) ? "null" : state->anc_adaptive ? "true" : "false",
      report_fresh(&state->voice_prompt_report, now) ? prompt_name(state->voice_prompt) : "unknown",
      !report_fresh(&state->proximity_report, now) ? "null" : state->proximity ? "true" : "false",
      lighting_name(state->lighting_desired_valid ? state->lighting : -1),
      state->call_context ? "true" : "false", state->tap_seq,
      !presence_valid ? "null" : (presence->raw & 0x01) ? "true" : "false",
      !presence_valid ? "null" : (presence->raw & 0x10) ? "true" : "false",
      !charging_valid ? "null" : charging->raw != 0xff && (charging->raw & 0x01) ? "true" : "false",
      !charging_valid ? "null" : charging->raw != 0xff && (charging->raw & 0x10) ? "true" : "false",
      !case_valid ? "null" : charging->extra == 1 ? "true" : "false",
      presence_raw, charging_raw, case_charging_raw,
      report_fresh(&state->battery_report, now) ? "true" : "false",
      report_fresh(&state->mode_report, now) ? "true" : "false");
}
