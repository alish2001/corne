# Verified firmware artifacts

Final [GitHub Actions run 35176314599](https://github.com/alish2001/corne/actions/runs/35176314599)
passed all five targets. Tested firmware/config commit:
`211f3b996cb62d687b383ec7af2699336b4d3b1e`. The following documentation-only commit
does not change any firmware inputs and skips CI.

| Image | Build + config verification | Flash used / 792 KiB | Static RAM / 256 KiB |
| --- | --- | --- | --- |
| `corne-left-normal.uf2` | PASS | 425652 B (52.48%) | 105930 B (40.41%) |
| `corne-right-normal.uf2` | PASS | 321456 B (39.64%) | 72176 B (27.53%) |
| `corne-settings-reset.uf2` | PASS | 52528 B (6.48%) | 12840 B (4.90%) |
| `corne-left-diagnostic.uf2` | PASS | 521176 B (64.26%) | 152754 B (58.27%) |
| `corne-right-diagnostic.uf2` | PASS | 391616 B (48.29%) | 120160 B (45.84%) |

Toolchain: Zephyr SDK **0.16.9**, GCC **12.2.0**, image
`zmkfirmware/zmk-build-arm@sha256:edb1c953438c6f720ddb79c3762f3972013b7fbbaf4fff3592fc869983e7afc5`.
All **56 active dependency revisions** in each exported manifest are exact SHAs.

Verified from generated config/header/map:

- Correct V2 ZMK variant, central/peripheral roles, `Corne` name, NVS,
  Studio/USB RPC configuration and nice!view enabled.
- `CONFIG_BT_CTLR_TX_PWR_DBM=8` on both halves.
- Global debounce values -1/-1; generated matrix binding values **5/5 ms**.
- Split 6/30/400 preferences and host 6/12/30/400 preferences unchanged.
- Normal images have `CONFIG_LOG` disabled and no diagnostic objects/wrappers.
- Normal/diagnostic radio preferences, TX power, PHY settings, relevant queue
  sizes, BLE/split/display priorities are identical.
- ARM disassembly verifies actual calls to wrappers from matrix enqueue,
  position dispatch, right notification sender, central split RX queue, HID
  enqueue and host notification sender. This goes beyond checking symbol presence.
- Every delivered UF2 has valid nRF52840 family/magic/block data, stays within the
  application partition, and matches its SHA256SUMS.
- The keymap is byte-for-byte identical to the rollback tag; SHA256:
  `315680f9b91fd1e3c84c4023e0b2dd988f41402c23b4a7e3a6c02e203df3187f`.

Known compiler warnings and their impact are reviewed in
[migration.md](migration.md#build-verification-and-warning-review): deprecated
stock KSCAN, the peripheral's inapplicable board USB-HID default, nice!view's
unhandled NONE output icon, and the reset shield's zero-row mock keymap.
No new warnings originate in the diagnostic C module. There are no linker
overflows, undefined Kconfig symbols, or battery/GATT buffer warnings in the final
builds. Static memory headroom does not establish runtime stack high-water marks.

The fresh [v0.3 rollback run 35175223663](https://github.com/alish2001/corne/actions/runs/35175223663)
also passed both halves. The original May artifact expired; these are rebuilt
rollback images. No physical keyboard was flashed or tested during this migration.
USB shell operation, displays, battery measurements, reconnect behavior and daily
stability still require the [hardware test sequence](migration.md#first-test-and-ab-record).

## Artifact checksums

```text
b4c38d5b2f29d7223b5ad11c9c5ad292fea276888048481fb71a49dcba6beb78  corne-left-normal.uf2
c45a25da85a6ceba5bd313c24fe92c5e0d588a5c0162e63485ffe5cd1f74c963  corne-right-normal.uf2
527672c2289e193203569fad8b6db3effe45406923bb1cf763c139b902f449af  corne-left-diagnostic.uf2
ee7ef194aa6fdf24d0f5d14755d5f3be3a698fc26a6300503bc039bdbba84728  corne-right-diagnostic.uf2
f5842301f675d0bb5fec1e5acc572e61bb45389be254743458abbdeab760f85c  corne-settings-reset.uf2
```

Each Actions artifact includes the UF2, checksums, build log, resolved `config`,
devicetree and its generated header, ELF/map, frozen manifest, image digest and
build-info JSON. They are also downloaded to the repository's ignored `artifacts/`
directory. Save a copy before GitHub's 90-day artifact retention expires.

The migration encountered only build-wrapper failures after successful firmware
compilation: checking defaults in rendered DTS rather than generated headers,
freezing inactive simulation dependencies, and Git ownership during provenance
collection. These are fixed in the final successful workflow; no upstream source
patch or pin fallback was needed.
