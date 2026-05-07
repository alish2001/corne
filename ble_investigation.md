# BLE Latency Investigation

Working doc for measuring and reducing Bluetooth latency on this Corne (nice!nano v2 split, aluminum case, macOS host).

## Current config audit (`config/corne.conf`)

| Setting | Value | Status |
|---|---|---|
| `CONFIG_BT_CTLR_TX_PWR_PLUS_8` | `y` | ✅ Maxed (+8 dBm). Critical for aluminum case. |
| `CONFIG_ZMK_KSCAN_DEBOUNCE_PRESS_MS` | `1` | ✅ Eager press detect. |
| `CONFIG_ZMK_KSCAN_DEBOUNCE_RELEASE_MS` | `10` | ✅ Conservative release. |
| `CONFIG_ZMK_STUDIO` | `y` | ⚠️ Extra GATT service alive — small latency tax. |
| `CONFIG_ZMK_USB_LOGGING` | off | ✅ Correct for daily use. |
| ZMK version (`west.yml:8`) | `v0.3` | ✅ Latest tagged release (cut 2025-08-01). Pinning is the recommended pattern per ZMK's June 2025 blog. |
| Connection interval prefs | unset | ❌ Only real BLE-side gap. |

**Verdict:** TX power and debounce are correct. The connection-interval block is the single missing knob worth testing — this is exactly the area where the aluminum case's physical attenuation hurts most.

## Why the aluminum case matters

Aluminum doesn't *block* 2.4 GHz, it attenuates and reflects. Two consequences:

1. **TX power is already maxed** — no firmware lever left for raw signal strength. Anything beyond is physical (antenna cutout placement near the controller's USB-C edge).
2. **Split inter-half BLE is the more fragile link.** Symptom would be right-hand keys lagging or dropping while left feels fine.

## Proposed change

Add to `config/corne.conf`:

```ini
# BLE connection interval — request faster, allow auto-renegotiation
CONFIG_BT_PERIPHERAL_PREF_MIN_INT=6     # 7.5 ms (Linux/Windows may honor; macOS likely clamps to 15ms)
CONFIG_BT_PERIPHERAL_PREF_MAX_INT=12    # 15 ms
CONFIG_BT_PERIPHERAL_PREF_LATENCY=30    # peripheral may skip up to 30 intervals when idle (battery save)
CONFIG_BT_PERIPHERAL_PREF_TIMEOUT=400   # 4s supervision timeout
CONFIG_BT_GAP_AUTO_UPDATE_CONN_PARAMS=y
```

**Note:** `LATENCY=30` is the battery-friendly choice. `LATENCY=0` would force the radio awake at every interval — best worst-case latency but ~3–4× battery drain. Test with `30` first; only drop to `0` if the idle-wake delta is felt.

## Downsides of the proposed change

1. **Battery life** — fastest interval × `LATENCY=0` would cut battery from ~2–3 months to ~1 month per half. `LATENCY=30` mostly mitigates this.
2. **macOS may ignore the request** — Apple often clamps to 15 ms. Worst case = no improvement (no harm).
3. **Stability in noisy 2.4 GHz** — tighter intervals = more collision opportunities with WiFi/other BLE. Aluminum case + interference + aggressive intervals could marginally raise dropped-packet rate. Supervision timeout of 4 s gives plenty of recovery room.
4. **Affects only half→host link** — split inter-half BLE has its own params (`CONFIG_ZMK_SPLIT_BLE_*`). Won't help if right-hand-only lag is the symptom.
5. **Renegotiation churn** — `AUTO_UPDATE_CONN_PARAMS=y` may briefly disconnect/reconnect on hosts with old BT stacks. macOS handles this fine.

## Measurement methodology

Three approaches, easy → precise. Run **before** applying the change to capture a baseline; run again **after**.

### A. USB vs BLE A/B feel test (quick, no tools)

Plug left half into Mac via USB-C (auto-switches transport). Type the same passage in both modes. Anything you feel as a difference is BLE pipeline overhead.

To force-toggle from a key, add to keymap (`#include <dt-bindings/zmk/outputs.h>` at top):
```dts
&out OUT_TOG     // toggle USB ↔ BLE
&out OUT_USB     // force USB
&out OUT_BLE     // force BLE
```

### B. End-to-end with a phone (gold standard for "feel")

iPhone slow-mo at 240 fps = ~4.2 ms/frame.

- App: [Is It Snappy](https://isitsnappy.com) (iOS), purpose-built for this.
- Aim phone at keyboard + screen. Hit a key in Notes. Count frames from contact → glyph appears.
- 10 reps, average. Test BLE first, then USB-C plugged in.
- Expect ~25–35 ms BLE, ~10–15 ms USB on a clean macOS setup.

### C. Pure BLE radio latency (firmware → Mac HID)

This isolates the BLE pipeline specifically. The left half exposes a USB CDC console even on BLE transport (the `studio-rpc-usb-uart` snippet in `build.yaml:17` enables this).

**Firmware side** — temporarily add to `config/corne.conf`:
```ini
CONFIG_ZMK_USB_LOGGING=y
CONFIG_ZMK_LOG_LEVEL_DBG=y
```
Reflash left half. Plug it in via USB *but keep BLE active* (`&out OUT_BLE`). Then:
```bash
brew install tio
tio /dev/tty.usbmodem*    # find with: ls /dev/tty.usbmodem*
```
You'll see `position_state_changed` with kernel timestamps as the firmware emits each HID report.

**Mac side** — Hammerspoon timestamps every HID event the OS receives:
```bash
brew install --cask hammerspoon
```
`~/.hammerspoon/init.lua`:
```lua
local logger = hs.logger.new('keylat', 'info')
hs.eventtap.new({hs.eventtap.event.types.keyDown}, function(e)
    local k = hs.keycodes.map[e:getKeyCode()] or e:getKeyCode()
    logger.f("%.3f keyDown %s", hs.timer.absoluteTime() / 1e9, k)
    return false
end):start()
```
Reload Hammerspoon, tail `~/.hammerspoon/hammerspoon.log` while watching `tio`. Delta between firmware-emit and Mac-receive timestamps = raw BLE latency. Average over 20 keypresses.

**Disable `CONFIG_ZMK_USB_LOGGING` when done** — burns battery.

## Test protocol

1. Flash both halves with **current** config. Record measurements (Method B at minimum, ideally B + C).
2. Apply conn-interval block. Reflash both halves.
3. **Re-pair** (host clears cached connection params on re-pair, otherwise you may keep the old negotiated interval). On Mac: forget device → re-pair via raise/numpad layer's `&bt BT_SEL N`.
4. Re-measure with same methodology.

## Results

### Baseline (before conn-interval changes)

- Date:
- Method B (camera) BLE avg: _ms over _ presses
- Method B (camera) USB avg: _ms over _ presses
- Method C (firmware↔Mac) avg: _ms over _ presses
- Subjective notes:

### After conn-interval changes (LATENCY=30)

- Date:
- Method B (camera) BLE avg:
- Method C (firmware↔Mac) avg:
- Subjective notes:
- Battery impact observation (after 1 week):

### Optional: LATENCY=0 retest

- Method B (camera) BLE avg:
- Method C (firmware↔Mac) avg:
- Subjective notes:

## Reference

- ZMK split BLE adds ~3.75 ms avg / ~7.5 ms worst-case ([docs](https://github.com/zmkfirmware/zmk/blob/main/docs/docs/features/split-keyboards.md))
- ZMK pinning blog: https://zmk.dev/blog/2025/06/20/pinned-zmk
- ZMK system config: https://github.com/zmkfirmware/zmk/blob/main/docs/docs/config/system.md
