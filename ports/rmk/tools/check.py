#!/usr/bin/env python3
"""Static checks only: no Rust toolchain or firmware compilation required."""
import json
import re
import subprocess
import sys
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[1]


def read_toml(path):
    return tomllib.loads(path.read_text(encoding="utf-8"))


def main():
    subprocess.run([sys.executable, str(ROOT / "tools/generate.py"), "--check"], check=True)
    configs = [read_toml(ROOT / target / "keyboard.toml") for target in ("dongle", "peripherals")]
    dongle, halves = configs
    assert dongle["keyboard"]["chip"] == "esp32s3"
    assert halves["keyboard"]["chip"] == "nrf52840"
    for key in ("split", "layout", "keymap", "behavior", "rmk"):
        assert dongle[key] == halves[key], f"Cross-target mismatch: {key}"
    assert dongle["split"]["central"]["rows"] == dongle["split"]["central"]["cols"] == 0
    assert len(dongle["split"]["peripheral"]) == 2
    assert [p["row_offset"] for p in dongle["split"]["peripheral"]] == [0, 4]
    for config in configs:
        assert len(config["keyboard"]["product_name"].encode()) <= 16, "BLE device name is limited to 16 bytes"
        assert not config["storage"]["clear_storage"], "Reset must be an explicit temporary CI build"
    pins = [
        (["P0_30", "P0_31", "P0_29", "P0_02"], ["P0_28", "P0_03", "P1_10", "P1_11", "P1_13", "P0_09", "P0_10"]),
        (["P1_09", "P0_28", "P0_03", "P1_10"], ["P0_30", "P0_31", "P0_29", "P0_02", "P1_13", "P0_10", "P0_09"]),
    ]
    for peripheral, (rows, cols) in zip(halves["split"]["peripheral"], pins):
        assert peripheral["matrix"]["row_pins"] == rows
        assert peripheral["matrix"]["col_pins"] == cols
        assert not peripheral["matrix"]["row2col"]
    coords = [(int(r), int(c)) for r, c in re.findall(r"\((\d+),(\d+)\)", dongle["layout"]["map"])]
    assert coords[30:32] == [(2, 6), (5, 6)], "Encoder click locations changed"
    assert coords[43:45] == [(3, 5), (7, 5)], "Layer-tap thumb locations changed"
    for layer in dongle["keymap"]["layer"]:
        assert layer["encoders"] == [["MouseWheelDown", "MouseWheelUp"], ["AudioVolUp", "AudioVolDown"]]
    assert dongle["storage"]["start_addr"] == 0x3F0000
    assert dongle["storage"]["num_sectors"] * 4096 == 0x10000
    assert halves["storage"]["start_addr"] == 0xD4000
    assert halves["storage"]["num_sectors"] * 4096 + 0xD4000 == 0xF4000
    memory = (ROOT / "peripherals/memory.x").read_text()
    assert "ORIGIN = 0x00001000, LENGTH = 0x000D3000" in memory
    partitions = [line.split(",") for line in (ROOT / "dongle/partitions.csv").read_text().splitlines() if line and not line.startswith("#")]
    factory = next(p for p in partitions if p[0].strip() == "factory")
    storage = next(p for p in partitions if p[0].strip() == "rmk")
    assert int(factory[3], 0) + int(factory[4], 0) <= int(storage[3], 0) == 0x3F0000
    assert int(storage[3], 0) + int(storage[4], 0) <= 4 * 1024 * 1024
    manifests = [read_toml(ROOT / target / "Cargo.toml") for target in ("dongle", "peripherals")]
    revisions = [manifest["dependencies"]["rmk"]["rev"] for manifest in manifests]
    assert revisions[0] == revisions[1] and re.fullmatch(r"[0-9a-f]{40}", revisions[0])
    for manifest in manifests:
        features = manifest["dependencies"]["rmk"]["features"]
        assert "split" in features and "dongle" not in features
        assert "display" not in features, "Wire protocol features must match on all images"
    vial = json.loads((ROOT / "dongle/vial.json").read_text())
    assert vial["matrix"] == {"rows": 8, "cols": 7}
    assert len(vial["customKeycodes"]) == 8
    encoders = [key for key in vial["layouts"]["keymap"][-1] if isinstance(key, str)]
    assert [key.split("\n")[0] for key in encoders] == ["0,0", "0,1", "1,0", "1,1"]
    assert all(key.split("\n")[-1] == "e" for key in encoders)
    # All tracked TOML and JSON files must parse, including target/toolchain configs.
    for path in ROOT.rglob("*.toml"):
        read_toml(path)
    for path in ROOT.rglob("*.json"):
        json.loads(path.read_text())
    print("PASS: matching protocol configuration, 50 keys, encoders, pins, flash boundaries, pinned RMK")


if __name__ == "__main__":
    main()
