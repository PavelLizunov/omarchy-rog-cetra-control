#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void build_mode_report(unsigned char *report, int mode) {
  memset(report, 0, 64);
  report[0] = 0xcc;
  report[1] = 0x41;
  report[2] = 0x08;
  report[5] = (unsigned char)mode;
}

static int selftest(void) {
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
  if (argc == 2 && strcmp(argv[1], "--selftest") == 0) return selftest();

  const char *fixture = getenv("CETRA_STATUS_FIXTURE");
  if (fixture && *fixture && argc == 1) {
    puts(fixture);
    return 0;
  }

  fprintf(stderr, "cetra-status is a test helper; run cetra-watch for device access\n");
  return 2;
}
