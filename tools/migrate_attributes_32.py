#!/usr/bin/env python3
"""
One-shot migration: re-pack each data/layouts/*/attributes.bin and border_attributes.bin
(u16 per tile) to widen bgMaterial and bridge 4->5 bits (0-31 each), consuming the two
free bits left by the elevation 6->4 bit narrowing.

  old u16: elevation bits 0-3 | free bits 4-5 | cliff bit 6 (0x40) | collision bit 7 (0x80)
           | bgMaterial bits 8-11 | bridge bits 12-15
  new u16: elevation bits 0-3 | cliff bit 4 (0x10) | collision bit 5 (0x20)
           | bgMaterial bits 6-10 (0x07C0) | bridge bits 11-15 (0xF800)

    new = (old & 0x000F) | ((old & 0x00C0) >> 2) | (((old >> 8) & 0x0F) << 6)
        | (((old >> 12) & 0x0F) << 11)

Field values are unchanged (elevation <= 15, bgMaterial/bridge <= 15 in old data), so the
re-pack is lossless.

Idempotent via two guards: a global marker file written after a successful run, and a
per-file check — the old format never sets bits 4-5 (they were the freed elevation bits,
zero across every layout), whereas the new format puts cliff/collision there, so a file
with any bit 4/5 set is already migrated. (Files with no cliff/collision anywhere are
ambiguous, hence the marker as the primary safeguard.)
"""
import glob
import os
import struct
import sys

LAYOUTS_DIR = os.path.join(os.path.dirname(__file__), "..", "data", "layouts")
MARKER = os.path.join(os.path.dirname(__file__), ".attributes_32_migrated")


def already_migrated(vals):
    # New format uses bits 4-5 for cliff/collision; old format never does.
    return any(v & 0x0030 for v in vals)


def migrate_file(attr_path, ref_path):
    if not os.path.exists(attr_path) or not os.path.exists(ref_path):
        return "no bin", False

    tiles = os.path.getsize(ref_path) // 2
    attr_size = os.path.getsize(attr_path)
    if attr_size != tiles * 2:
        return f"skip (size {attr_size} != {tiles * 2})", False

    with open(attr_path, "rb") as f:
        old = list(struct.unpack(f"<{tiles}H", f.read()))

    if already_migrated(old):
        return "skip (already migrated)", False

    new = [(v & 0x000F) | ((v & 0x00C0) >> 2) | (((v >> 8) & 0x0F) << 6) | (((v >> 12) & 0x0F) << 11)
           for v in old]
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
        for attr, ref in (("attributes.bin", "map.bin"), ("border_attributes.bin", "border.bin")):
            result, did = migrate_file(os.path.join(d, attr), os.path.join(d, ref))
            migrated += did
            skipped += not did
            print(f"  {os.path.basename(d):45s} {attr:22s} {result}")
    print(f"\nDone: {migrated} migrated, {skipped} skipped.")

    with open(MARKER, "w") as f:
        f.write("attributes.bin migrated to the 5-bit bgMaterial/bridge layout.\n")


if __name__ == "__main__":
    sys.exit(main())
