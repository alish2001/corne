# Windows pairing investigation — 2026-09-25

Windows shows Corne but rejects pairing with “Try connecting your device again.”
The user cleared the Omarchy slot on the same dual-boot Bluetooth adapter and
successfully paired an iPhone using the modern firmware. The Windows adapter is
identified as Intel Wireless Bluetooth; exact model/driver and Windows version
are not yet known. The user reports that the previous firmware paired with this
same Windows PC, and that Device Manager shows no hidden Corne entry after normal
removal. A Windows-specific firmware compatibility regression is therefore a
credible hypothesis; successful iPhone pairing does not rule it out. No successful
Windows pairing or confirmed cause has yet been observed for the new build.
The motherboard is reported as an ASUS ROG Crosshair X670-series board. The
experiment tests interoperability, not whether this hardware is powerful enough.

## Separate the icon from the pairing failure

At the pinned ZMK revision, an unconnected selected profile and no ready USB HID
endpoint produce `ZMK_TRANSPORT_NONE`. The nice!view output-status switch handles
only USB and BLE, leaving its initialized text empty for NONE. This explains the
blank icon; it is not evidence that advertising or pairing has been disabled.
The icon is left unchanged in this experiment to isolate the radio change.

## Optional 1M PHY test

ZMK's [connection troubleshooting guide](https://zmk.dev/docs/troubleshooting/connection-issues#additional-bluetooth-options)
specifically lists disabling 2M PHY as a workaround for some Windows Intel/Realtek
adapter firmware versions. This is a hypothesis to test, not a confirmed fix.

`build-windows.yaml` adds two **left/central-only** images:

- `corne-left-windows-1m.uf2`: normal baseline plus `CONFIG_BT_CTLR_PHY_2M=n`.
- `corne-left-windows-1m-diagnostic.uf2`: diagnostic baseline plus that same setting.

ZMK/Zephyr pins, keymap, Studio, display, 5/5 ms debounce, +8 dBm, connection
intervals, peripheral latency and supervision timeout preferences remain unchanged.
Pairing security and bond/profile handling are not modified. The old v0.3 build
also had 2M enabled; if this workaround helps, it does not establish that simply
enabling 2M was the regression. Stack/adapter interactions may have changed.

This controller-wide switch affects **both** host and split links of the central.
The existing modern right image can remain installed: the peers negotiate the
mandatory 1M PHY. No right reflash or full settings reset is required for this test.
The five baseline targets in `build.yaml`/`build-diagnostic.yaml` stay unchanged;
the workflow's `all` mode still builds only those five.

Build explicitly:

```sh
gh workflow run build.yml --ref codex/windows-ble-compat -f mode=windows
# Or use the existing pinned local west workspace:
python3 scripts/build.py corne-left-windows-1m --workspace /path/to/west
```

Both images passed [Actions run 36210445821](https://github.com/alish2001/corne/actions/runs/36210445821)
at firmware-input commit `9569594`. Compared with the corresponding baseline
normal/diagnostic artifacts, the **only resolved Kconfig change** is
`CONFIG_BT_CTLR_PHY_2M=y` to `n`. All 56 frozen dependency revisions and the keymap
hash match the baseline. UF2 block structure, nRF52840 family, application flash
address range and SHA256 checksums were verified. The known KSCAN and nice!view
NONE-state warnings remain; there are no new compiler warnings.

| Image | Flash | RAM | SHA256 |
| --- | --- | --- | --- |
| Normal compatibility | 425492 B | 105930 B | `2f65265198b6f864e917a71d5cab9e0a1900454e7578e0fd9927992f347d060b` |
| Diagnostic compatibility | 521016 B | 152754 B | `22efbbcd746b7fe012ed1e10d647b0ab2e14a975a8c6c28f7a4bc7eded379a47` |

These are build/inspection results. Windows pairing on the user's PC still needs
to be tested; a successful build does not establish a pairing fix.

## Controlled test

1. Keep the right half on the modern firmware and save the original
   `corne-left-normal.uf2` for rollback.
2. Flash only `corne-left-windows-1m.uf2` to the left.
3. Use the same intended Windows profile and pairing procedure as the failed
   attempt. If removing an entry, remove the Corne pairing on Windows and clear
   that selected keyboard profile with `BT_CLR`; do not erase unrelated profiles.
   In the checked-in keymap, hold the right middle thumb (Raise), use A/S/D/F/G to
   select displayed slots 1–5, and tap the physical Tab position (left home-row
   outer key) for `BT_CLR`. Studio edits can override these bindings.
4. Try pairing from Windows. Note whether rejection is immediate, whether a PIN
   appears, or whether it connects then disconnects. If it succeeds, test typing
   on both halves, then reconnect after a Windows sleep/wake cycle.
5. If necessary, compare with the original modern left normal image using the
   same clearing procedure. Reflashing itself does not erase bonds. An equal
   procedure on both builds is needed before attributing a change to PHY.

Do not flash `settings_reset` as the first response: it deletes other host bonds,
split settings and Studio edits. The [dual-boot limitation](https://zmk.dev/docs/troubleshooting/connection-issues#issues-with-dual-boot-setups)
still applies: separate slots do not give the same physical adapter independent
Windows/Linux identities.

## If pairing still fails

Use the **original** `corne-left-diagnostic.uf2` to capture the baseline failure,
or the compatibility diagnostic image to inspect the experiment. The left USB
console can connect to the Mac while Windows attempts Bluetooth pairing. The
right can remain on its matching modern normal firmware.

Enable runtime logs (the older `kernel` command exists in these images):

```text
kernel log_level zmk 4
kernel log_level corne_diag 3
corne status
```

Capture one failed pairing, including `CONNECT`, `SECURITY`, `PAIRING_FAILED`,
`DISCONNECT`, and any `Rejecting pairing request to taken profile` lines. Those
distinguish occupied/stale bonds, authentication requirements and link failures.
Passkey/security modes are not enabled together with the PHY test; they are a
separate possible experiment if the failure evidence warrants it.
