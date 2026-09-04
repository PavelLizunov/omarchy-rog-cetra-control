# ROG Cetra Control — Complete Project Handbook & AI Handoff

> Comprehensive engineering guide, architectural blueprint, reverse-engineered hardware nuances, operational procedures, known issues, and roadmap for AI agents and developers.

---

## 1. Executive Summary & Purpose

**ROG Cetra Control** (`io.github.pavellizunov.rog-cetra-control`) is a production-grade Omarchy Quattro (Quickshell) plugin and native Linux system service for the **ASUS ROG Cetra True Wireless SpeedNova** gaming headset operating over its 2.4 GHz USB-C receiver (`0b05:1ad3`).

Under Linux, vendor gaming headsets typically operate only as generic USB Audio devices with no battery monitoring, no ANC control, and no access to onboard DSP features. This project provides full reverse-engineered hardware control directly over USB HID:
- Real-time battery monitoring (Left earbud, Right earbud, Charging case) with transient-loss debounce.
- Full Noise Control (Off, Active Noise Cancelling, Ambient Sound transparency).
- Multi-level ANC intensity (Low, Mid, High) and Smart Adaptive ANC switching.
- Aura RGB Lighting control (Off, Color Cycle, Static, Breathing, Strobing) with desktop theme synchronization.
- Voice prompt language selection (English, Chinese, Beeps).
- In-Ear proximity detection (Auto-pause/resume).
- Real-time hardware microphone mute synchronization with physical earbud tap gestures and PipeWire communication detection.

---

## 2. Hardware Architecture & Protocol Specifications

### Supported Device Identifiers
- **USB Vendor ID (VID):** `0x0b05` (ASUSTek Computer, Inc.)
- **USB Product ID (PID):** `0x1ad3` (ROG CETRA TRUE WIRELESS SPEEDNOVA)
- **Primary HID Interface:** Interface `3` (`/dev/hidrawX`)
- **USB Path:** Typically `3-1:1.3`
- **Report IDs:** Vendor commands use `0xcc`, Telephony uses `0x05`, Consumer Control uses `0x0c`.

### Core HID Communication Protocol

All host-initiated vendor requests are sent to Interface 3 as 17-byte HID Output Reports (leading `0x00` report selector + 16 payload bytes):
```text
00 cc 12 <COMMAND_ID> 00 00 00 00 00 00 00 00 00 00 00 00 00
```
All write control commands are sent as 64-byte HID Output Reports:
```text
cc <OPCODE> <SUB_OPCODE> 00 00 <VALUES...>
```

#### Protocol Registry Table

| Function | Request Opcode | Write Command (64-byte) | Response Payload Layout | Verified Semantics |
|---|---|---|---|---|
| **Battery Status** | `cc 12 07` | — | `byte 5`: mask (`0x05`)<br>`byte 6`: Left (0–100 or 255)<br>`byte 7`: Right (0–100 or 255)<br>`byte 8`: Case (0–100 or 255) | Value `255` (`0xff`) indicates component is in case or off. Debounced to 2 sec. Unsolicited status arrives as `cc 12 09`. |
| **Noise Control** | `cc 12 25` | `cc 41 08 00 00 <MODE>` | `byte 5`: Active mode | `0`: Off<br>`1`: ANC<br>`2`: Ambient Transparency |
| **ANC Level** | `cc 12 2b` | `cc 41 0c 00 00 <LVL>` | `byte 5`: Active level | `1`: Low<br>`2`: Mid<br>`3`: High |
| **Adaptive ANC** | `cc 12 2c` | `cc 41 0d 00 00 <0\|1>` | `byte 5`: Active state | `0`: Disabled (Manual)<br>`1`: Enabled (Smart / Adaptive) |
| **Aura Lighting** | — | 1: `cc 51 28 00 00 <Z> <EFF> <R> <G> <B>`<br>2: `cc 50 55 00 ...` (Commit) | — | Zones: `0` (Left), `1` (Right)<br>Effects: `0`: Off (`00 00 00`), `1`: Static, `2`: Breathing, `3`: Strobing, `4`: Cycle |
| **Voice Prompt** | `cc 12 28` | `cc 41 0a 00 00 <VAL>` | `byte 5`: Active prompt | `0`: Prompt Sound (Beeps)<br>`1`: English Voice<br>`2`: Chinese Voice |
| **In-Ear Detect** | `cc 12 26` | `cc 41 09 00 00 <0\|1>` | `byte 5`: Active state | `0`: Disabled<br>`1`: Enabled (Auto-Pause) |
| **Call Context** | — | `05 31` (Call)<br>`05 00` (Media) | — | Activates native headset telephony controls. Right earbud tap becomes mute toggle. |

---

## 3. The Physical Microphone Nuance (Crucial Knowledge)

### The Hardware Reality
1. **Audio Chip Autonomy:** The microphone hardware mute is managed by the internal DSP inside the right earbud. When the user single-taps the right earbud during an active call context (`05 31`), the earbud DSP cuts power/ADC to the microphone capsules and plays the native voice prompt (*"Microphone off"* or *"Microphone on"*).
2. **Zero Host Write Equivalent:** There is **no HID write command** in existence that forces the earbud to play the voice prompt or physically toggle internal microphone mute from the host. Official ASUS HAL 1.3.95.0 (`AacAudioHal_x64.dll`) and G-Helper source code have been decompiled and confirmed: functions `60` (`MIC_VOLUME`), `61` (`MIC_MUTE_INDICATOR`), and `209` (`MUTE_STATE`) return `E_NOTIMPL`.
3. **Hardware ADC Cutout:**
   - When muted (*Microphone off*): the earbud ADC transmits **100.0% mathematical zeroes (`0x0000`)** in the PCM stream (RMS = 0.00, Peak = 0).
   - When live (*Microphone on*): pre-amp analog thermal noise is always present (< 2% zeroes, RMS > 5.0).
4. **Gesture Packet Breakdown (`cc 70`):**
   ```text
   cc 70 00 00 00 <BYTE 5> <BYTE 6> <BYTE 7> ...
   ```
   - `BYTE 5`: Earbud (`0x00` = Left, `0x01` = Right).
   - `BYTE 6`: Gesture (`0x01` = Single tap, `0x02` = Double tap, `0x03` = Triple tap, `0x00` = Long press).
   - `BYTE 7`: Sub-gesture (`0x01` for long press).
5. **Contextual Dual-Role:**
   - In Media Mode (`05 00`): A single-tap on either earbud emits standard USB Consumer Control `[0c 08]` (`KEY_PLAYPAUSE`), pausing/resuming media. The microphone is **not** muted and no voice prompt plays.
   - In Call Mode (`05 31`): A single-tap on the right earbud toggles the hardware microphone with voice prompts.
   - Left earbud double-tap (`cc 70 .. 00 02`) always toggles hardware ANC mode.

---

## 4. Software & Service Architecture

```text
┌────────────────────────────────────────────────────────┐
│                   Omarchy Shell Bar                    │
│      [ CetraIcon ] [ Microphone Icon ] [ 84% ]         │
└───────────────────────────▲────────────────────────────┘
                            │
┌───────────────────────────┴────────────────────────────┐
│                       Cetra.qml                        │
│   (KeyboardPanel, QuickSettings, PipeWire Call Filter) │
└─────────────┬────────────────────────────▲─────────────┘
              │ stdin                      │ stdout
              │ (commands)                 │ (JSON state)
┌─────────────▼────────────────────────────┴─────────────┐
│                 bin/cetra-watch (Daemon)               │
│   • Single exclusive hidraw owner                      │
│   • UNIX socket server: rog-cetra-control.sock         │
│   • Status cache: rog-cetra-control.status             │
│   • Telemetry log: rog-cetra-control.log               │
└───────────────────────────▲────────────────────────────┘
                            │ USB HID Interface 3
┌───────────────────────────▼────────────────────────────┐
│      ASUS ROG Cetra SpeedNova USB Receiver (0b05:1ad3) │
└────────────────────────────────────────────────────────┘
```

### File Hierarchy & Roles

- `manifest.json`: Schema v1 plugin manifest declaring kind `bar-widget`, entry point `Cetra.qml`, and settings schema.
- `Cetra.qml`: Main UI entry point. Manages panel geometry, theming, PipeWire call stream tracking, keyboard shortcuts, and button interactions.
- `CetraIcon.qml`: High-performance SVG icon wrapper using QtQuick MultiEffect colorization.
- `assets/cetra-symbolic.svg`: Handcrafted, pixel-aligned symbolic icon matching Cetra industrial design.
- `cetra-watch.c`: Long-lived C daemon. Opens `/dev/hidraw` on interface 3, handles polling schedule, reads interrupt packets, decodes gestures, and broadcasts JSON state over UNIX socket.
- `cetra-status.c`: Standalone CLI helper for test fixture playback and quick offline verification.
- `setup`: Automated compilation, environment dependency checks (`hidapi`, `pkgconf`, `base-devel`), and hot-reload lockscreen guard.
- `tests/run.sh`: Mandatory automated test harness.

---

## 5. Known Issues, Edge Cases & Lessons Learned

### Issue 1: Upstream Quickshell Lockscreen Crash (`omacom/omarchy#9441`)
- **Phenomenon:** When local plugin files in `~/.config/omarchy/plugins/` are modified while the Omarchy lockscreen is active or requested, Quickshell triggers a hot-reload of all plugin services, leading to a session lock assertion failure and SIGABRT.
- **Remedy:** `./setup` contains an explicit preflight `ensure_shell_unlocked` querying `omarchy-shell lock status`. It unconditionally halts with an informative error if the session is locked.

### Issue 2: Broken Pipe / SIGPIPE Termination
- **Phenomenon:** When a client monitor closes its UNIX socket abruptly, writing to the client fd can raise `SIGPIPE` and kill the daemon.
- **Remedy:** `cetra-watch.c` explicitly installs `signal(SIGPIPE, SIG_IGN)` and uses `MSG_NOSIGNAL`.

### Issue 3: False "In Case" Transitions
- **Phenomenon:** The 2.4 GHz receiver occasionally drops a single packet during frequency hopping, returning `255` for one cycle before returning valid percentages.
- **Remedy:** `cetra-watch.c` applies a 2-cycle debounce counter (`left_missing >= 2`). Transient drops are ignored; persistent removal correctly triggers `In case`.

### Issue 4: Out-of-Case Microphone Desynchronization
- **Phenomenon:** If the user muted the microphone during a call and then docked the earbuds in the case, the earbud firmware resets its internal state to Live on the next deployment.
- **Remedy:** On connection transition (`!was_connected && state.connected`), `cetra-watch` automatically re-asserts `mic_live = true` and re-applies current lighting. Furthermore, clicking the microphone card manually flips the indicator to easily re-align state if ever desynchronized.

---

## 6. Development, Testing & Verification Protocol

Before modifying code or submitting commits, execute:

```bash
# 1. Run all automated regression tests
./tests/run.sh

# 2. Validate manifest and plugin structure
omarchy plugin validate .

# 3. Verify clean Git formatting
git diff --check
```

### Telemetry Logging
Live telemetry is written with microsecond timestamps to:
```bash
tail -f ~/.local/state/omarchy/rog-cetra-control.log
```
Every tap event, battery update, mode readback, and unhandled packet is recorded for post-mortem analysis.

---

## 7. Roadmap & Unresolved Research Tasks

1. **Hardware Equalizer GUI:** The `cc 41 04 00 00 B0..B9` 10-band DSP write protocol is decoded and verified in `RESEARCH.md`. A dedicated equalizer sub-panel can be exposed in `Cetra.qml`.
2. **Bluetooth Mode Support (`0b05:1ad4` or standard BT RFCOMM):** Currently, the plugin exclusively targets the USB 2.4 GHz SpeedNova dongle (`0b05:1ad3`). Expanding to Bluetooth mode requires implementing the `AacR55ESBT` RFCOMM transport documented in `RESEARCH.md`.
3. **Per-Application Audio Profiles:** Automatic switching of equalizer and ANC presets based on active Hyprland window focus.
