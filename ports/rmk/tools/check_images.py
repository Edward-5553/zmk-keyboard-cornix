#!/usr/bin/env python3
"""Check CI image addresses without executing or compiling firmware."""
import argparse
import struct
from pathlib import Path


def check_uf2(path):
    data = path.read_bytes()
    assert data and len(data) % 512 == 0, f"Invalid UF2 length: {path}"
    count = len(data) // 512
    addresses = []
    for index in range(count):
        block = data[index * 512:(index + 1) * 512]
        magic0, magic1, flags, address, size, block_no, total, family = struct.unpack_from("<8I", block)
        assert (magic0, magic1) == (0x0A324655, 0x9E5D5157)
        assert struct.unpack_from("<I", block, 508)[0] == 0x0AB16F30
        assert flags & 0x2000 and family == 0xADA52840, "Expected nRF52840 family"
        assert not flags & 1, "Unexpected non-flash UF2 block"
        assert total == count and block_no == index
        assert 0 < size <= 476
        assert 0x1000 <= address < address + size <= 0xD4000, "UF2 overlaps storage/bootloader"
        addresses.append((address, address + size))
    for previous, current in zip(sorted(addresses), sorted(addresses)[1:]):
        assert previous[1] <= current[0], "Overlapping UF2 blocks"
    print(f"PASS {path.name}: application only, {min(a for a, _ in addresses):#x}..{max(b for _, b in addresses):#x}")


def check_merged(path):
    data = path.read_bytes()
    assert 0x10000 < len(data) < 0x3F0000, "Merged image would erase RMK storage; use --skip-padding"
    assert data[0] == data[0x10000] == 0xE9, "Expected S3 bootloader at 0x0 and application at 0x10000"
    assert data[0x8000:0x8002] == b"\xaa\x50", "Expected partition table at 0x8000"
    print(f"PASS {path.name}: merged image ends at {len(data):#x}, before RMK storage")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    images = list(args.directory.glob("*.uf2")) + list(args.directory.glob("*-merged.bin"))
    assert images, f"No firmware images found in {args.directory}"
    for image in images:
        (check_uf2 if image.suffix == ".uf2" else check_merged)(image)


if __name__ == "__main__":
    main()
