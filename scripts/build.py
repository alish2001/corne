#!/usr/bin/env python3
"""Run the upstream west build recipe and retain evidence beside each UF2.

Requires an initialized/updated west workspace and ZMK's build dependencies.
Examples: python3 scripts/build.py corne-left-normal --workspace /path/to/west
          python3 scripts/build.py --matrix diagnostic
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import shutil
import subprocess

import yaml

REPO = Path(__file__).resolve().parents[1]


def entries(mode="all"):
    paths = {"normal": "build.yaml", "diagnostic": "build-diagnostic.yaml"}
    return [entry for key, path in paths.items() if mode in ("all", key)
            for entry in yaml.safe_load((REPO / path).read_text())["include"]]


def verify(build, entry):
    config = (build / "zephyr/.config").read_text()
    # Binding defaults are materialized in the generated header, not zephyr.dts.
    dt_header = (build / "zephyr/include/generated/zephyr/devicetree_generated.h").read_text()
    link_map = (build / "zephyr/zmk.map").read_text()
    values = dict(re.findall(r"^(CONFIG_\w+)=(.*)$", config, re.MULTILINE))
    assert values["CONFIG_BOARD_REVISION"] == '"2.0.0"'
    assert values["CONFIG_ZMK_BOARD_COMPAT"] == "y"
    if entry["shield"] == "settings_reset":
        assert values["CONFIG_ZMK_SETTINGS_RESET_ON_START"] == "y"
        return
    diagnostic = entry["artifact-name"].endswith("diagnostic")
    central = "corne_left" in entry["shield"]
    for key in ("BT_CTLR_TX_PWR_PLUS_8", "ZMK_DISPLAY", "ZMK_STUDIO", "NVS"):
        assert values[f"CONFIG_{key}"] == "y", key
    assert values.get("CONFIG_ZMK_STUDIO_LOCKING", "n") == "n"
    assert values["CONFIG_ZMK_KSCAN_DEBOUNCE_PRESS_MS"] == "-1"
    assert values["CONFIG_ZMK_KSCAN_DEBOUNCE_RELEASE_MS"] == "-1"
    for edge in ("press", "release"):
        assert re.search(rf"#define DT_N_S_kscan_P_debounce_{edge}_ms\s+5\s*$",
                         dt_header, re.MULTILINE), f"effective {edge} debounce"
    assert values.get("CONFIG_ZMK_SPLIT_ROLE_CENTRAL", "n") == ("y" if central else "n")
    if central:
        assert values["CONFIG_ZMK_KEYBOARD_NAME"] == '"Corne"'
        for key, expected in (("INT", "6"), ("LATENCY", "30"), ("TIMEOUT", "400")):
            assert values[f"CONFIG_ZMK_SPLIT_BLE_PREF_{key}"] == expected
        assert values["CONFIG_ZMK_STUDIO_TRANSPORT_UART"] == "y"
    assert values.get("CONFIG_CORNE_DIAGNOSTICS", "n") == ("y" if diagnostic else "n")
    assert values.get("CONFIG_ZMK_USB_LOGGING", "n") == ("y" if diagnostic else "n")
    wrappers = ["zmk_event_manager_raise", "bt_gatt_notify_cb", "z_impl_k_msgq_put"]
    if central:
        wrappers.append("zmk_hog_send_keyboard_report")
    for name in wrappers:
        assert (f"__wrap_{name}" in link_map) == diagnostic, name
    if not diagnostic:
        assert "corne_diag" not in link_map


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("artifact", nargs="?")
    parser.add_argument("--matrix", choices=("normal", "diagnostic", "all"))
    parser.add_argument("--workspace", type=Path)
    parser.add_argument("--output", type=Path, default=REPO / "artifacts")
    args = parser.parse_args()
    if args.matrix:
        print(json.dumps({"include": [{"artifact": e["artifact-name"]} for e in entries(args.matrix)]}))
        return
    matches = [e for e in entries() if e["artifact-name"] == args.artifact]
    if len(matches) != 1 or args.workspace is None:
        parser.error("Supply an artifact name from build*.yaml and --workspace")
    entry = matches[0]
    # The container runs as a different UID from the checkout action. Trust only
    # this explicitly selected repository for this one read, not all directories.
    config_commit = subprocess.check_output(
        ["git", "-c", f"safe.directory={REPO}", "rev-parse", "HEAD"],
        cwd=REPO, text=True).strip()
    workspace = args.workspace.resolve()
    build = REPO / "build" / args.artifact
    output = args.output.resolve() / args.artifact
    output.mkdir(parents=True, exist_ok=True)
    command = ["west", "build", "-p", "always", "-s", str(workspace / "zmk/app"),
               "-d", str(build), "-b", entry["board"]]
    if entry.get("snippet"):
        command += ["-S", entry["snippet"]]
    command += ["--", f"-DZMK_CONFIG={REPO / 'config'}", f"-DZMK_EXTRA_MODULES={REPO}",
                f"-DSHIELD={entry['shield']}"]
    command += shlex.split(entry.get("cmake-args", ""))
    (output / "command.json").write_text(json.dumps(command, indent=2) + "\n")
    with (output / "build.log").open("w") as log:
        process = subprocess.Popen(command, cwd=workspace, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, text=True)
        for line in process.stdout:
            print(line, end="", flush=True)
            log.write(line)
        rc = process.wait()
    # Preserve partial evidence even when compilation or verification fails.
    for source in (".config", "zephyr.dts", "zmk.map", "zmk.elf"):
        path = build / "zephyr" / source
        if path.exists():
            shutil.copy2(path, output / source.lstrip("."))
    header = build / "zephyr/include/generated/zephyr/devicetree_generated.h"
    if header.exists():
        shutil.copy2(header, output / header.name)
    if rc:
        raise SystemExit(rc)
    verify(build, entry)
    uf2 = output / f"{args.artifact}.uf2"
    shutil.copy2(build / "zephyr/zmk.uf2", uf2)
    frozen = subprocess.check_output(["west", "manifest", "--freeze", "--active-only"],
                                     cwd=workspace, text=True)
    (output / "west-frozen.yml").write_text(frozen)
    (output / "SHA256SUMS").write_text(f"{hashlib.sha256(uf2.read_bytes()).hexdigest()}  {uf2.name}\n")
    metadata = {
        "config_commit": config_commit,
        "keymap_sha256": hashlib.sha256((REPO / "config/corne.keymap").read_bytes()).hexdigest(),
        "zephyr_version": (workspace / "zephyr/VERSION").read_text(),
        "verification": "passed",
    }
    (output / "build-info.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(f"Verified {uf2}")


if __name__ == "__main__":
    main()
