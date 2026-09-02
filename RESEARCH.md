# ROG Cetra SpeedNova Protocol Research

This document is the handoff for continued reverse engineering of the ROG
Cetra True Wireless SpeedNova receiver `0b05:1ad3`. It separates reproduced
facts from hypotheses so future work does not repeat closed branches or expose
unverified device commands.

Research snapshot: 2026-09-02.

## Safety boundary

- Do not fuzz HID opcodes or values on the headset.
- Do not send `cc 41 0b` until its value semantics are recovered from official
  ASUS code or a controlled Armoury Crate capture.
- Do not treat Telephony reports `05 31`, `05 33`, or `05 00` as microphone
  state commands.
- Do not expose software `Live`/`Muted` controls or an absolute mute indicator
  without a reproducible absolute hardware readback.
- Keep a single hidraw reader. Multiple readers race and consume each other's
  asynchronous reports.

## Supported hardware and Linux path

- Receiver VID:PID: `0b05:1ad3`.
- USB path during testing: `3-1`.
- HID path during testing: `/dev/hidraw0`, interface `3`.
- Linux drivers: `snd-usb-audio`, `usbhid`, and `hid-generic`.
- No dedicated Cetra kernel driver is involved.
- The interface descriptor exposes Telephony report ID `0x05` and vendor report
  ID `0xcc`.

## Reproduced commands

### Battery

Host request:

```text
cc 12 07
```

The Linux HID write contains a leading zero report selector and is 17 bytes:

```text
00 cc 12 07 00 00 00 00 00 00 00 00 00 00 00 00 00
```

Response fields:

- byte 6: left earbud battery;
- byte 7: right earbud battery;
- byte 8: case battery;
- `255`: component unavailable or in the case.

An unsolicited `cc 12 09` packet also carries device-status/battery values. In
the 2026-09-02 live trace it appeared when the next regular `cc 12 07` response
changed the left battery from `68` to `67`; it was not a microphone event.

### Noise control

Readback request:

```text
cc 12 25
```

Response byte 5:

- `0`: Off;
- `1`: ANC;
- `2`: Ambient.

Write command, as a 64-byte HID Output Report:

```text
cc 41 08 00 00 MODE
```

Off, ANC, and Ambient were each changed through the runtime socket and verified
through hardware readback without restarting Omarchy Shell.

### Call context

The standard Telephony HID Output Reports are:

```text
05 31  call context active
05 00  media context active
```

`05 31` enables the right-earbud call gesture. A physical right-earbud tap then
toggles the headset's internal microphone mute and plays its native
`microphone off/on` prompt. The internal toggle does not change ALSA, PulseAudio,
PipeWire, or EasyEffects mute state.

`05 31` and `05 33` do not emulate a physical tap, do not play the native voice
prompt, and are not reliable internal mute-state commands.

Automatic call-context detection was reproduced with a synthetic capture stream
named `WEBRTC VoiceEngine`. EasyEffects, `pw-record`, Voxtype, and recognition
keepalive streams are intentionally excluded.

## Microphone state: current evidence

A packet with this raw HID content was observed after physical right-earbud taps
in earlier hardware tests:

```text
cc 70 00 00 00 01 01
```

It behaves like an edge event in those observations, not an absolute state: the
payload is identical for both directions, and state had to be toggled locally.
It is not a stable contract:

- a 2026-09-02 tap that audibly produced `microphone off` did not produce
  `cc 70` or any other tap-specific report on the owned hidraw interface;
- the official ASUS R55ES parser in HAL 1.3.95.0 has no `0x70` branch and
  returns without dispatching a callback;
- an existing USBPcap capture contains no `cc 70` packet;
- the receiver exposes no confirmed absolute internal mute-state readback.

The plugin therefore uses the headset voice prompt as the authoritative state
and does not display an inferred `Live`/`Muted` value.

### Live trace evidence

The live trace SHA-256 was:

```text
3fad6ea9a9f34040208f4fc27bd4738c88316bcb261969d7bf93cd28e3bd295e
```

Relevant operations, with unrelated polling removed:

```text
16:40:41.354066 ioctl(..., "\x05\x31" => "\x05\x31") = 2
16:40:41.361831 read(..., "\x05\x01", 64) = 2
...
# User confirmed that the physical tap played "microphone off".
# There was no host write and no cc 70 input report for the tap.
...
16:41:11.159558 read(...,
  "\xcc\x12\x09\x00\x00\x43\x25\x64...", 64) = 64
16:41:11.718845 read(...,
  "\xcc\x12\x07\x00\x00\x05\x43\x25\x64...", 64) = 64
```

The `0x43` value is decimal `67`, confirming that `cc 12 09` was a battery
update matching the following regular battery response.

## Official ASUS package

Downloaded package:

```text
https://dlcdnets.asus.com/pub/ASUS/Accessory/Headset/ROG_CETRA_TRUE_WIRELESS_SPEEDNOVA/ROG_CETRA_TRUE_WIRELESS_SPEEDNOVA.zip
```

Package details:

- Armoury Crate Gear version: `1.0.1.14`;
- ZIP SHA-256:
  `098cb50673cd247f0b1ab60c1fd7dc6361e44999e20febe7f7b403d310d82d06`;
- model module: `Device/r55es/6867`;
- official capabilities:
  `Device/r55es/6867/6867/resources/src/_ref/caps.json`;
- generic function map:
  `SDK_HAL/FWHSPlugin/headset/index.js`.

Extracted binaries:

| File | Version / SHA-256 |
| --- | --- |
| `AacAudioHal_x64.dll` | `1.3.95.0`, `c252aee03409db836aaebdd8062f6464eef2b1313fb79afa4ba0ad135009969f` |
| `R2Clib64.dll` | `e7f1f6a1f5543b46075b9927243dd345908f62a0a2a8fee755882a51a583adb8` |
| `ArmouryAudioSDK.dll` | `ba65cf55093f67b377adebdcb051cfed7a9b44b4bacc59b1fed6527f7b2ba8af` |

## Official HAL findings

The addresses below are virtual addresses in the PE image with base
`0x180000000`. Subtract the image base for RVAs.

| Address | Finding |
| --- | --- |
| `0x180039f10` | `C_R55ES_Protocol` constructor; stores VID `0x0b05`, PID `0x1ad3`, report ID `0xcc`. |
| `0x180028fd0` | asynchronous HID reader; reads 65 bytes and removes raw report ID byte 0 before parser dispatch. |
| `0x180039760` | R55ES input parser. Top-level payload branches cover `0x12`, `0x71`, and `0x41`, but not `0x70`. |
| `0x180039ee0` | registers `0x180039760` as the receive callback. |
| `0x1800809f0` | device-status callback bridge. |
| `0x180080ba0` | scalar callback bridge for events 46, 57, and 903. |
| `0x180080ca0` | link-information callback bridge for event 902. |
| `0x180081e90` | `AacR55ES::SetFunction`. |
| `0x180082750` | `AacR55ES::GetFunction`. |
| `0x1800843d0` | R55ESBT response PDU parser. |
| `0x180084890` | R55ESBT notification PDU parser. |
| `0x180123990` | primary `AacR55ES` vtable. |
| `0x1801239d8` | secondary vtable containing `SetFunction` and `GetFunction`. |
| `0x180114400` | `C_R55ES_Protocol` vtable. |
| `0x1801492f8` | RTTI type descriptor for `AacR55ES`. |
| `0x180149318` | RTTI type descriptor for `AacR55ESBT`. |

The reader transforms a raw report as follows:

```text
raw HID:      cc 70 00 00 00 01 01 ...
parser input:    70 00 00 00 01 01 ...
```

The parser compares the first payload byte with `0x12`, `0x71`, and `0x41`.
Payload `0x70` reaches the return path without reading the trailing `01 01` or
calling a callback.

Supported R55ES callback events recovered from parser and registration paths:

| Payload after report-ID removal | Public event |
| --- | --- |
| `12 01 ...` / `12 08 ...` / `12 09 ...` | `8`, device status |
| `12 25 ...` | `46`, ANC type |
| `12 29 ...` | `57`, WDL mode |
| `71 01 ...` | `902`, device link information |
| `71 02 ...` | `903`, device pairing |

`SetFunction(19)` registers callbacks only for events `8`, `46`, `57`, `902`,
and `903`. It does not register device-level `MIC_VOLUME_CHANGED=60`.

### Function IDs checked

- Function `52` sends `cc 41 0a 00 00 VALUE`; official resources associate it
  with voice-prompt language/type settings.
- Function `53` sends `cc 41 0b 00 00 VALUE`; it configures gesture mode. The
  valid values and exact semantics remain unresolved. It is not yet evidence of
  a command that executes a gesture.
- Function `61`, `MIC_MUTE_INDICATOR`, is not implemented by R55ES
  `SetFunction`.
- Function `209`, `MUTE_STATE`, belongs to the separate Windows endpoint API in
  `ArmouryAudioSDK.dll`, where it uses `IAudioEndpointVolume`. R55ES
  `SetFunction(209)` and `GetFunction(209)` return `E_NOTIMPL`.

The R55ESBT response and notification parsers check protocol IDs `0x0071`,
`0x0112`, `0x0271`, `0x0812`, `0x0912`, `0x2312`, and `0x2512`. No `0x0070`
branch was found. A complete comparison of the BT `SetFunction` and
`GetFunction` switches is still required.

## G-Helper findings

The test build from <https://github.com/seerge/g-helper/issues/2867> contains:

- `CetraSpeedNova : AsusHeadset`;
- decimal device IDs `2821/6867`, equal to hexadecimal `0x0b05/0x1ad3`;
- `SetAnc` using `cc 41 08 00 00 MODE`;
- `SetVoicePrompt` using `cc 41 0a 00 00 VALUE`;
- no Cetra-specific microphone-toggle method.

Extracted `GHelper.dll` SHA-256:

```text
6cc1b2f4e4f7cd074c51e52652b88ae4bf645e2596b361e629bf6d20b0f2c1f6
```

## Existing USBPcap findings

Capture SHA-256:

```text
0fe94ebf4d26275388a5a653eb4c3a690388824c12e7bf2702297110895c524a
```

The capture contains vendor `cc` reports, but:

- no report `05` traffic;
- no USB Audio Class microphone `MUTE_CONTROL` request;
- no `cc 70 00 00 00 01 01` report;
- USB Audio control transfers are playback/microphone gain operations.

The capture was not annotated with physical tap times, so it cannot identify
the mic gesture path by itself.

## Firmware state observations

- Internal mute persisted across separate synthetic call sessions.
- Internal mute persisted across a helper restart.
- An earlier test indicated that placing the earbuds in the case resets the next
  call session to Live, but the complete controlled case cycle described below
  has not yet been repeated after the latest runtime changes.

These observations concern headset behaviour only. They do not provide an
absolute host-readable state.

## Operational finding: Omarchy hot reload

Quickshell crash PID `1079563` was a confirmed Omarchy/Quickshell hot-reload
issue, not a Cetra protocol crash. The local plugin edit was only the reload
trigger. Upstream issue: <https://github.com/omacom/omarchy/issues/9441>.

The setup script must not replace helper binaries while
`omarchy-shell lock status` reports `locked`, `requested`, or `secure` as true.
No new coredump was observed after adding that guard.

## Exact next plan for faster models

Work in this order. Do not modify the plugin until a step produces a reproduced
fact that changes runtime behaviour.

### 1. Finish function 53 statically

1. Analyze `AacR55ES::SetFunction` at `0x180081e90` and
   `AacR55ES::GetFunction` at `0x180082750`.
2. Isolate switch case `53` and follow every range check, lookup table, enum
   conversion, and call into `C_R55ES_Protocol::mutex_setCmd`.
3. Recover all accepted input values and the corresponding
   `cc 41 0b 00 00 VALUE` bytes.
4. Search official JS, JSON resources, localized strings, and model modules for
   the names represented by those values.
5. Determine whether function 53 only assigns gestures or can execute one.
6. Record addresses and assembly excerpts. Do not test values on hardware.

Success criterion: a complete value-to-meaning table backed by official code,
or a precise proof that this build does not expose such a table.

### 2. Complete the Bluetooth comparison

1. Recover `AacR55ESBT` vtables through RTTI at `0x180149318`.
2. Identify BT `SetFunction` and `GetFunction` entry points.
3. Compare their supported function IDs with USB R55ES, especially `53`, `60`,
   `61`, and `209`.
4. Trace all mic, mute, gesture, telephony, and voice-prompt strings and xrefs.
5. Check calls into `R2Clib64.dll` for a transport-level mic command not visible
   in the R55ES parser.

Success criterion: a side-by-side USB/BT function table and exact outgoing bytes
for any BT-only mic operation. A name or string without a send path is not
enough.

### 3. Capture a controlled physical toggle at USB level

Capture every interface and endpoint for USB device `3-1`, not only the hidraw
file owned by `cetra-watch`.

Test matrix:

1. Put both earbuds in the case long enough to establish a fresh baseline.
2. Take them out and start a synthetic `WEBRTC VoiceEngine` capture.
3. Confirm that `05 31` was sent and the plugin reports call context active.
4. Start `usbmon`/Wireshark capture for the complete device.
5. Tap the right earbud once and verbally record `microphone off`.
6. Wait two seconds, tap again, and record `microphone on`.
7. Stop capture immediately and annotate exact monotonic/wall-clock times.
8. Diff all interrupt, control, isochronous, HID, and USB Audio transfers in a
   two-second window around each tap.
9. Repeat once with no call context to identify the media-gesture difference.

Look specifically for:

- report IDs other than `0xcc` on sibling HID interfaces;
- Consumer/Telephony input reports;
- USB Audio Class mute controls;
- vendor control transfers;
- reports that differ between `off`, `on`, and media-context taps;
- an absolute state response following an official read request.

Success criterion: two independently reproduced, direction-labelled captures.
An identical edge event may support gesture detection but not absolute state.

### 4. Differential capture under official Armoury Crate

Only if static analysis does not reveal an execution command:

1. Capture Armoury Crate startup without touching controls.
2. Capture each exposed gesture configuration change one at a time.
3. Capture any official microphone/mute UI action, if the model UI exposes one.
4. Diff Host-to-Device transfers against the idle capture.
5. Replay nothing until the command is identified in official code and its
   argument domain is bounded.

Success criterion: a Host-to-Device command emitted specifically by an official
mic action, followed by the same internal toggle and native voice prompt.

### 5. Repeat the case reset lifecycle

1. Establish call context and tap until `microphone off` is heard.
2. End and restart the synthetic call; verify the headset remains muted.
3. Restart `cetra-watch`; verify the headset remains muted.
4. Put both earbuds in the case and wait for battery readback with both earbuds
   unavailable.
5. Take them out, establish call context, and perform the first right-earbud tap.
6. If the prompt is `microphone off`, the case reset to Live is confirmed. If it
   is `microphone on`, the earlier reset observation was wrong or timing-specific.
7. Repeat the full cycle once before documenting it as stable firmware behaviour.

### 6. Real application verification

After protocol work, verify automatic call context with current versions of:

- Discord or Vesktop;
- Steam voice chat;
- Telegram;
- one Chromium or Firefox WebRTC call.

For each app, verify `05 31` on capture start, `05 00` after capture stops, native
right-earbud prompts during the call, and normal media gesture outside the call.

### 7. Plugin changes after evidence

- Add a software toggle only if a Host-to-Device execution command is reproduced
  and confirmed to play the native prompt.
- Add an absolute `Live`/`Muted` display only if an absolute readback is found.
- If only a reliable edge event exists, expose at most a recent gesture event,
  not persistent mute state.
- Update this document with artifact SHA-256 values, exact bytes, timestamps,
  and function addresses before changing README claims.

## Final release checklist

- `./tests/run.sh` passes.
- `omarchy plugin validate .` passes.
- `git diff --check` passes.
- A clean source-only clone can run `./setup` while the session is unlocked.
- One and only one `cetra-watch` owns the receiver after setup and reconnect.
- Battery and all three noise-control modes pass hardware readback.
- Automatic call context is tested in at least one real communication app.
- No undocumented Host-to-Device command is present in source or UI.
