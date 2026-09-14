# ROG Cetra SpeedNova Protocol Research

Source-map update (2026-09-14): native report builders, parsing, commands and
filesystem code now live in `daemon/` under the same `cetra-watch.c` translation
unit. See MODULES.md. Protocol bytes and documented observations were not changed
by relocation. Historical source line numbers below must not be treated as current.

This document is the handoff for continued reverse engineering of the ROG
Cetra True Wireless SpeedNova receiver `0b05:1ad3`. It separates reproduced
facts from hypotheses so future work does not repeat closed branches or expose
unverified device commands.

Initial research snapshot: 2026-09-02; dated follow-ups below extend it.

Documentation correction: 2026-09-06. PCM silence observations below do not
establish absolute mute readback. `BACKLOG.md` owns current implementation and
verification status; historical captures are not evidence that a current
runtime implementation has passed tests.

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

### Present earbud with unavailable battery, 2026-09-14

The user reported both earbuds in use while the left percentage was missing.
Owner telemetry last showed left=89 at 07:55:31.600; at 07:56:20.316 battery
readback was L=ff/R=60/Case=55 (hex), while later presence reports remained 11.
Later owner startup also received L=ff. This is missing battery telemetry despite
reported availability, not evidence the earbud is absent or empty. The UI keeps
the present icon active and labels charge unknown. No percentage is reconstructed
from the other side, PCM, presence bits or old logs.

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
- `255`: battery value unavailable. This does not establish physical case placement;
  later trials also observed it with confirmed presence.

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

### ANC Level & Adaptive ANC

ANC Level readback request:

```text
cc 12 2b
```

Response byte 5:
- `1`: Low
- `2`: Mid
- `3`: High

Write command, 64-byte HID Output Report:

```text
cc 41 0c 00 00 LEVEL
```

Adaptive ANC readback request:

```text
cc 12 2c
```

Response byte 5:
- `0`: Disabled (Manual)
- `1`: Enabled (Smart / Adaptive)

Write command, 64-byte HID Output Report:

```text
cc 41 0d 00 00 ENABLED
```

### Voice Prompt Language

Readback request:

```text
cc 12 28
```

Response byte 5:
- `0`: Prompt Sound (Beeps)
- `1`: English
- `2`: Chinese

Write command, 64-byte HID Output Report:

```text
cc 41 0a 00 00 VALUE
```

### In-Ear Detection (Proximity)

Readback request: `cc 12 26` (byte 5: `0` = Off, `1` = On).
Write command, 64-byte HID Output Report: `cc 41 09 00 00 VALUE`.

#### USB proximity event delivery

The 2026-09-13 owner log confirms `proximity on` at 13:17:16.501 and readback
`in_ear=1` at 13:17:16.613, with further Off/On readbacks at 13:18:15.597 and
13:18:27.101. These establish the setting value only. The user reported an
earbud tone on removal without a PC playback pause; no synchronized removal
marker or comparative call-context-off trial accompanies that report.

No verified command for forwarding proximity events to USB was identified in
the documented HAL inventory. This does not prove such a command cannot exist,
nor establish that auto-pause works exclusively over Bluetooth AVRCP. The `01`
presence response describes earbud availability, not in-ear sensor state.

Further acceptance requires marked removal/insertion trials with one existing
HID owner, verified proximity readback and separately recorded requested call
context, followed by comparison with official software or Bluetooth behavior.
Do not fuzz opcodes or synthesize player actions from availability reports.

### Sidetone

Readback request: `cc 12 24` (byte 5: `0` = Off, `1` = On).
Write command, 64-byte HID Output Report: `cc 41 11 00 00 VALUE`.

### Aura RGB Lighting

Write command 1 (64-byte HID Output Report):

```text
cc 51 28 00 00 01 EFFECT R G B 00 ...
```

Effects:
- `0`: Off
- `1`: Static
- `2`: Breathing
- `3`: Strobing
- `4`: Color Cycle

Runtime sequence clarification (2026-09-14): `set_lighting()` sends zone 1 then
zone 0 and commits twice. Runtime Off is effect 1 (Static) with RGB 0/0/0, not
effect 0 as listed in the historical effect inventory above. Exact official Off
traffic and the necessity of both commits remain unverified. Mock transaction
tests establish software error handling, not the official protocol sequence.

Write command 2 (commit / save, 64-byte HID Output Report):

```text
cc 50 55 00 00 00 ...
```

### Hardware Equalizer (10 Bands)

Write command, 64-byte HID Output Report:

```text
cc 41 04 00 00 B0 B1 B2 B3 B4 B5 B6 B7 B8 B9
```

Bands correspond to frequencies: 125, 250, 500, 1K, 2K, 4K, 8K, 16K.

This historical list has eight frequencies for ten payload values. The mapping
is incomplete; no ten-band UI or arbitrary EQ write is authorized by this note.

### Microphone PCM Silence Observations (Not Absolute Readback)

Earlier research reported zero-valued PCM after the `Microphone off` prompt and
a noise floor after `Microphone on`, using a 50 ms buffer from
`alsa_input.usb-ASUSTek_ROG_CETRA_TRUE_WIRELESS_SPEEDNOVA_0000000000000000-00.mono-fallback`.
These are observations of one capture path, not proof that the earbud ADC
disconnects or that live input always has a particular noise floor.

Software mute in the audio graph, a gate, routing, processing, or capture failure
can also produce silence. Combining PCM silence with an edge notification does
not establish absolute headset-native mute state or eliminate desynchronization.
The earlier deterministic-detection claim was erroneous: production code did
not implement this proposed PCM check, and it is not a validated mute readback.
The native voice prompt remains the user's authoritative indication.

### Call context and Gesture Architecture

The standard Telephony HID Output Reports are:

```text
05 31  call context active
05 00  media context active
```

The vendor gesture input report is `0xcc 0x70` (64 bytes):

- **Byte 5 (Earbud):**
  - `0x00`: Left earbud
  - `0x01`: Right earbud
- **Byte 6 (Gesture):**
  - `0x01`: Single tap
  - `0x02`: Double tap
  - `0x03`: Triple tap
  - `0x00`: Long press (with byte 7 = `0x01`)

Concurrent Consumer Control report `0x0c` (2 bytes):
- `0c 08`: `Usage: Play/Pause` (mapped to Linux `KEY_PLAYPAUSE`)
- `0c 10`: `Usage: Next Track`
- `0c 00`: Key release

#### Historical gesture behavior matrix (conditional observations):

These observations are not unconditional firmware guarantees. The later
continuous-capture trial below reproduced missing media-key delivery despite
call_context=false. Context requests, vendor tap reports and Consumer keys are
separate evidence.
| Earbud | Gesture | Packet Payload | Context | Hardware Behavior |
|---|---|---|---|---|
| Left (`00`) | Single tap | `cc 70 .. 00 01` | Any | Emits `0c 08` (`KEY_PLAYPAUSE`). Mic untouched. |
| Left (`00`) | Double tap | `cc 70 .. 00 02` | Any | Toggles ANC mode in hardware. Mic untouched. |
| Right (`01`) | Double tap | `cc 70 .. 01 02` | Media | Emits `0c 10` (Next Track). Mic untouched. |
| Right (`01`) | Double tap | `cc 70 .. 01 02` | Call | Answer / End call. Mic untouched. |
| Right (`01`) | Long press | `cc 70 .. 01 00 01`| Any | Decline call / Voice assistant. Mic untouched. |
| Right (`01`) | Single tap | `cc 70 .. 01 01` | Media (`05 00`) | Emits `0c 08` (`KEY_PLAYPAUSE`). Stops/plays video. Mic untouched. |
| Right (`01`) | Single tap | `cc 70 .. 01 01` | Call (`05 31`) | Native Mute/Unmute toggle with voice prompt (`microphone off/on`). |

`05 31` enables the right-earbud call gesture. A physical right-earbud tap then
toggles the headset's internal microphone mute and plays its native
`microphone off/on` prompt. Outside calls (`05 00`), right-earbud tap is a media
gesture (`KEY_PLAYPAUSE`), which pauses/resumes active media without muting.

Historical implementation used application-name capture matching and offered
"Always Mute Gesture". The persistent override was subsequently removed. Current
automatic requests require a verified Cetra capture path and communication
classification; see HANDBOOK.md. Neither policy proves native mute state.

### Right tap and Consumer Play/Pause, 2026-09-13

Existing owner PID `318143` logged these local times (UTC+03:00):

```text
13:14:06.271 CALL_CONTEXT: requested=active force=false
13:14:06.277 TELEPHONY_EVENT: state=0x01
13:17:23.009 GESTURE: right earbud single-tap in call observed (seq=1, microphone_state=unknown)
13:17:23.059 CONSUMER_KEY: usage=0x08
```

The two input events are 50 ms apart, not simultaneous. The installed Omarchy
media bindings map XF86AudioPlay/XF86AudioPause to `omarchy-shell media playPause`.
The daemon only logs Consumer Control packets; reading or ignoring them through
hidraw does not consume the kernel input event.

`call_context` stores aggregated requested intent in `sync_call_context()`, not
hardware readback. Telephony input is logged, not reconciled with that field.
The earlier controlled two-tap capture below had no companion Consumer report.
The evidence establishes that media-key suppression is not guaranteed, not that
every call-mode tap emits Play/Pause or that native mute toggled in this trial.
No verified suppression opcode is documented.

Subsequent user observation: during an actual ongoing call, right taps did not
pause video. This is a user report without a new synchronized packet capture;
it further limits the earlier event pair to its observed context. No general
call-mode media-key suppression workaround is enabled.

Additional user observation: outside a real call, enabling the persistent call
request still resulted in media pause rather than the expected native microphone
gesture. This does not identify the missing precondition (active capture, audio
profile or other firmware context). The UI labels this as an experimental
request and never treats the requested bit as confirmed tap behavior. Subsequent
user decision removed the manual control altogether; legacy saved values are
ignored. Automatic capture-driven requests and daemon call IPC remain.

### ANC readback timing, 2026-09-14

Existing owner telemetry records `anc_level 3` at 01:54:16.367 and level-3
readback at 01:54:24.539 (8.172 seconds); level-1 request at 01:54:09.347
was followed by level-1 readback at 01:54:14.421 (5.074 seconds). Immediate
readback is not sufficient to establish the applied value. The UI's old nominal
three-second timeout was shorter than the existing ten-second periodic query
cycle. The corrected UI allows 48 scheduler ticks at 250 ms and clears failures
when a matching late reply arrives. This is UI policy, not ASUS timing guarantees.

A system-level filter would require separate implementation and acceptance,
preferably scoped to the receiver's input device and an explicit user policy.
A global key guard also affects other keyboards. Sending a compensating
`playPause` is not reliable cancellation because it toggles playback state.

## Microphone state: current evidence

A packet with this raw HID content was observed after physical right-earbud taps
in earlier hardware tests:

```text
cc 70 00 00 00 01 01
```

It behaves like an edge event in those observations, not an absolute state: the
payload is identical for both directions. Toggling a local bit was an inference,
not a hardware readback.
It is not a stable contract:

- a 2026-09-02 tap that audibly produced `microphone off` did not produce
  `cc 70` or any other tap-specific report on the owned hidraw interface;
- the official ASUS R55ES parser in HAL 1.3.95.0 has no `0x70` branch and
  returns without dispatching a callback;
- an existing USBPcap capture contains no `cc 70` packet;
- the receiver exposes no confirmed absolute internal mute-state readback.

The current UI therefore displays `Unknown`, not inferred `Live`/`Muted`; the
user follows the headset voice prompt. The plugin does not listen to or decode
that prompt. The daemon correction in progress defines
`"microphone_state":"unknown"` as no confirmed absolute hardware state and
retains `tap_seq` only as an observed in-call right-earbud single-tap report
count. Neither this counter nor its parity is a mute state. Removal of daemon
`mic_live` and `mic_state` awaits primary-agent verification under P0.1 in
`BACKLOG.md`.

The prefix shown above has 7 bytes. If a received report ends there, byte 7 is
absent; reading a sub-gesture requires a report length of at least 8 bytes. Full
captures below are 64 bytes. This parser boundary does not assert that those
captures were truncated or change any verified opcode or gesture semantics.

### Live trace evidence

#### First trace (negative edge capture)
SHA-256: `3fad6ea9a9f34040208f4fc27bd4738c88316bcb261969d7bf93cd28e3bd295e`.
In this trace, an audible `microphone off` did not produce a `cc 70` packet on the
interface, showing edge detection was not guaranteed in all earlier helper states.

#### Controlled two-tap trace (2026-09-02)
SHA-256: `c24e60240c1af3465c2bfedc0763cfb7c8a491d2200ead4ceb797f51477082c8`.

With call context active (`05 31` acknowledged by `05 01`), the user performed two
consecutive physical right-earbud taps:

1. `21:12:07.936399`: First tap, earbud audibly prompt: `microphone off`.
   Incoming HID packet on `/dev/hidraw0`:
   ```text
   cc 70 00 00 00 01 01 00 00 00 00 00 00 00 00 00 ... (64 bytes)
   ```
2. `21:12:14.156970`: Second tap (6.2s later), earbud prompt: `microphone on`.
   Incoming HID packet on `/dev/hidraw0`:
   ```text
   cc 70 00 00 00 01 01 00 00 00 00 00 00 00 00 00 ... (64 bytes)
   ```

Key facts established:
- Exactly two `cc 70` packets were received across the entire recording session,
  matching the two physical taps 1:1.
- Both packets are byte-for-byte identical (`01 01`).
- No other HID report (`0x05`, `0x0c`, or `0xcc`) or Audio Control transfer
  accompanied the taps.
- This confirms `cc 70 00 00 00 01 01` is strictly an edge notification of a
  gesture tap, not an absolute mute/unmute state.
- Because the headset toggles mute internally and no absolute readback has been
  confirmed, inferring `Live`/`Muted` from these edges can desynchronize. The
  native voice prompt remains the user's authoritative mute indication.

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
  with voice-prompt language/type settings (`0: English (0x01)`, `1: Chinese (0x02)`, `2: Prompt Sound (0x00)`).
- Function `53` (`GESTURE_MODE`) sends `cc 41 0b 00 00 VALUE` via `0x18008235b`.
  Static analysis confirms:
  - Input: takes a single byte from `[rdi + 4]`, with no range validation.
  - `AacR55ES::GetFunction(53)` at `0x180082e11` returns `0x80004001` (`E_NOTIMPL`).
  - There is no readback request opcode (no `12 0b`) in `C_R55ES_Protocol`.
  - In `caps.json` for both USB (`6867`) and Bluetooth (`6869`), `hasGestureMode` is
    absent (`undefined`). This establishes that the inspected resources do not
    expose custom remapping, not that every firmware path lacks support.
    The Armoury Crate UI only displays static user manuals
    (`userManual.gesture`) and never invokes Function `53`.
  - In the generic SDK (`headset/index.js`), `GESTURE_MODE` is a configuration setter
    for side assignment (`0: BOTH`, `1: LEFT`, `2: RIGHT`), not an execution command.
    It does not execute or emulate a physical gesture.
- Function `60`, `MIC_VOLUME_CHANGED`, returns `E_NOTIMPL` in both USB and BT.
- Function `61`, `MIC_MUTE_INDICATOR`, returns `E_NOTIMPL` in both USB and BT.
- Function `209`, `MUTE_STATE`, belongs to the separate Windows endpoint API in
  `ArmouryAudioSDK.dll`, where it uses `IAudioEndpointVolume`. R55ES
  `SetFunction(209)` and `GetFunction(209)` return `E_NOTIMPL`.

### Bluetooth HAL comparison (`AacR55ESBT`)

Type descriptor: `0x180149308` (`.?AVAacR55ESBT@@`).
Primary vtable: `0x180123cd8`.
COM secondary vtable: `0x180123cf8`.
Entry points:
- `AacR55ESBT::SetFunction`: `0x180083240`.
- `AacR55ESBT::GetFunction`: `0x180083740`.

#### Side-by-side function table

| ID | Name | USB Set | USB Get | BT Set | BT Get | Notes |
|---|---|---|---|---|---|---|
| `2` | (Internal) | `0x1800821bf` | `0x180082bbf` | `0x1800832bd` | `0x1800837d3` | Basic device init |
| `7` | (Internal) | `E_NOTIMPL` | `0x180082c32` | `E_NOTIMPL` | `E_NOTIMPL` | USB only |
| `8` | `DEVICE_STATUS` | `E_NOTIMPL` | `0x180082d4c` | `E_NOTIMPL` | `E_NOTIMPL` | Polls `12 01`, `12 08`, `12 09` |
| `9` | `AI_MIC_SWITCH` | `0x1800823d7` | `0x180082af2` | `E_NOTIMPL` | `E_NOTIMPL` | Noise reduction toggle |
| `10` | `SLEEP_TIME` | `0x180082040` | `E_NOTIMPL` | `E_NOTIMPL` | `E_NOTIMPL` | USB sleep timer |
| `19` | `REG_CALLBACKS`| `0x180081f3d` | `E_NOTIMPL` | `E_NOTIMPL` | `E_NOTIMPL` | Registers events 8, 46, 57, 902, 903 |
| `30` | `GAMING_MODE` | `E_NOTIMPL` | `E_NOTIMPL` | `0x1800833bb` | `0x18008386a` | BT low-latency mode |
| `31` | `PROXIMITY_MODE`| `0x1800822cc` | `0x180082880` | `0x1800833d1` | `0x180083880` | In-ear detection |
| `32` | `ANC_LEVEL` | `0x180082226` | `0x180082834` | `0x180083310` | `0x180083834` | ANC level adjustment |
| `46` | `ANC_TYPE` | `0x180082280` | `0x18008284f` | `0x180083369` | `0x18008384f` | Mode (Off/ANC/Ambient) |
| `48` | `SIDE_TONE_SWITCH`| `0x180082432` | `0x1800829bb` | `0x1800834e0` | `0x1800839bb` | Sidetone toggle |
| `52` | `VOICE_PROMPT_TYPE`| `0x18008230a` | `0x1800828c5` | `0x18008340e` | `0x1800838c5` | Language / voice prompt |
| `53` | `GESTURE_MODE` | `0x18008235b` | `E_NOTIMPL` | `0x180083466` | `E_NOTIMPL` | Configuration setter; no readback |
| `54` | `DIRAC_SWITCH` | `0x180082399` | `0x180082934` | `0x1800834a3` | `0x180083934` | Dirac audio processing |
| `55` | `RESET_DEVICE_USB`| `0x180082470` | `E_NOTIMPL` | `0x18008351d` | `E_NOTIMPL` | Soft device reset |
| `56` | `SKU_ID` | `E_NOTIMPL` | `0x180082979` | `E_NOTIMPL` | `0x180083979` | Hardware revision / SKU ID |
| `57` | `WDL_MODE` | `E_NOTIMPL` | `0x180082d4c` | `E_NOTIMPL` | `E_NOTIMPL` | Wireless dongle mode |
| `59` | `ANC_ADAPTIVE` | `0x1800821e8` | `0x1800827ef` | `0x1800832d3` | `0x1800837ef` | Adaptive ANC toggle |
| `60` | `MIC_VOLUME` | `E_NOTIMPL` | `E_NOTIMPL` | `E_NOTIMPL` | `E_NOTIMPL` | Not implemented in HAL |
| `61` | `MIC_MUTE_IND` | `E_NOTIMPL` | `E_NOTIMPL` | `E_NOTIMPL` | `E_NOTIMPL` | Not implemented in HAL |
| `62` | `BATCH_EQ` | `0x180082139` | `E_NOTIMPL` | `0x18008354d` | `E_NOTIMPL` | Equalizer bands write |
| `209` | `MUTE_STATE` | `E_NOTIMPL` | `E_NOTIMPL` | `E_NOTIMPL` | `E_NOTIMPL` | Windows endpoint only |

`R2Clib64.dll` (`RvcLib.dll`) contains only Realtek DSP / I2C / UVC functions
(`ReadI2CRegister_DSP`, `WriteI2CRegister_DSP`, `UVC_Open`, etc.) used for Audio LED
Control on other hardware. It provides no transport or microphone controls for
R55ES.

#### Complete inventory of `C_R55ES_Protocol` requests

All host-initiated `0x12` / `0x41` / `0x71` request opcodes in `C_R55ES_Protocol`:

| Opcode | Method | Function |
|---|---|---|
| `12 00` | `mutex_getFWVersion` (`0x18003a345`) | Firmware version readback |
| `12 01` | `mutex_getTwsExist` (`0x18003a99e`) | Earbud presence readback |
| `12 02` | `mutex_getSkuId` (`0x18003bf7e`) | SKU ID readback |
| `12 03` | `mutex_getEffectInfo` (`0x18003c4da`) | Audio effect parameters |
| `12 07` | `mutex_getPowerInfo` (`0x18003c22a`) | Battery levels (L/R/Case) |
| `12 08` | `mutex_getChargingState` (`0x18003ac50`) | Charging state |
| `12 17` | `mutex_getDirac` (`0x18003bcbe`) | Dirac switch state |
| `12 24` | `mutex_getSidetoneOnOff` (`0x18003ca5e`) | Sidetone state |
| `12 25` | `mutex_getANC` (`0x18003af0e`) | Noise control mode (0/1/2) |
| `12 26` | `mutex_getProximity` (`0x18003b73e`) | In-ear detection state |
| `12 28` | `mutex_getLanguage` (`0x18003b9fe`) | Voice prompt language |
| `12 29` | `mutex_getWDLStatus` (`0x18003a6ee`) | Wireless dongle status |
| `12 2b` | `mutex_getANCLevel` (`0x18003b1be`) | ANC intensity level |
| `12 2c` | `mutex_getAdaptiveANC` (`0x18003b47e`) | Adaptive ANC setting |
| `41 20` | `mutex_getNROnOff` (`0x18003c79e`) | AI NR mic setting |
| `71 00` | `mutex_getLinkHistory` (`0x18003d1eb`) | Multipoint connection history |
| `71 01` | Link info | Device link information |
| `71 02` | Device pairing | Pairing state |

No microphone mute-state or gesture-configuration readback request was found in
this inventory of `C_R55ES_Protocol` in HAL 1.3.95.0. Together with the checked
function paths, it establishes that no host command for native mute or absolute
mute readback has been confirmed by this investigation, not that no such command
could exist in any firmware or other unexamined path.

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
- An earlier test suggested a case-cycle mute reset. The side-separated trials
  below qualify this observation: it must not be generalized to every docking
  event or to a guaranteed initial Live state.
- User observation (2026-09-06): after setting native microphone mute to Off,
  placing the earbud in the case and taking it out, the microphone was On again.
  The user also reports that the noise-control mode was unchanged. This supports
  a mute reset across the case cycle, not a reset of all headset settings.
  This report is not a time-correlated HID/audio capture, a repeatability count,
  or proof of which step (docking, charging, waking, or reconnecting) resets mute.
  Earbud absence or receiver reconnect alone must not be treated as a verified
  case cycle or used to publish an absolute microphone state.

These observations concern headset behaviour only. They do not provide an
absolute host-readable state.

### Side-separated mute trials, 2026-09-06

All times below are local UTC+03:00. The user listened to the native prompts and
checked whether speech passed in Discord. Audio was not recorded by the agent;
speech/prompt observations are user reports, not timestamped PCM measurements.
HID evidence is from the existing owner's telemetry, not a second hidraw reader.
The observed owner PID remained `3681891` across the accepted trials, with
`call_context=true`. No plugin files or settings were changed during the trials.

| Trial | Initial condition and action | User observation | Telemetry anchors |
| --- | --- | --- | --- |
| R1 | Both out, prompt Off and silence; dock only right, then retrieve without tapping | Silence after retrieval; first subsequent tap said On and speech returned | Mute tap 23:33:11.122; `08` byte 5=`10` at 23:33:19.290/.846; `01` byte 5=`01` at 23:34:14.983; right missing 23:34:16.603; right returns 23:34:22.671, `01`=`11` at .771; unmute tap 23:35:37.040 |
| L0, excluded | First attempted left trial | User reported forgetting to establish Off before docking | Events around 23:37 are retained as packet observations only, not mute-reset evidence |
| L1 | Both out, Off and silence confirmed; dock only left, then retrieve | Silence reported while docked; speech reported after retrieval, without a tap | Initial tap 23:39:21.617; `08`=`01` at 23:40:22.043/.701; `01`=`10` at 23:40:25.653; left missing 23:40:27.673; `01`=`11` at 23:41:23.243; left telemetry returns 23:41:23.343; tap counter stayed 5 |
| L2 | Both out, Off and silence confirmed; user continues speaking while docking only left | Speech resumed around docking/lid closure; exact ordering and subsecond timing unknown | Initial tap 23:45:12.340; `08`=`01` at 23:46:00.335; `01`=`10` at 23:46:03.937; left missing 23:46:06.314; tap counter stayed 6 |
| L2 continuation | Left remains docked; right tap; then retrieve left without another tap | Right tap stopped speech and said Off; speech resumed after left retrieval | Tap 23:48:00.742 (seq 7); `01`=`11` at 23:50:04.534; left telemetry returns 23:50:04.792; no new tap, restart or call-context command in this interval |
| R2 | Right-only docking/retrieval, user started the action before a separate checkpoint | User reports silence both during docking and after retrieval, speech returned only after tap | Initial tap 23:52:56.374; `08`=`10` at 23:53:06.972/07.834; `01`=`01` at 23:53:11.383; right missing 23:53:13.455; returns 23:53:51.859; `01`=`11` at 23:53:53.270; final tap 23:54:22.035 |

L1 and L2 differ in the reported point when speech resumed. Do not describe
them as two identical timed docking resets. Together they establish a reported
loss of effective mute across left-side transitions; L2 localizes one occurrence
to docking and another to retrieval after re-muting. R2 has weaker initial-state
control than R1 and is corroboration, not an identical controlled repetition.
The earlier tests around 15:11-15:12 included daemon restarts and must not be
combined with these stable-owner trials to prove a pure case-only transition.

The results are consistent with microphone-role handoff or mute reset when left
availability changes. They do not establish a permanent left master, the physical
microphone carrying speech, the storage location of mute, or a firmware guarantee.
The side holding the touch control does not prove which side owns audio state.
Effective mute can be lost without a tap: counting taps is insufficient even
when every reported tap in a particular interval is received.

### Presence/charging candidate layout and verification boundary

The correlation-only assessment below records the initial investigation. The
following HAL re-verification section supersedes its extraction blocker and
bit-versus-nibble uncertainty; physical charging-current and mute caveats remain.

Raw offsets count the leading `cc` as byte 0. Hexadecimal byte values, not decimal
numbers, are shown below. The earlier HAL notes associate request `12 01` with
`mutex_getTwsExist` and `12 08` with `mutex_getChargingState`; that identifies the
request family, not the precise response-field layout.

| Raw response | Observed byte 5 | Correlated action | Current confidence |
| --- | --- | --- | --- |
| `cc 12 01` | `01` | Right unavailable, left available | Observed side correlation |
| `cc 12 01` | `10` | Left unavailable, right available | Observed side correlation |
| `cc 12 01` | `11` | Both available | Observed side correlation |
| `cc 12 08` | `01` | Left docked | Docking/charging candidate, not proof of charging current |
| `cc 12 08` | `10` | Right docked | Docking/charging candidate, not proof of charging current |
| `cc 12 08` | `00` | Seen after right retrieval | Neither-side flag candidate |

A low-nibble left/high-nibble right encoding fits these observations. A pair of
single-bit flags (bits 0 and 4) and two nibble-valued fields are indistinguishable
on values `00/01/10/11`; do not choose one without checking other values or the
official parser. Byte 6 was zero in the selected trials, but earlier logs contain
`08` reports with byte 6=`01`. Its meaning remains unknown and must not be dropped
from evidence or silently treated as padding.

Battery reports on left retrieval briefly alternated `(L,ff,caseA)` and
`(ff,R,caseB)` before converging. The existing battery debounce retains values;
its combined `connected` field is not a reliable case-state oracle, and neither
battery `255` nor a receiver reconnect proves a docking cycle.

Static HAL re-verification attempt (2026-09-07): no local DLL/archive or saved
disassembly was found in the checked project, temporary, Downloads and state
directories. The documented ASUS URL answered HEAD with HTTP 200, length
394449221, but GET attempts including retries and HTTP/1.1 failed with curl 35
(`TLS unexpected eof while reading`). No downloaded ZIP/DLL hash was verified.
The hashes and VA addresses in the earlier HAL section remain historical notes,
not independently re-established evidence for this response layout.

To resume without repeating user trials:

1. Obtain the official archive and verify its documented SHA-256; never execute
   the Windows binaries. If changed, identify the new version before using VAs.
2. Inspect `0x180039760` branches for `01` and `08`, accounting for the reader
   removing raw report ID byte 0; raw byte 5 becomes parser offset 4.
3. Record actual masks/shifts and byte-6 accesses, then follow assignments through
   `0x1800809f0` to event 8 and the UI's status/charging interpretation.
4. Preserve packet-length and invalid-domain cases in offline tests before adding
   decoded fields. Do not infer absolute mute or add polling/unknown commands.

Until that verification, `01` and `08` stay raw telemetry; no presence/charging
runtime decoder or automatic risk notification is claimed as implemented.

### HAL re-verification completed, 2026-09-07

The download blocker was bypassed without disabling certificate validation:
`curl --ipv4 --http1.1 --tlsv1.2 --tls-max 1.2` downloaded the official archive.
This combination worked where earlier default GET requests failed; the exact
TLS/network failure cause was not isolated by varying one parameter at a time.
ZIP SHA-256 exactly matches `098cb50673cd247f0b1ab60c1fd7dc6361e44999e20febe7f7b403d310d82d06`.
Extracted HAL SHA-256 exactly matches
`c252aee03409db836aaebdd8062f6464eef2b1313fb79afa4ba0ad135009969f`.

Extraction was static: ZIP -> WiX attached CAB -> x64 MSI -> internal CAB -> DLL.
No Windows executable, installer or ASUS JavaScript was executed. The attached
CAB SHA-1 `b74ea7795e720fb0492797508bbe878ce60209d9` and MSI SHA-1
`66ea2aed89efa0750754f9e367fa627f9687eb27` also match the Burn manifest.

| Response/field | Verified HAL processing | Evidence VA |
| --- | --- | --- |
| `01` raw byte 5 | Stored at protocol offset `0x73`; bit `0x01` controls first/L battery availability, bit `0x10` second/R | `0x18003982c-0x18003983a`, `0x180080b64`, `0x180080b82` |
| `01` aggregate | Separately compares the entire byte with `0x11`; not equivalent to the per-side checks on unsupported values | `0x180080b51` |
| `08` raw byte 5 | Stored at `0x75`; bit `0x01` first/L charging, bit `0x10` second/R charging; `0xff` explicitly produces false for both | `0x180039875-0x18003988a`, `0x1800809a1-0x1800809d3` |
| `08` raw byte 6 | Stored at `0x76`, zero-extended unchanged into third charging slot | `0x180039875-0x18003988a`, `0x1800809d9-0x1800809dd` |

Reader instructions at `0x180029094` / `0x1800290ef` remove the report ID, so raw
byte 5 is parser offset 4, and raw byte 6 is parser offset 5. The semantic `01`
branch does not consume raw byte 6. The status branches use `test ... 0x01` and
`test ... 0x10`, not `0x0f` masks or shifts decoding two nibble-valued enums.

The event-8 status payload begins at `0x18014b618`. Charging slots occupy indices
1, 8, 10; battery slots occupy 2, 9, 11. The official headset SDK consumes those
charging positions; USB caps specify `power.batteryList=["L","R","Case"]`.
The official UI treats charging string `"1"` as true (and requires battery below
100 for its charging display). Thus raw `08` byte 6 is used as Case charging
status by the HAL/SDK/UI chain, not padding. The intervening native-to-JS
serializer was not separately reverse-engineered in this pass.

These facts verify the software interpretation, not electrical charging current
or the firmware meaning of every possible value of byte 6. Preserve its raw
value and treat unverified domains conservatively. Neither message is absolute
microphone mute readback, and neither proves a permanent master-earbud role.
Passive runtime decoding was subsequently implemented (2026-09-07) without new
queries. It exposes nullable per-side presence/charging and case-charging fields,
retains the raw bytes, and expires decoded values after 30000 monotonic ms. This
TTL is a conservative plugin policy, not a measured device reporting period.
Only presence masks 00/01/10/11 and charging masks 00/01/10/11/ff are interpreted;
case byte 6 is interpreted independently only for 0/1. Unknown domains preserve
their raw value but do not become a boolean claim. Short reports do not refresh
the previous observation. Receiver reset clears both report families.
Battery/connected/call/microphone state remain independent; no UI state or mute
inference was added. With event-only delivery the new fields may legitimately
expire while device state remains unchanged. Raw bytes then describe history.

Offline acceptance includes all 256 value domains, packet lengths 0..64 with
exact-size allocations under ASan/UBSan, independent freshness boundaries and
an actual owner-loop expiry test covering stdout, cache and clients without new
HID input. Removing the final owner publication makes that expiry test fail.

Historical address precision: `0x18003a99e` and `0x18003ac50` are opcode-store
instructions inside the presence and charging getter functions. The respective
function entries are `0x18003a970` and `0x18003ac20`.

Local reproducibility artifacts (temporary, may be removed on reboot):

- `/tmp/opencode/cetra-asus-tls12-20260907.zip`
- `/tmp/opencode/cetra-hal-extract-20260907/AacAudioHal_x64.dll`
- `/tmp/opencode/cetra-hal-extract-20260907/FINDINGS.md`
- Same directory: `reader.asm`, `parser.asm`, `status-callback.asm`, getter
  disassemblies, extraction scripts and SDK/UI text excerpts.

## Operational finding: Omarchy hot reload

2026-09-14 UI acceptance found a stale QML panel after logged local-plugin reloads:
the removed auto-pause row still appeared while its deleted locale key fell back
to English. One explicitly authorized shell restart loaded the current panel and
removed that row. A reload log and a new helper PID therefore do not prove current
QML source delivery. Verify a source-specific visible change in the running shell;
request separate restart permission if needed. The exact cache invalidation
mechanism was not isolated, and the packaged shell was not modified.

Quickshell crash PID `1079563` was a confirmed Omarchy/Quickshell hot-reload
issue, not a Cetra protocol crash. The local plugin edit was only the reload
trigger. Upstream issue: <https://github.com/omacom/omarchy/issues/9441>.

The setup script must not replace helper binaries while
`omarchy-shell lock status` reports `locked`, `requested`, or `secure` as true.
No new coredump was observed after adding that guard.

## Audio peak routing: 2026-09-14

This is audio-server evidence, not native mute evidence. The installed host had
libpulse 17.0-98-gb096 and PipeWire 1.6.8. EasyEffects and Discord were running.

- A libpulse peak stream with PA_STREAM_DONT_MOVE alone was redirected to
  easyeffects_source. Device-name validation rejected it before numeric output.
- Installed WirePlumber `scripts/linking/find-defined-target.lua`, lines 36–68,
  ignores metadata target overrides when node.dont-move=true. PipeWire's Pulse
  PA_STREAM_DONT_MOVE handling supplies dont-reconnect, which is not equivalent.
- Adding node.dont-move=true retained the physical Cetra source. The Pulse
  auto-suspend flag still left the stream suspended and its link paused despite
  the physical source actively feeding EasyEffects. Removing that flag allowed
  peaks. Setting node.passive=in-follow retained working capture. Current upstream
  capture handling also uses in-follow for dont_inhibit_auto_suspend; installed
  1.6.8 exposed passive=true when given that Pulse flag.
- The final helper uses peak detection, DONT_MOVE, dont-move/dont-fallback and
  in-follow. An eight-second isolated trial produced 156 numeric frames / 160,
  range 0.060–0.704 on the visual cube-root scale. Its source was Cetra; existing
  streams had unchanged source indices. Closing stdin exited zero and removed
  only its stream. Installed opt-out likewise removed the helper/stream while
  Discord remained on easyeffects_source. No EasyEffects settings were edited.
- The installed panel displayed 25%, with Unknown native mute. Complete external
  capture cessation and physical USB reconnect were not exercised in this trial.
  EasyEffects/keepalive links can keep the physical input externally active even
  after a call ends; this is not the meter sustaining its own admission gate.

Sources consulted (upstream master is supporting provenance, not a claim that
the installed binary exactly matches it):
- https://raw.githubusercontent.com/pulseaudio/pavucontrol/master/src/mainwindow.cc
- https://raw.githubusercontent.com/PipeWire/pipewire/master/src/modules/module-protocol-pulse/pulse-server.c
- https://raw.githubusercontent.com/wwmm/easyeffects/master/src/pw_node_manager.cpp
- `/usr/share/wireplumber/scripts/linking/find-defined-target.lua`

## Continuous capture suppresses media gestures: 2026-09-14

A marked A/B/A trial reproduced missing Play/Pause outside a requested call:

1. With voxtype-mic-keepalive.service capturing easyeffects_source to /dev/null,
   call_context was false and no Cetra meter was running. Left/right vendor tap
   reports arrived, but no accompanying Consumer 0x08 report was logged; user
   reported neither tap controlled video. A separately authorized native Omarchy
   media playPause command changed Chromium from Playing to Paused.
2. User authorized temporary stop of that keepalive service. Source-output list
   became empty. Both media taps worked according to the user. Left tap at
   13:45:28.288 was followed by Consumer 0x08 at :28.389; right tap at
   13:45:40.205 was followed by Consumer 0x08 at :40.355. Another right pair
   appeared at 13:45:43.614/:43.664.
3. The service was restored (active; enabled configuration unchanged). User
   confirmed the media tap failed again. No headset command or application
   routing change was introduced to manufacture recovery.

This establishes an interaction with continuous capture through this host's
Voxtype/EasyEffects path. The internal firmware mechanism and universality across
other capture paths remain unknown. call_context=false does not guarantee native
media-key delivery. The daemon's historical "outside call -> media play/pause"
log wording denotes a gesture classification, not proof of Consumer delivery.
Do not inject synthetic Play/Pause from vendor taps: genuine Consumer events may
also arrive, creating duplicate toggles. Changing Voxtype's persistent capture
policy requires separate user authorization outside the Cetra plugin.

## Remaining investigation plan

Work in this order. Do not modify the plugin until a step produces a reproduced
fact that changes runtime behaviour.

### 1. Function 53 static analysis (Completed)

Resolved statically:
- `AacR55ES::SetFunction(53)` (`0x18008235b`) and `AacR55ESBT::SetFunction(53)` (`0x180083466`)
  send `0x41 0x0b` (`cc 41 0b 00 00 <byte>`) without range check or dispatching callbacks.
- `GetFunction(53)` returns `E_NOTIMPL` on both USB and BT.
- No `12 0b` readback opcode exists in `C_R55ES_Protocol`.
- `caps.json` omits `hasGestureMode`; Armoury Crate never calls function 53 for this model.
- Generic SDK defines `GESTURE_MODE` as a gesture assignment config (`both: 0, left: 1, right: 2`),
  not a gesture execution command.

### 2. Complete Bluetooth comparison (Completed)

Resolved statically:
- Entry points identified: `SetFunction` at `0x180083240`, `GetFunction` at `0x180083740`.
- Complete side-by-side table documented above.
- Function 60, 61, 209 are `E_NOTIMPL` on both USB and BT.
- `R2Clib64.dll` contains only Realtek DSP / I2C / UVC functions, not R55ES transports.
- The inspected `C_R55ES_Protocol` request inventory contains no confirmed
  microphone state getter; this is a finding about the inspected HAL version.

### 3. Capture a controlled physical toggle at USB level (Completed)

Executed live capture session (trace SHA-256 `c24e60240c1af3465c2bfedc0763cfb7c8a491d2200ead4ceb797f51477082c8`):
1. Call context was established (`05 31` sent and acknowledged by `05 01`).
2. Earbud tap 1 at `21:12:07.936399` audibly announced `microphone off`: emitted `cc 70 00 00 00 01 01`.
3. Earbud tap 2 at `21:12:14.156970` audibly announced `microphone on`: emitted `cc 70 00 00 00 01 01`.
4. Result: `cc 70` is verified as an identical 64-byte edge event for both directions.
5. No companion Consumer, Telephony, or Audio Class report accompanied either tap.
6. Conclusion: The headset firmware signals gesture edge events, but maintains internal
   mute state autonomously. Software cannot infer absolute state reliably without risk
   of desynchronization.

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
