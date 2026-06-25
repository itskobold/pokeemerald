#!/usr/bin/env python3
"""
One-shot migration: widen each data/layouts/*/attributes.bin from one u8 per tile
to one u16 per tile, remapping the packed bits to the new layout:

  old byte: elevation bits 0-5 | cliff bit 6 (0x40) | collision bit 7 (0x80)
  new u16:  elevation bits 0-7 | cliff bit 8 (0x100) | collision bit 9 (0x200) | bgMaterial bits 10-13

    new = (old & 0x3F) | ((old & 0xC0) << 2)

Idempotent: a file already sized to 2 bytes per tile (matching the tile count from
map.bin) is treated as already migrated and skipped, so this is safe to re-run.
"""
import glob
import os
import struct
import sys

LAYOUTS_DIR = os.path.join(os.path.dirname(__file__), "..", "data", "layouts")


def migrate_dir(layout_dir):
    map_path = os.path.join(layout_dir, "map.bin")
    attr_path = os.path.join(layout_dir, "attributes.bin")

    if not os.path.exists(map_path) or not os.path.exists(attr_path):
        return "no map.bin/attributes.bin"

    tiles = os.path.getsize(map_path) // 2  # map.bin is u16 per tile
    attr_size = os.path.getsize(attr_path)

    if attr_size == tiles * 2:
        return "skip (already u16)"
    if attr_size != tiles:
        return f"skip (unexpected size {attr_size}, tiles {tiles})"

    with open(attr_path, "rb") as f:
        old = f.read()
    new = [(b & 0x3F) | ((b & 0xC0) << 2) for b in old]
    with open(attr_path, "wb") as f:
        f.write(struct.pack(f"<{len(new)}H", *new))

    return f"migrated ({tiles} tiles)"


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
