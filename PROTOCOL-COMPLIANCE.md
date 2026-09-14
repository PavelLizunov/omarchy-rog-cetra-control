# Production protocol cross-check

Reviewed 2026-09-14 against RESEARCH.md and current daemon/protocol.h,
daemon/commands.h, daemon/reports.h and cetra-watch.c. This is source review,
not new reverse engineering. All HID I/O remains in one elected cetra-watch.

| Operation | Builder / caller | Wire command and domain | Research evidence / boundary |
| --- | --- | --- | --- |
| Battery query | send_request; startup and alternating poll | 00 cc 12 07, 17 bytes | Battery section; response percentages 0..100, ff unavailable |
| Mode query | send_request; startup, poll, bounded retry, left double tap | 00 cc 12 25 | Noise control; retry reads only |
| Presence query | send_request; startup, 10-second schedule | 00 cc 12 01 | HAL re-verification; masks 00/01/10/11 |
| ANC level read | send_request; round-robin / explicit write readback | 00 cc 12 2b | ANC level, values 1..3 |
| Adaptive read | send_request; round-robin / explicit write readback | 00 cc 12 2c | Adaptive, boolean domain |
| Voice read | send_request; round-robin / explicit write readback | 00 cc 12 28 | Voice prompt, 0/1/2 |
| Proximity read | send_request; round-robin / explicit IPC readback | 00 cc 12 26 | Proximity setting only, not playback behavior |
| Set mode | set_mode via validated mode IPC | cc 41 08, 64 bytes, 0/1/2 | Noise control; actual hardware trials |
| Set level | set_anc_level via validated IPC | cc 41 0c, 64 bytes, 1..3 | ANC level; actual hardware trials |
| Set adaptive | set_anc_adaptive via boolean IPC | cc 41 0d, 64 bytes, 0/1 | Adaptive; actual hardware trials |
| Set voice | set_voice_prompt via enum IPC | cc 41 0a, 64 bytes, 0/1/2 | Voice prompt; actual hardware trials |
| Set proximity | set_proximity, research IPC only | cc 41 09, 64 bytes, 0/1 | Known setting; UI removed; USB auto-pause unverified |
| Lighting | set_lighting; explicit IPC and session replay | cc 51 28 zone 1 then 0; cc 50 55 twice | RGB domains 0..255; effects 1..4; runtime Off = Static black. Exact official Off/duplicate necessity remains unverified |
| Call context | set_call_context; aggregate intent/start/reconnect/shutdown | 05 31 or 05 00, two bytes | Requested context, not absolute native mute or guaranteed gesture mapping |

Charging 12 08 is decoded passively; no new charging query was introduced.
Unknown reports are bounded diagnostic observations. A tap increments observed
sequence only and never drives mute parity or a synthesized media action.
No runtime sidetone, EQ, function-53 or software-mute opcode was added.

The settings and peak helpers never link or call HID APIs. UI controls do not
create hardware owners. Auto-theme is explicit opt-in and session-authorized;
successful lighting preference replay is scoped to the existing owner session.
Protocol failures cannot authorize speculative retry writes.

Remaining hardware limitations are retained in RESEARCH.md and BACKLOG.md.
This cross-check does not turn them into verified capabilities.
