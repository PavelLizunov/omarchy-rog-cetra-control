#include <hidapi/hidapi.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define VENDOR_ID 0x0b05
#define PRODUCT_ID 0x1ad3
#define HID_INTERFACE 3

struct status {
  unsigned char state;
  unsigned char left;
  unsigned char right;
  unsigned char case_level;
  int mode;
};

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

static void print_level(unsigned char value) {
  if (value <= 100) printf("%u", value);
  else printf("null");
}

static void print_status(const char *status_name, bool receiver, const struct status *value) {
  bool connected = value && (value->left <= 100 || value->right <= 100);
  printf("{\"status\":\"%s\",\"receiver\":%s,\"connected\":%s",
         status_name, receiver ? "true" : "false", connected ? "true" : "false");
  if (value) {
    printf(",\"state\":%u,\"left\":", value->state);
    print_level(value->left);
    printf(",\"right\":");
    print_level(value->right);
    printf(",\"case\":");
    print_level(value->case_level);
    printf(",\"mode\":\"%s\"", mode_name(value->mode));
  }
  puts("}");
}

static bool parse_battery(const unsigned char *response, int size, struct status *value) {
  if (size < 9 || response[0] != 0xcc || response[1] != 0x12 || response[2] != 0x07) return false;
  value->state = response[5];
  value->left = response[6];
  value->right = response[7];
  value->case_level = response[8];
  return true;
}

static hid_device *open_receiver(void) {
  struct hid_device_info *devices = hid_enumerate(VENDOR_ID, PRODUCT_ID);
  struct hid_device_info *current = devices;
  hid_device *device = NULL;
  while (current) {
    if (current->interface_number == HID_INTERFACE) {
      device = hid_open_path(current->path);
      break;
    }
    current = current->next;
  }
  hid_free_enumeration(devices);
  return device;
}

static bool request(hid_device *device, unsigned char command_id, unsigned char *response) {
  unsigned char command[16] = {0xcc, 0x12, command_id};
  unsigned char report[17] = {0};
  memcpy(report + 1, command, sizeof(command));

  for (int attempt = 0; attempt < 3; attempt++) {
    memset(response, 0, 64);
    if (hid_write(device, report, sizeof(report)) < 0) return false;
    for (int read_attempt = 0; read_attempt < 4; read_attempt++) {
      int size = hid_read_timeout(device, response, 64, 250);
      if (size >= 6 && response[0] == 0xcc && response[1] == 0x12 && response[2] == command_id)
        return true;
    }
    usleep(100000);
  }
  return false;
}

static bool read_status(hid_device *device, struct status *value) {
  unsigned char response[64] = {0};
  value->mode = -1;
  if (!request(device, 0x07, response) || !parse_battery(response, 64, value)) return false;
  if (request(device, 0x25, response) && response[5] <= 2) value->mode = response[5];
  return true;
}

static void build_mode_report(unsigned char *report, int mode) {
  memset(report, 0, 64);
  report[0] = 0xcc;
  report[1] = 0x41;
  report[2] = 0x08;
  report[5] = (unsigned char)mode;
}

static bool set_mode(hid_device *device, int mode) {
  unsigned char report[64];
  build_mode_report(report, mode);
  return hid_send_output_report(device, report, sizeof(report)) == (int)sizeof(report);
}

static void cache_paths(char *lock_path, size_t lock_size, char *cache_path, size_t cache_size) {
  const char *runtime = getenv("XDG_RUNTIME_DIR");
  if (!runtime || !*runtime) runtime = "/tmp";
  snprintf(lock_path, lock_size, "%s/rog-cetra-control.lock", runtime);
  snprintf(cache_path, cache_size, "%s/rog-cetra-control.status", runtime);
}

static bool print_cache(const char *path) {
  struct stat statbuf;
  if (stat(path, &statbuf) != 0 || time(NULL) - statbuf.st_mtime > 3) return false;
  FILE *file = fopen(path, "r");
  if (!file) return false;
  char line[512];
  bool ok = fgets(line, sizeof(line), file) != NULL;
  fclose(file);
  if (ok) fputs(line, stdout);
  return ok;
}

static void write_cache(const char *path, const char *line) {
  char temp[1024];
  snprintf(temp, sizeof(temp), "%s.%ld", path, (long)getpid());
  FILE *file = fopen(temp, "w");
  if (!file) return;
  fputs(line, file);
  fclose(file);
  chmod(temp, 0600);
  rename(temp, path);
}

static void format_level(char *target, size_t size, unsigned char value) {
  if (value <= 100) snprintf(target, size, "%u", value);
  else snprintf(target, size, "null");
}

static void format_status(char *line, size_t size, const struct status *value) {
  char left[5];
  char right[5];
  char case_level[5];
  format_level(left, sizeof(left), value->left);
  format_level(right, sizeof(right), value->right);
  format_level(case_level, sizeof(case_level), value->case_level);
  snprintf(line, size,
      "{\"status\":\"ok\",\"receiver\":true,\"connected\":%s,\"state\":%u,"
      "\"left\":%s,\"right\":%s,\"case\":%s,\"mode\":\"%s\"}\n",
      (value->left <= 100 || value->right <= 100) ? "true" : "false", value->state,
      left, right, case_level, mode_name(value->mode));
}

static int selftest(void) {
  const unsigned char sample[] = {0xcc, 0x12, 0x07, 0, 0, 5, 91, 98, 100};
  struct status value = {.mode = 1};
  if (!parse_battery(sample, sizeof(sample), &value)) return 1;
  if (value.state != 5 || value.left != 91 || value.right != 98 || value.case_level != 100) return 1;
  if (parse_mode("off") != 0 || parse_mode("anc") != 1 || parse_mode("ambient") != 2) return 1;
  if (strcmp(mode_name(1), "anc") != 0) return 1;
  unsigned char report[64];
  build_mode_report(report, 2);
  if (report[0] != 0xcc || report[1] != 0x41 || report[2] != 0x08 || report[5] != 2) return 1;
  for (size_t i = 0; i < sizeof(report); i++)
    if (i != 0 && i != 1 && i != 2 && i != 5 && report[i] != 0) return 1;
  puts("ok");
  return 0;
}

int main(int argc, char **argv) {
  if (argc > 1 && strcmp(argv[1], "--selftest") == 0) return selftest();

  const char *fixture = getenv("CETRA_STATUS_FIXTURE");
  if (fixture && *fixture && argc == 1) {
    puts(fixture);
    return 0;
  }

  int target_mode = -1;
  if (argc == 3 && strcmp(argv[1], "--set-mode") == 0) {
    target_mode = parse_mode(argv[2]);
    if (target_mode < 0) {
      fprintf(stderr, "mode must be off, anc, or ambient\n");
      return 2;
    }
  } else if (argc != 1) {
    fprintf(stderr, "usage: cetra-status [--set-mode off|anc|ambient]\n");
    return 2;
  }

  char lock_path[512];
  char cache_path[512];
  cache_paths(lock_path, sizeof(lock_path), cache_path, sizeof(cache_path));
  if (target_mode < 0 && print_cache(cache_path)) return 0;

  int lock_fd = open(lock_path, O_CREAT | O_RDWR, 0600);
  if (lock_fd < 0 || flock(lock_fd, target_mode >= 0 ? LOCK_EX : LOCK_EX | LOCK_NB) != 0) {
    if (target_mode < 0 && !print_cache(cache_path)) print_status("busy", true, NULL);
    if (lock_fd >= 0) close(lock_fd);
    return target_mode >= 0 ? 1 : 0;
  }

  if (hid_init() != 0) {
    print_status("hid-init-failed", false, NULL);
    close(lock_fd);
    return target_mode >= 0 ? 1 : 0;
  }

  hid_device *device = open_receiver();
  if (!device) {
    hid_exit();
    print_status(errno == EACCES ? "permission-denied" : "receiver-missing", false, NULL);
    close(lock_fd);
    return target_mode >= 0 ? 1 : 0;
  }

  bool changed = true;
  if (target_mode >= 0) {
    changed = set_mode(device, target_mode);
    usleep(150000);
  }

  struct status value = {0};
  bool read_ok = read_status(device, &value);
  hid_close(device);
  hid_exit();

  if (!changed || !read_ok) {
    print_status(!changed ? "write-failed" : "timeout", true, NULL);
    close(lock_fd);
    return target_mode >= 0 ? 1 : 0;
  }

  char line[512];
  format_status(line, sizeof(line), &value);
  fputs(line, stdout);
  write_cache(cache_path, line);
  close(lock_fd);
  return target_mode >= 0 && value.mode != target_mode ? 1 : 0;
}
