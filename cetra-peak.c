// Audio-only peak client. No HID, files, recording, default-source or mute writes.
#define _GNU_SOURCE
#include <pulse/pulseaudio.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t alive = 1;
static pa_stream *stream;
static const char *source_name;
static bool failed;
static bool ready;
static float latest;
static long received;
static long started;

static long now_ms(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000L + t.tv_nsec / 1000000L;
}

static void stop(int sig) { (void)sig; alive = 0; }

static void stdin_ready(pa_mainloop_api *api, pa_io_event *event, int fd, pa_io_event_flags_t flags, void *unused) {
  (void)api; (void)event; (void)unused;
  if (flags & (PA_IO_EVENT_HANGUP | PA_IO_EVENT_ERROR)) { alive = 0; return; }
  char ignored[32];
  if (read(fd, ignored, sizeof(ignored)) <= 0) alive = 0;
}

static void publish(pa_mainloop_api *api, pa_time_event *event, const struct timeval *scheduled, void *unused) {
  (void)api; (void)scheduled;
  long now = now_ms();
  if (!ready && now - started >= 3000) { failed = true; return; }
  char line[32];
  int len = ready && !pa_stream_is_suspended(stream) && received && now - received < 1000
    ? snprintf(line, sizeof(line), "{\"level\":%.6f}\n", cbrtf(latest))
    : snprintf(line, sizeof(line), "{\"level\":null}\n");
  if (write(STDOUT_FILENO, line, (size_t)len) != len) { failed = true; return; }
  pa_context_rttime_restart(unused, event, pa_rtclock_now() + 50000);
}

static bool valid_source(const char *name) {
  const char *prefix = "alsa_input.usb-ASUSTek_ROG_CETRA_TRUE_WIRELESS_SPEEDNOVA_";
  return strlen(name) < 512 && strncmp(name, prefix, strlen(prefix)) == 0
      && strchr(name + strlen(prefix), '.') != NULL
      && strspn(name, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-") == strlen(name);
}

static bool valid_peak(float value) { return isfinite(value) && value >= 0.0f && value <= 1.0f; }

static void check_route(pa_stream *s, void *unused) {
  (void)unused;
  const char *actual = pa_stream_get_device_name(s);
  if (!actual || strcmp(actual, source_name) != 0) {
    fprintf(stderr, "Peak source mismatch: %s\n", actual ? actual : "unavailable");
    failed = true; ready = false;
  }
}

static void stream_state(pa_stream *s, void *unused) {
  (void)unused;
  switch (pa_stream_get_state(s)) {
    case PA_STREAM_READY: ready = true; check_route(s, NULL); break;
    case PA_STREAM_FAILED:
    case PA_STREAM_TERMINATED:
      fprintf(stderr, "Peak stream stopped: %s\n", pa_strerror(pa_context_errno(pa_stream_get_context(s))));
      failed = true; ready = false; break;
    default: break;
  }
}

static void suspended(pa_stream *s, void *unused) {
  (void)unused;
  if (pa_stream_is_suspended(s)) received = 0;
}

static void read_peak(pa_stream *s, size_t available, void *unused) {
  (void)available; (void)unused;
  const void *data;
  size_t size;
  if (pa_stream_peek(s, &data, &size) < 0) { failed = true; return; }
  if (!size) return;
  if (data && size % sizeof(float) == 0) {
    float value;
    memcpy(&value, (const char *)data + size - sizeof(float), sizeof(value));
    if (valid_peak(value)) { latest = value; received = now_ms(); }
    else received = 0;
  } else received = 0;
  if (pa_stream_drop(s) < 0) failed = true;
}

static void context_state(pa_context *context, void *unused) {
  (void)unused;
  switch (pa_context_get_state(context)) {
    case PA_CONTEXT_READY: {
      pa_sample_spec spec = { .format = PA_SAMPLE_FLOAT32NE, .rate = 20, .channels = 1 };
      pa_proplist *props = pa_proplist_new();
      if (!props) { failed = true; break; }
      pa_proplist_sets(props, PA_PROP_APPLICATION_ID, "io.github.pavellizunov.rog-cetra-control.peak");
      pa_proplist_sets(props, PA_PROP_APPLICATION_NAME, "Cetra Peak Detect");
      pa_proplist_sets(props, PA_PROP_MEDIA_NAME, "Peak detect");
      pa_proplist_sets(props, PA_PROP_MEDIA_ROLE, "production");
      pa_proplist_sets(props, "media.category", "Capture");
      // WirePlumber ignores metadata target overrides only with node.dont-move.
      // PA_STREAM_DONT_MOVE alone maps to dont-reconnect in pipewire-pulse.
      pa_proplist_sets(props, "node.dont-move", "true");
      pa_proplist_sets(props, "node.dont-fallback", "true");
      // Follow an already-active source without keeping it awake. In PipeWire
      // 1.6.8 the Pulse auto-suspend flag sets passive=true, which never starts
      // this meter; in-follow is the upstream corrected capture behavior.
      pa_proplist_sets(props, "node.passive", "in-follow");
      stream = pa_stream_new_with_proplist(context, "Cetra input level", &spec, NULL, props);
      pa_proplist_free(props);
      if (!stream) { failed = true; break; }
      pa_stream_set_state_callback(stream, stream_state, NULL);
      pa_stream_set_moved_callback(stream, check_route, NULL);
      pa_stream_set_suspended_callback(stream, suspended, NULL);
      pa_stream_set_read_callback(stream, read_peak, NULL);
      pa_buffer_attr attr = { .maxlength = 4096, .tlength = (uint32_t)-1,
        .prebuf = (uint32_t)-1, .minreq = (uint32_t)-1, .fragsize = sizeof(float) };
      pa_stream_flags_t flags = PA_STREAM_PEAK_DETECT | PA_STREAM_DONT_MOVE;
      if (pa_stream_connect_record(stream, source_name, &attr, flags) < 0) {
        fprintf(stderr, "Peak connect failed: %s\n", pa_strerror(pa_context_errno(context)));
        failed = true;
      }
      break;
    }
    case PA_CONTEXT_FAILED:
    case PA_CONTEXT_TERMINATED: failed = true; break;
    default: break;
  }
}

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "--selftest") == 0) {
    if (!valid_source("alsa_input.usb-ASUSTek_ROG_CETRA_TRUE_WIRELESS_SPEEDNOVA_0000-00.mono-fallback")
        || valid_source("@DEFAULT_SOURCE@") || valid_source("alsa_input.laptop")
        || valid_peak(NAN) || valid_peak(INFINITY) || valid_peak(-1) || valid_peak(2)
        || !valid_peak(0) || !valid_peak(1)) return 1;
    puts("ok"); return 0;
  }
  if (argc != 2 || !valid_source(argv[1])) return 2;
  source_name = argv[1];
  pid_t parent = getppid();
  if (prctl(PR_SET_PDEATHSIG, SIGTERM) != 0 || getppid() != parent) return 1;
  signal(SIGTERM, stop); signal(SIGINT, stop); signal(SIGPIPE, SIG_IGN);
  int flags = fcntl(STDOUT_FILENO, F_GETFL);
  if (flags < 0 || fcntl(STDOUT_FILENO, F_SETFL, flags | O_NONBLOCK) < 0) return 1;
  pa_mainloop *loop = pa_mainloop_new();
  if (!loop) return 1;
  pa_context *context = pa_context_new(pa_mainloop_get_api(loop), "Cetra Peak Detect");
  if (!context) { pa_mainloop_free(loop); return 1; }
  pa_context_set_state_callback(context, context_state, NULL);
  if (pa_context_connect(context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL) < 0) failed = true;
  pa_mainloop_api *api = pa_mainloop_get_api(loop);
  pa_io_event *input = api->io_new(api, STDIN_FILENO, PA_IO_EVENT_INPUT | PA_IO_EVENT_HANGUP | PA_IO_EVENT_ERROR, stdin_ready, NULL);
  started = now_ms();
  pa_time_event *timer = pa_context_rttime_new(context, pa_rtclock_now(), publish, context);
  if (!input || !timer) failed = true;
  while (alive && !failed) {
    if (pa_mainloop_iterate(loop, 1, NULL) < 0 && errno != EINTR) { failed = true; break; }
  }
  if (input) api->io_free(input);
  if (timer) api->time_free(timer);
  if (stream) {
    pa_stream_set_state_callback(stream, NULL, NULL);
    pa_stream_disconnect(stream); pa_stream_unref(stream);
  }
  pa_context_set_state_callback(context, NULL, NULL);
  pa_context_disconnect(context); pa_context_unref(context); pa_mainloop_free(loop);
  return failed ? 1 : 0;
}
