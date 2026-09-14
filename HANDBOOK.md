# Engineering handbook

Plugin ID: `io.github.pavellizunov.rog-cetra-control`.
Hardware: ASUS ROG Cetra True Wireless SpeedNova USB receiver `0b05:1ad3`,
interface 3. `/dev/hidrawN` numbering is dynamic; never hardcode the observed path.

Read AGENTS.md first, BACKLOG.md before behavior changes, and MODULES.md to find
the owning source. RESEARCH.md contains the dated protocol evidence. This
handbook describes the current architecture, not a release-readiness verdict.

## Ownership

```text
Omarchy shell
  Cetra.qml (one view per bar instance)
    CetraViewModel → battery/noise/microphone/language/lighting/voice sections
    I18n → local catalogs
  CetraService.qml (one manifest service)
    CetraPreferences → scoped shell API + FileView watch + bounded settings reader
    AudioTopology → tracked PipeWire nodes/links → tri-state endpoint observations
    CallDetector → event-driven communication observation + bounded settlement
    MicrophoneMeter → bin/cetra-peak → libpulse peak stream (opt-in, audio-only)
    Process → bin/cetra-watch
      owner lock → HID interface 3
      stdin + UNIX clients → validated commands
      reports → JSON stdout + clients + atomic status cache
```

Views do not create processes. They share service telemetry and pending requests.
The service is shell-owned, not a system service. Disable/reload can destroy it.
Additional CLI clients connect to the UNIX socket, never read hidraw themselves.

## Native module boundary

`cetra-watch.c` owns election and the ordered polling loop. Its private `daemon/`
implementation headers contain types, protocol builders, report decoding,
commands, IPC, logging/cache and selftest. They compile in one translation unit
with static linkage. This keeps the existing mock interception and binary
behavior; it does not introduce a public C API or independently scheduled tasks.
See MODULES.md for the include graph and checks for each module.

## State contract

| Family | Meaning |
| --- | --- |
| Battery | Last reported percentages or null; two missing reports clear a side. battery_fresh expires at 30 seconds and the UI hides stale values |
| Presence / charging | Nullable booleans derived only from verified domains; expire after 30 seconds; raw fields remain historical |
| ANC level / Adaptive / voice / proximity | Unknown until valid readback, invalidated on loss/absence/invalid reports, 30-second freshness |
| Mode | Last readback enum; mode_fresh expires at 30 seconds and the UI shows Unknown. Pending request is separate |
| Lighting | Last fully transmitted command, never hardware color readback; unknown on a fresh owner |
| Call context | Aggregated requested context; Telephony input is logged, not absolute tap-assignment confirmation |
| Microphone | Always `unknown`; observed tap count is not mute parity |

After presence has been observed, the UI requires fresh presence instead of
letting stale percentages enable controls. C `connected` stays battery-derived.
Neither presence nor missing battery proves placement in the case.

Settings requests expire after 48 scheduler ticks at 250 ms, nominally 12 seconds.
This allows the periodic ten-second readback cycle. A late matching response
clears its failure. Commands are not replayed to conceal missing confirmation.
Manual ANC selection disables Adaptive if it is On or Unknown; pending state is
shared across views. The three-second mode timeout is a separate control path.

## Scheduling

- Receiver open: verified battery `07`, mode `25`, presence `01` queries.
- Battery/mode alternate every 500 ms; presence is queried every 10 seconds.
- While available, settings `2b/2c/28/26` rotate every 2500 ms, nominally ten
  seconds per field. Presence absence vetoes replies/polling until new presence.
- Charging `08` is passive. Sending a query never refreshes an observation.
- The owner drains at most 16 ready reports before returning to commands/clients.
- Audio topology is observed while host and receiver are ready. Call loss uses
  up to two extra two-second one-shot confirmations; no periodic CLI is launched.
- Theme changes use one service-owned 350 ms one-shot debounce, not per-view timers.

Intervals are plugin policy, not ASUS timing guarantees. A stalled event loop
delays work and tick-based expiry.

## Preferences and lighting

Omarchy's scoped update API replaces an inline entry. CetraPreferences merges
current preferences and reads back its own saved `shell.json` entry through
`cetra-status --read-settings` (1 MiB, three-second deadline, one child). FileView
only watches changes with preload false; no text/data/reload read is issued.
Accepted writes override older disk completions until matching
readback or a three-second reload. Both host snapshots and widget injections
were observed to lag; views must not overwrite the shared state with them.

RGB selection and theme selection alone send no lighting writes. Explicit Apply
or an effect action sends the selection. Auto-theme is a separate opt-in, default
Off, and needs a current-session explicit colored apply. Off/Cycle are preserved.
Receiver/helper reset disarms QML automatic updates. The native owner can replay
its last successful explicit lighting preference within its own session.

The actual Off packet uses effect 1 with RGB zero for both zones and two commits.
Historical effect-0 Off and duplicate-commit necessity are still research limits.
Do not change the sequence to match an old table without protocol evidence.

## Microphone and calls

The physical earbud voice prompt is the user's indication of native mute. The
plugin does not decode the prompt, infer mute from PCM, inject software mute or infer a
state from tap parity. Effective mute was lost across left-side transitions in
user trials without a new tap; see RESEARCH.md for the exact limitations.

The opt-in MicrophoneMeter uses the libpulse helper cetra-peak for visual input level,
not absolute state. One service Loader owns it when enabled/connected; it selects
only the Cetra ALSA input and fails closed on multiple matching inputs. An active
external endpoint path admits capture. Monitor streams are excluded so they cannot
sustain that gate or automatic call detection. Nullable level updates are capped
at 20 Hz during use; PCM and peaks are not persisted. Other input and processing
paths are not silently substituted.
The helper checks the actual Pulse device name on ready/move. It sets
PA_STREAM_PEAK_DETECT, PA_STREAM_DONT_MOVE, node.dont-move, node.dont-fallback
and node.passive=in-follow. See RESEARCH.md for the tested PipeWire distinction
between dont-move/dont-reconnect and passive=true/in-follow. A mismatch exits;
QML retries no faster than every two seconds. Other application routes are never
changed. Processing nodes and keepalives alone do not authorize capture. The
observer follows actual active links back from endpoints, rejects mixed physical
inputs and bounds its graph to 512 nodes / 2048 links. Unknown topology fails closed.

Helper argv is one validated Cetra ALSA source name. Stdout contains bounded
JSON lines {"level":number-or-null}, at most 20 Hz; valid peaks are cube-root
scaled. Data older than one second becomes null (QML watchdog: 1500 ms).
Startup is bounded to three seconds. EOF on the service-owned stdin, SIGTERM,
parent death or stdout backpressure closes capture. No helper restart overlaps
an existing process; service unload explicitly signals the owned child.

Manual Request call mode was removed because it did not provide verified benefit
outside a call. The legacy preference is ignored. Automatic capture-driven
requests remain; browser recording can be a false-positive call classification.

## Host lifecycle and verification

Never replace helpers or trigger a reload under an active/requested screen lock.
Inspect the actual panel after updates: a log saying reload and a new helper PID
did not guarantee fresh QML in the installed host. Use only an authorized restart.

One live disable/re-enable test produced a normal owner exit and one replacement,
but Omarchy removed inline preferences. Known preferences were restored. This
does not establish teardown guarantees for hung descendants or every host version.

Before completion: aggregate tests, manifest validation and whitespace checks.
Runtime changes also require one owner, valid JSON and continuing logs. Separate
offline assertions, actual Qt checks, rendered UI checks and physical hardware
observations in every report. Release acceptance is tracked in RELEASE.md/BACKLOG.md.
