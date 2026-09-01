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
};

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
  }
  puts("}");
}

static bool parse_response(const unsigned char *response, int size, struct status *value) {
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

static void cache_paths(char *lock_path, size_t lock_size, char *cache_path, size_t cache_size) {
  const char *runtime = getenv("XDG_RUNTIME_DIR");
  if (!runtime || !*runtime) runtime = "/tmp";
  snprintf(lock_path, lock_size, "%s/slovn-cetra.lock", runtime);
  snprintf(cache_path, cache_size, "%s/slovn-cetra.status", runtime);
}

static bool print_cache(const char *path) {
  struct stat statbuf;
  if (stat(path, &statbuf) != 0 || time(NULL) - statbuf.st_mtime > 10) return false;
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

static int selftest(void) {
  const unsigned char sample[] = {0xcc, 0x12, 0x07, 0, 0, 5, 91, 98, 100};
  struct status value = {0};
  if (!parse_response(sample, sizeof(sample), &value)) return 1;
  if (value.state != 5 || value.left != 91 || value.right != 98 || value.case_level != 100) return 1;
  puts("ok");
  return 0;
}

int main(int argc, char **argv) {
  const unsigned char command[16] = {
      0xcc, 0x12, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  unsigned char report[17] = {0};
  unsigned char response[64] = {0};

  if (argc > 1 && strcmp(argv[1], "--selftest") == 0) return selftest();

  const char *fixture = getenv("CETRA_STATUS_FIXTURE");
  if (fixture && *fixture) {
    puts(fixture);
    return 0;
  }

  char lock_path[512];
  char cache_path[512];
  cache_paths(lock_path, sizeof(lock_path), cache_path, sizeof(cache_path));
  if (print_cache(cache_path)) return 0;
  int lock_fd = open(lock_path, O_CREAT | O_RDWR, 0600);
  if (lock_fd < 0 || flock(lock_fd, LOCK_EX | LOCK_NB) != 0) {
    if (!print_cache(cache_path)) print_status("busy", true, NULL);
    if (lock_fd >= 0) close(lock_fd);
    return 0;
  }

  if (hid_init() != 0) {
    print_status("hid-init-failed", false, NULL);
    close(lock_fd);
    return 0;
  }

  hid_device *device = open_receiver();
  if (!device) {
    hid_exit();
    print_status(errno == EACCES ? "permission-denied" : "receiver-missing", false, NULL);
    close(lock_fd);
    return 0;
  }

  memcpy(report + 1, command, sizeof(command));
  int size = -1;
  for (int attempt = 0; attempt < 3; attempt++) {
    memset(response, 0, sizeof(response));
    if (hid_write(device, report, sizeof(report)) < 0) break;
    size = hid_read_timeout(device, response, sizeof(response), 700);
    if (size >= 9 && response[0] == 0xcc && response[1] == 0x12 && response[2] == 0x07) break;
    usleep(100000);
  }
  hid_close(device);
  hid_exit();

  struct status value = {0};
  if (!parse_response(response, size, &value)) {
    print_status(size == 0 ? "timeout" : "protocol-error", true, NULL);
    close(lock_fd);
    return 0;
  }

  char left[5];
  char right[5];
  char case_level[5];
  format_level(left, sizeof(left), value.left);
  format_level(right, sizeof(right), value.right);
  format_level(case_level, sizeof(case_level), value.case_level);

  char line[512];
  int length = snprintf(line, sizeof(line),
      "{\"status\":\"ok\",\"receiver\":true,\"connected\":%s,\"state\":%u,"
      "\"left\":%s,\"right\":%s,\"case\":%s}\n",
      (value.left <= 100 || value.right <= 100) ? "true" : "false", value.state,
      left, right, case_level);
  if (length > 0) {
    fputs(line, stdout);
    write_cache(cache_path, line);
  }
  close(lock_fd);
  return 0;
}
