// Private HID transport/builders. Included by cetra-watch.c, never standalone.
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
