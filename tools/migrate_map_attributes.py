#!/usr/bin/env python3
"""
One-shot migration: split the old 16-bit packed map blocks
(10-bit metatile id | 2-bit collision | 4-bit elevation) into the new format:
  - map.bin / border.bin : full 16-bit metatile id (high 6 bits stripped)
  - attributes.bin       : one byte per tile [location:2 | elevation:4 | collision:2]

Idempotent: a layout directory that already has attributes.bin is skipped, so this
is safe to re-run after adding new (old-format) maps.
"""
import glob
import os
import struct
import sys

LAYOUTS_DIR = os.path.join(os.path.dirname(__file__), "..", "data", "layouts")

OLD_METATILE_MASK = 0x03FF
OLD_COLLISION_SHIFT = 10
OLD_COLLISION_MASK = 0x3
OLD_ELEVATION_SHIFT = 12
OLD_ELEVATION_MASK = 0xF

# New attribute byte layout
NEW_COLLISION_SHIFT = 0
NEW_ELEVATION_SHIFT = 2


def read_u16_array(path):
    with open(path, "rb") as f:
        data = f.read()
    if len(data) % 2 != 0:
        raise ValueError(f"{path}: not a whole number of u16 blocks")
    return list(struct.unpack(f"<{len(data) // 2}H", data))


def write_u16_array(path, values):
    with open(path, "wb") as f:
        f.write(struct.pack(f"<{len(values)}H", *values))


def write_u8_array(path, values):
    with open(path, "wb") as f:
        f.write(bytes(values))


def split_block(block):
    metatile = block & OLD_METATILE_MASK
    collision = (block >> OLD_COLLISION_SHIFT) & OLD_COLLISION_MASK
    elevation = (block >> OLD_ELEVATION_SHIFT) & OLD_ELEVATION_MASK
    attr = (collision << NEW_COLLISION_SHIFT) | (elevation << NEW_ELEVATION_SHIFT)
    return metatile, attr


def migrate_dir(layout_dir):
    map_path = os.path.join(layout_dir, "map.bin")
    border_path = os.path.join(layout_dir, "border.bin")
    attr_path = os.path.join(layout_dir, "attributes.bin")

    if not os.path.exists(map_path):
        return "no map.bin"
    if os.path.exists(attr_path):
        return "skip (already migrated)"

    blocks = read_u16_array(map_path)
    metatiles = []
    attrs = []
    for b in blocks:
        m, a = split_block(b)
        metatiles.append(m)
        attrs.append(a)
    write_u16_array(map_path, metatiles)
    write_u8_array(attr_path, attrs)

    if os.path.exists(border_path):
        border = read_u16_array(border_path)
        write_u16_array(border_path, [b & OLD_METATILE_MASK for b in border])

    return f"migrated ({len(blocks)} tiles)"


def main():
    dirs = sorted(d for d in glob.glob(os.path.join(LAYOUTS_DIR, "*")) if os.path.isdir(d))
    migrated = skipped = 0
    for d in dirs:
        result = migrate_dir(d)
        if result.startswith("migrated"):
            migrated += 1
        else:
            skipped += 1
        print(f"  {os.path.basename(d):45s} {result}")
    print(f"\nDone: {migrated} migrated, {skipped} skipped.")


if __name__ == "__main__":
    sys.exit(main())
