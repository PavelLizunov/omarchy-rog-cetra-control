/* Linked instead of hidapi: no test can reach a real receiver. */
static hid_device test_device;
static int hid_inits, hid_outputs, hid_queries;
static unsigned char last_output[64], last_query[17];
int hid_init(void) { hid_inits++; return -1; }
int hid_exit(void) { abort(); }
struct hid_device_info *hid_enumerate(unsigned short vendor, unsigned short product) {
  (void)vendor; (void)product; abort();
}
void hid_free_enumeration(struct hid_device_info *devices) { (void)devices; abort(); }
hid_device *hid_open_path(const char *path) { (void)path; abort(); }
void hid_close(hid_device *device) { (void)device; abort(); }
int hid_read_timeout(hid_device *device, unsigned char *data, size_t length, int timeout) {
  (void)device; (void)data; (void)length; (void)timeout; abort();
}
int hid_write(hid_device *device, const unsigned char *data, size_t length) {
  assert(device == &test_device && length == sizeof(last_query));
  memcpy(last_query, data, length);
  hid_queries++;
  return (int)length;
}
int hid_send_output_report(hid_device *device, const unsigned char *data, size_t length) {
  assert(device == &test_device && length == sizeof(last_output));
  memcpy(last_output, data, length);
  hid_outputs++;
  return (int)length;
}
