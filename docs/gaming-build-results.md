# Gaming firmware build — September 26, 2026

[GitHub Actions run 36218054728](https://github.com/alish2001/corne/actions/runs/36218054728)
passed all five targets from firmware/config commit
`0dacf145aa443bb47d8140bc94bfd6d17651cf79` on `codex/gaming-layout`.
The follow-up build-results commit changes documentation only and skips CI.

Use the normal left/right pair for daily use. See
[the layout and flashing instructions](gaming-layout.md).

| Image | Build + config verification | Flash used / 792 KiB | Static RAM / 256 KiB |
| --- | --- | --- | --- |
| `corne-left-normal.uf2` | PASS | 425880 B (52.51%) | 105954 B (40.42%) |
| `corne-right-normal.uf2` | PASS | 321456 B (39.64%) | 72176 B (27.53%) |
| `corne-left-diagnostic.uf2` | PASS | 521600 B (64.32%) | 152778 B (58.28%) |
| `corne-right-diagnostic.uf2` | PASS | 391616 B (48.29%) | 120160 B (45.84%) |
| `corne-settings-reset.uf2` | PASS | 52528 B (6.48%) | 12840 B (4.90%) |

## Verification

- Source and generated devicetree retain the original four layers' bindings.
  All seven active layers have 42 bindings. Gaming preserves the right-hand
  finger positions, and Game Fn preserves WASD/Ctrl/Shift/Space.
- Compiled mode selection is Lower (1) + Raise (2) -> Modes (6), with G ->
  Gaming (4), N -> Numpad (3), and Esc -> Base (0). Other mode-menu keys are
  blocked except the two transparent layer thumbs. Both Gaming exit paths and
  the left Fn+F -> Alt binding are present.
- Reviewed pinned ZMK layer dispatch: key releases use the layer state saved at
  press time, so releasing mode-selection thumbs after `&to` reaches the
  original momentary behaviors. This is source inspection, not hardware testing.
- The complete frozen dependency manifests and container digest match the
  previously verified baseline. All resolved `CONFIG_BT_*` and
  `CONFIG_ZMK_SPLIT_*` values are unchanged, including the working PHY settings.
- Configuration differences are limited to generated devicetree feature flags
  for the added conditional layer and referenced behaviors. Normal images
  retain disabled logging. Existing debounce/role/Studio/diagnostic checks pass.
- Every downloaded artifact records the expected firmware commit and keymap
  hash. UF2 checksums, magic values, nRF52840 family ID, block counts, unique
  addresses and application partition bounds all pass.
- Compiler warnings match the known baseline categories: deprecated KSCAN,
  nice!view's missing NONE icon, the right side's inapplicable USB default, and
  the reset shield's empty mock keymap. No new warning category was introduced.

The right normal and diagnostic UF2s are byte-for-byte identical to the prior
baseline: this split's left central handles keymap interpretation. Both normal
images are included so either half can be moved back from diagnostic firmware.
The settings-reset image is also unchanged and is not needed for this update.

The keyboard has not been flashed or physically tested with this gaming build.
Saved Studio mappings may require **Restore Stock Settings** after recording any
custom edits to preserve; see the layout guide before flashing.

## Checksums

```text
63d8140df1d8072838fee6aca23ed7d7ad91d10346a23ac8d53f135539622b58  corne-left-normal.uf2
c45a25da85a6ceba5bd313c24fe92c5e0d588a5c0162e63485ffe5cd1f74c963  corne-right-normal.uf2
ae0b1bb11ef6607841724bdded85146576e1a135cc6f1571e458bbb4da19c38a  corne-left-diagnostic.uf2
ee7ef194aa6fdf24d0f5d14755d5f3be3a698fc26a6300503bc039bdbba84728  corne-right-diagnostic.uf2
f5842301f675d0bb5fec1e5acc572e61bb45389be254743458abbdeab760f85c  corne-settings-reset.uf2
```

The local daily-use package is
`artifacts/corne-gaming-firmware-2026-09-26.zip` (normal left/right, checksums and
flashing instructions). Full downloaded evidence and the verification report
are retained under `artifacts/gaming-2026-09-26/`. GitHub retains the complete
build artifacts for 90 days.
