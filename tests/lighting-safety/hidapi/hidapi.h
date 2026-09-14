#ifndef CETRA_LIGHTING_TEST_HIDAPI_H
#define CETRA_LIGHTING_TEST_HIDAPI_H
#include <stddef.h>
typedef struct hid_device_ { int generation; } hid_device;
struct hid_device_info {
  char *path;
  int interface_number;
  struct hid_device_info *next;
};
int hid_init(void);
int hid_exit(void);
struct hid_device_info *hid_enumerate(unsigned short vendor, unsigned short product);
void hid_free_enumeration(struct hid_device_info *devices);
hid_device *hid_open_path(const char *path);
int hid_write(hid_device *device, const unsigned char *data, size_t length);
int hid_send_output_report(hid_device *device, const unsigned char *data, size_t length);
int hid_read_timeout(hid_device *device, unsigned char *data, size_t length, int timeout);
void hid_close(hid_device *device);
#endif
