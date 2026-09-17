# Wireless Corne firmware

nice!nano V2 on both halves, left central, right BLE peripheral, nice!view displays,
and ZMK Studio. The keymap is in [config/corne.keymap](config/corne.keymap).

The modern baseline pins **ZMK `9ebbeff0a8b69a42f14aec022cdf16c7a107b9e0`** and
**Zephyr 4.1.0 `10ba6d0cb38bc3d258775d27982f707599320085`** in
[config/west.yml](config/west.yml). It preserves +8 dBm TX power, restores upstream
5/5 ms debounce, and leaves host/split connection preferences alone.

- [Migration audit, known limitations and test plan](docs/migration.md)
- [Diagnostic build and release-tracing guide](docs/diagnostics.md)
- Rollback tag: **`zmk-v0.3-known-good`**, original config commit `1fa9653`.

## Build

Push the migration branch to build all five targets, or use the commands below to
select `normal` or `diagnostic`. After merging, these choices are also available
under **Actions → Firmware → Run workflow**:

```sh
gh workflow run build.yml --ref codex/zmk-main-baseline -f mode=normal
gh workflow run build.yml --ref codex/zmk-main-baseline -f mode=diagnostic
gh run list --branch codex/zmk-main-baseline
gh run download RUN_ID --dir artifacts/RUN_ID
```

Artifacts contain the UF2, SHA256SUMS, build log, resolved config, devicetree,
ELF/map, frozen dependency manifest and build metadata. `normal` includes reset:

| Target | Firmware |
| --- | --- |
| Left daily driver | `corne-left-normal.uf2` |
| Right daily driver | `corne-right-normal.uf2` |
| Left diagnostics | `corne-left-diagnostic.uf2` |
| Right diagnostics | `corne-right-diagnostic.uf2` |
| Settings reset, either half | `corne-settings-reset.uf2` |

Flash the matching image to **both halves** for the migration. Reset firmware is
optional recovery tooling: it erases saved settings, including bonds and Studio
edits. After using it, flash ordinary left/right firmware again.

The workflow runs ZMK's supported `west init`, `west update`, `west zephyr-export`
and `west build` commands in a pinned ZMK build container. Its small local wrapper
also verifies the resolved settings and retains diagnostic evidence. To build in
an existing ZMK/Zephyr development environment (Python with PyYAML, west and the
SDK installed):

```sh
repo="$PWD"
workspace="$(mktemp -d)"
mkdir "$workspace/config"
cp -R "$repo/config/." "$workspace/config/"
cd "$workspace"
west init -l config
west update --fetch-opt=--filter=tree:0
west zephyr-export
python3 "$repo/scripts/build.py" corne-left-normal --workspace "$workspace"
python3 "$repo/scripts/build.py" corne-right-normal --workspace "$workspace"
python3 "$repo/scripts/build.py" corne-settings-reset --workspace "$workspace"
python3 "$repo/scripts/build.py" corne-left-diagnostic --workspace "$workspace"
python3 "$repo/scripts/build.py" corne-right-diagnostic --workspace "$workspace"
```

Do not initialize west in the repository itself: the repository's `zephyr/`
directory is the optional diagnostic module descriptor.
