# Modern firmware baseline — 2026-09-16

## Pins and rollback snapshot

| Component | Old | New |
| --- | --- | --- |
| Config | `1fa9653`, tag `zmk-v0.3-known-good` | branch `codex/zmk-main-baseline` |
| ZMK | v0.3: `edf5c0814fd3ea202e43aad2d68fd32e882a518c` | `9ebbeff0a8b69a42f14aec022cdf16c7a107b9e0` |
| Zephyr | 3.5.0, `dacab4875df72109b96cc8977547a0dc04875bcd` in the fresh rollback build | 4.1.0, `10ba6d0cb38bc3d258775d27982f707599320085` |
| Board | `nice_nano_v2` | `nice_nano@2.0.0//zmk` |

`git ls-remote` verified main HEAD, and recent upstream issues/PRs were reviewed
before choosing it. The chosen ZMK commit is dated September 14. ZMK's own manifest
still follows `v4.1.0+zmk-fixes`, so the user manifest also pins Zephyr. Imported
module revisions are SHAs; each build exports a resolved `west-frozen.yml`.
The build container and checkout/upload Actions are pinned by digest/SHA.

The old tag preserves the exact original tree and original v0.3 workflow. Its
original May build artifacts have expired, so the rollback files are a **fresh
rebuild of that source**, not a dump of the firmware/settings on the keyboard.
The September rollback build confirms the old Zephyr revision above.

## Existing configuration and migration scope

There were only two build entries, no settings-reset entry, no external modules,
custom widgets, overlays or custom behaviors. Both used the upstream Corne,
nice_view_adapter and nice_view shields. The left additionally used
`studio-rpc-usb-uart`; the right is a split peripheral. All are preserved.

| Area | Preserved behavior / deliberate change |
| --- | --- |
| Keymap | File unchanged, including four layers and three Studio-reserved layers. |
| Name | Upstream Corne shield still resolves to `Corne`. |
| Studio | Enabled on both, RPC on central, USB snippet on left, locking disabled as before. |
| BLE power | `CONFIG_BT_CTLR_TX_PWR_PLUS_8=y`; resolved power is 8 dBm on both. |
| Debounce | Remove global press=1/release=10; Kconfig becomes -1/-1, selecting DTS 5/5 ms. |
| Host preferences | Unchanged defaults: interval min/max 6/12 (7.5/15 ms), latency 30, timeout 400 (4 s). |
| Split preferences | Unchanged defaults: interval 6 (7.5 ms), latency 30, timeout 400 (4 s). |
| Power | Deep sleep remains disabled; no new power management, advertising or idle tuning. |
| Displays | Upstream nice!view status screens retained, including WPM/layers/profiles and local battery. |
| Battery | Upstream local reporting retained; split battery fetching/proxy remain disabled. |
| Diagnostics | Separate opt-in snippet/module; no logging or observer code in normal images. |

The modern [HWMv2 board and ZMK variant](https://zmk.dev/blog/2025/12/09/zephyr-4-1)
preserve settings/NVS, bootloader support and battery/ext-power wiring. The explicit
2.0.0 revision is verified in
[the pinned board definition](https://github.com/zmkfirmware/zmk/blob/9ebbeff0a8b69a42f14aec022cdf16c7a107b9e0/app/module/boards/nicekeyboards/nice_nano/board.yml).
Using the bare `nice_nano` target would omit ZMK-specific settings and is incorrect.

Upstream changes necessarily accompany the stack migration: LVGL/nice!view APIs,
larger display buffers and a 4096-byte display thread stack, serial VCOM inversion,
and a lower-priority VCOM thread. The normal display thread remains priority 5.
No custom display replacement was added. Endpoint code now has an explicit `NONE`
state and migrates the old saved transport preference into `endpoints/preferred2`.
This is upstream behavior, not a local bond/profile change.

## Battery audit

The ZMK files `app/module/drivers/sensor/battery/battery_nrf_vddh.c`,
`battery_common.c` and `app/src/battery.c` have **no diff** between v0.3 and the pin.
Both V2 board definitions select `zmk,battery-nrf-vddh`. The driver still uses:

- Internal VDDH/5 ADC channel; 12-bit resolution; internal reference, gain 1/2;
  acquisition time 40 microseconds.
- `oversampling=4`, meaning **16 samples**, not four samples.
- Calibration requested on the first read, then disabled on later reads.
- The same raw-to-millivolt conversion, multiplied by five.
- The same crude SOC approximation: 0% at/below 3450 mV, 100% at/above 4200 mV,
  otherwise integer `mV * 2 / 15 - 459`.
- Reporting interval 60 seconds while active, with sampling stopped when idle.

Sources: [VDDH driver](https://github.com/zmkfirmware/zmk/blob/9ebbeff0a8b69a42f14aec022cdf16c7a107b9e0/app/module/drivers/sensor/battery/battery_nrf_vddh.c),
[SOC conversion](https://github.com/zmkfirmware/zmk/blob/9ebbeff0a8b69a42f14aec022cdf16c7a107b9e0/app/module/drivers/sensor/battery/battery_common.c).

The underlying Zephyr nrfx SAADC implementation **did** change between 3.5 and 4.1:
newer HAL support, acquisition-time handling, runtime-PM paths, and correction of
negative single-ended samples to zero. The configured 12-bit, 40-us, 16x VDDH
measurement remains supported with the same meaning. There is no evidence here
of a new battery calibration or more accurate LiPo model. See the
[3.5 driver](https://github.com/zmkfirmware/zephyr/blob/dacab4875df72109b96cc8977547a0dc04875bcd/drivers/adc/adc_nrfx_saadc.c)
and [4.1 driver](https://github.com/zmkfirmware/zephyr/blob/10ba6d0cb38bc3d258775d27982f707599320085/drivers/adc/adc_nrfx_saadc.c).

The split BAS proxy gained an attribute user-data pointer fix
([#3105](https://github.com/zmkfirmware/zmk/pull/3105)); its percent-only protocol
did not become a raw-voltage transport. This configuration does not enable that
proxy. Each display shows its own half's local battery. Diagnostic ADC logs on
each USB port expose raw ADC, mV and SOC; `corne status` shows cached local mV/SOC.
No extra sampling, constants or permanent UI changes were introduced.

USB diagnostic capture powers/charges the board, so its voltage and radio behavior
are not a controlled battery-only measurement. A high SOC estimate alone does not
prove battery health or exclude voltage sag. Compare unplugged behavior separately;
do not calibrate the percentage from USB-connected readings.

## Fixes gained and known limitations

- **Controller prepare-pipeline overflow fix:** Zephyr `10ba6d0cb38b`, the pin itself.
  Confirmed relevant to nRF52840 split central lockups; the reporter confirmed
  improvement in [#3480](https://github.com/zmkfirmware/zmk/issues/3480).
- **Peripheral assertion during connection update plus flash operations:**
  Zephyr `b2aee46d1685`, included in the pin; see
  [commit](https://github.com/zmkfirmware/zephyr/commit/b2aee46d168593c7fb37edb1b663808f2cb851cd).
- **Correct discovery buffer setting:** ZMK [#3216](https://github.com/zmkfirmware/zmk/pull/3216)
  uses `BT_ATT_TX_COUNT=10` on the central, replacing the obsolete L2CAP-level
  override. This is an upstream buffer fix; split interval/latency/timeout do not change.
- **nice!view SPI release ordering:** included Zephyr `9df4b12b5af3` fixes releasing
  the SPI bus too early around VCOM activity.
- ZMK [#3204](https://github.com/zmkfirmware/zmk/pull/3204) releases pointing-device
  input state on peripheral disconnect; it is not a general keyboard release fix.

These remain test considerations, not reasons to invent a local fix:

| Upstream report | Relevance and decision |
| --- | --- |
| [#3195](https://github.com/zmkfirmware/zmk/issues/3195): Studio + deep sleep wake failure, exact Corne/nice!view/nano setup | Open. This baseline keeps deep sleep disabled, as before. Do not enable it as part of this experiment. |
| [#3459](https://github.com/zmkfirmware/zmk/issues/3459): display/BLE thread priority contention | Open, reported on a downstream animated I2C OLED, not reproduced on stock nice!view. Preserve upstream priorities and watch for bursts. |
| [#3156](https://github.com/zmkfirmware/zmk/issues/3156), [#3411](https://github.com/zmkfirmware/zmk/pull/3411): concurrent split discovery | #3156 closed; follow-up PR still open, not included. Report used two peripherals with battery fetching. This setup has one and fetching disabled. |
| [#3309](https://github.com/zmkfirmware/zmk/issues/3309): BlueZ resume/GATT error storm | Open, reported on v0.3 with another board and additional features. Modern stack is not proof it is solved. Test host sleep/reconnect. |
| [#3448](https://github.com/zmkfirmware/zmk/pull/3448): split full-state convergence/retry | Closed **unmerged**. Pin still lacks that proposed retry/heartbeat behavior. Trace notification failures and missing releases. |
| [#3447](https://github.com/zmkfirmware/zmk/pull/3447), [#3482](https://github.com/zmkfirmware/zmk/pull/3482) | Closed **unmerged**; do not count their battery-event/RPA proposals as gained fixes. |
| [#2938](https://github.com/zmkfirmware/zmk/pull/2938), [#3458](https://github.com/zmkfirmware/zmk/pull/3458) | Battery-reporting proposals still open, not included. |

No reviewed report established an unresolved HEAD-specific blocker requiring an
older pin for this exact configuration. Build success is not a hardware stability
certification. No dual-boot bonding changes or experimental PRs were applied.

## Build verification and warning review

Files changed: `config/west.yml`, `build.yaml`, `config/corne.conf`,
`.github/workflows/build.yml`, `README.md`, and the historical-note banner in
`ble_investigation.md`. Added `build-diagnostic.yaml`, `.gitignore`,
`zephyr/module.yml`, root `Kconfig`/`CMakeLists.txt`, `src/diagnostics.c`,
`snippets/corne-diagnostics/{snippet.yml,corne-diagnostics.conf}`,
`scripts/build.py`, and the three guides under `docs/`. The keymap is unchanged.

The build wrapper checks resolved V2/ZMK compatibility, +8 dBm, debounce and DTS,
roles, Studio, nice!view, NVS, split preferences, and absence/presence of diagnostic
link wrappers. Firmware plus evidence is archived separately per target.

Initial supported upstream-workflow build:
[35175222218](https://github.com/alish2001/corne/actions/runs/35175222218), all three
normal/reset targets passed. Fresh v0.3 rollback build:
[35175223663](https://github.com/alish2001/corne/actions/runs/35175223663), both passed.
Final five-target build results are recorded in [build-results.md](build-results.md).

Warnings are retained and reviewed rather than globally suppressed:

- `KSCAN` is deprecated by Zephyr; ZMK's stock Corne still uses that driver.
  Migrating to a different input driver would change this experiment's scope.
- Right-side `ZMK_USB=y` board default resolves to `n` because a split peripheral
  does not provide USB HID. Diagnostic CDC logging is independent of USB HID.
- nice!view's status switch lacks `ZMK_TRANSPORT_NONE`. Its text buffer is
  initialized empty, so disconnected/no-endpoint status can show a blank output
  icon; this is a known upstream UI limitation, not an uninitialized buffer.
- Stock settings-reset uses a zero-row mock matrix and produces empty-keymap
  initializer/array-bounds warnings. It resets settings during startup; it is
  not a typing image. No reset-driver workaround is applied.

## First test and A/B record

1. Save/download the rollback binaries. Record OS/BlueZ/kernel, adapter, OS repeat
   delay/rate, radio placement, charge condition and any Studio edits. Keep these
   constant across versions. Test on one OS with a clean selected host profile.
2. Run a short v0.3 session unplugged: left only, right only, rapid alternating
   hands, and fast typing. Hold then release keys on each half to exercise OS
   repeat; distinguish expected held-key repeat from repeat after physical release.
3. Flash **both** modern normal halves, preserve the existing keymap settings if
   desired, and reconnect using a clean host bond. First try without a full reset.
   If a reset is needed, save Studio changes first, reset both halves, then flash
   both normal images and pair the halves before the host.
4. Repeat the same typing and held-release tests, then several idle/wake and host
   suspend/resume cycles. Deep keyboard sleep is disabled; test idle and host sleep
   without silently enabling `ZMK_SLEEP`.
5. Power off/on the right half with no keys held; verify recovery. Then, in an
   empty test editor, test right-half loss while a key is held and observe whether
   the host releases it or repeats. Note the recovery action needed.
6. Repeat at full charge and after substantial normal discharge; record each
   half separately. Use Linux/BlueZ first and optionally repeat on macOS with its
   repeat settings recorded. Do not switch OS/bonds during a run.
7. If a repeat appears, record side/key, release time, charge/USB state, and whether
   the other half still works. Use both diagnostic images and the release-tracing
   guide to locate the first missing stage. Return to normal firmware afterward.

Record exposure time, repeat-after-release count, right-side stalls, reconnect
failures and recovery required. A short symptom-free session does not establish
that an intermittent fault is fixed. The initial A/B changes both the stack and
debounce by request; it cannot attribute an improvement uniquely to Bluetooth.

## Roll back

Flash both UF2s from the fresh v0.3 rollback archive. To return the source tree
without rewriting the migration branch:

```sh
git switch -c rollback-v0.3 zmk-v0.3-known-good
# Rebuild the old workflow, if needed:
gh workflow run build.yml --ref zmk-v0.3-known-good
```

The tag retains the original workflow, `nice_nano_v2` board names, keymap, +8 dBm
and 1/10 ms debounce. Restore saved Studio settings separately. A full settings
reset is optional if cross-version settings/bonds prevent recovery; it erases
those settings and requires pairing again. Upstream endpoint preference migration
is one-way, so the old firmware may return to its default USB preference.
