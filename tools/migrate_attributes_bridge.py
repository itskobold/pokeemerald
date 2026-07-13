#!/usr/bin/env python3
"""
One-shot migration: re-pack each data/layouts/*/attributes.bin (u16 per tile) to free
the top nibble for the new per-tile bridge attribute, by narrowing elevation 8->6 bits.

  old u16: elevation bits 0-7 | cliff bit 8 (0x100) | collision bit 9 (0x200) | bgMaterial bits 10-13
  new u16: elevation bits 0-5 | cliff bit 6 (0x40)  | collision bit 7 (0x80)  | bgMaterial bits 8-11
           | bridge bits 12-15 (initialised to 0)

    new = (old & 0x003F) | ((old & 0x3F00) >> 2)

Elevation's old bits 6-7 are always zero (the max value across every layout is 11), so
narrowing to 6 bits is lossless. The dropped free bits 14-15 were also always zero.

Idempotent via two guards: a global marker file written after a successful run (skips
everything on re-run), and a per-file check — the old format never sets bits 6-7 (elevation
is <= 63 and cliff/collision live at bits 8-9), whereas the new format puts cliff at bit 6 and
collision at bit 7, so a file with any bit 6/7 set is already migrated. (A handful of layouts
have no cliff/collision at all, hence the marker as the primary safeguard.)
"""
import glob
import os
import struct
import sys

LAYOUTS_DIR = os.path.join(os.path.dirname(__file__), "..", "data", "layouts")
MARKER = os.path.join(os.path.dirname(__file__), ".attributes_bridge_migrated")


def already_migrated(vals):
    # New format uses bits 6-7 for cliff/collision; old format never does.
    return any(v & 0x00C0 for v in vals)


def migrate_dir(layout_dir):
    map_path = os.path.join(layout_dir, "map.bin")
    attr_path = os.path.join(layout_dir, "attributes.bin")

    if not os.path.exists(map_path) or not os.path.exists(attr_path):
        return "no map.bin/attributes.bin", False

    tiles = os.path.getsize(map_path) // 2
    attr_size = os.path.getsize(attr_path)
    if attr_size != tiles * 2:
        return f"skip (size {attr_size} != {tiles * 2})", False

    with open(attr_path, "rb") as f:
        old = list(struct.unpack(f"<{tiles}H", f.read()))

    if already_migrated(old):
        return "skip (already migrated)", False

    # Sanity: no old-format tile should carry an elevation above the 6-bit cap once narrowed.
    if any((v & 0x00FF) > 0x3F for v in old):
        return "skip (elevation > 63, refusing to truncate)", False

    new = [(v & 0x003F) | ((v & 0x3F00) >> 2) for v in old]
    with open(attr_path, "wb") as f:
        f.write(struct.pack(f"<{tiles}H", *new))
    return f"migrated ({tiles} tiles)", True


def main():
    if os.path.exists(MARKER):
        print(f"Marker {MARKER} present; already migrated. Delete it to force a re-run.")
        return 0

    dirs = sorted(d for d in glob.glob(os.path.join(LAYOUTS_DIR, "*")) if os.path.isdir(d))
    migrated = skipped = 0
    for d in dirs:
        result, did = migrate_dir(d)
        migrated += did
        skipped += not did
        print(f"  {os.path.basename(d):45s} {result}")
    print(f"\nDone: {migrated} migrated, {skipped} skipped.")

    with open(MARKER, "w") as f:
        f.write("attributes.bin migrated to the bridge layout (elevation 6-bit + bridge nibble).\n")


if __name__ == "__main__":
    sys.exit(main())
