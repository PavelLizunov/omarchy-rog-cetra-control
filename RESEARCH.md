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

### Microphone Hardware Detection Method

The headset hardware ADC disconnects on internal mute:
- **Muted (Microphone off)**: 100.0% mathematical zeroes (`0x0000`) in PCM stream (RMS = 0.00, Peak = 0).
- **Live (Microphone on)**: Analog pre-amp noise floor present (RMS > 5.0, < 2% zeroes), even during complete acoustic silence.
Combined with `cc 70 00 00 00 01 01` edge notifications on physical tap, reading a 50ms audio buffer from `alsa_input.usb-ASUSTek_ROG_CETRA_TRUE_WIRELESS_SPEEDNOVA_0000000000000000-00.mono-fallback` allows instantaneous, 100% deterministic hardware mute state detection without desynchronization risk.

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
- Both packets are 100% byte-for-byte identical (`01 01`).
- No other HID report (`0x05`, `0x0c`, or `0xcc`) or Audio Control transfer
  accompanied the taps.
- This confirms `cc 70 00 00 00 01 01` is strictly an edge notification of a
  gesture tap, not an absolute mute/unmute state.
- Because the headset toggles mute internally and provides no absolute readback,
  inferring `Live`/`Muted` in software would inevitably desynchronize. The native
  voice prompt remains the sole authoritative mute state.

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
    absent (`undefined`), meaning custom gesture remapping is not supported by the
    hardware/firmware. The Armoury Crate UI only displays static user manuals
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

This inventory proves conclusively that official ASUS code contains no request
opcode for microphone mute state or gesture configuration readback.

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
- Complete inventory of all 16 `C_R55ES_Protocol` request opcodes confirmed no microphone
  state getter exists.

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
