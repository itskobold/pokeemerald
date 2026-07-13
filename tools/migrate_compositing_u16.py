#!/usr/bin/env python3
"""
One-shot migration: widen each data/tilesets/*/*/metatile_compositing.bin from one
u8 per metatile to one u16 per metatile, converting the old 2-bit reflection-layer
*selector* into a 3-bit reflection-layer *mask*:

  old byte: front bits 0-2 | behind bits 3-5 | reflection selector bits 6-7
            (selector 0 = none, 1/2/3 = layer 0/1/2 reflects)
  new u16:  front bits 0-2 | behind bits 3-5 | reflection mask bits 6-8
            (one bit per layer, so multiple layers can reflect)

    refl_mask = 0 if selector == 0 else (1 << (selector - 1))
    new = (old & 0x3F) | (refl_mask << 6)

Metatile count comes from the sibling metatiles.bin (NUM_TILES_PER_METATILE=12,
u16 per tile -> 24 bytes per metatile). Idempotent: a file already sized to 2 bytes
per metatile is treated as migrated and skipped, so this is safe to re-run.
"""
import glob
import os
import struct
import sys

TILESETS_DIR = os.path.join(os.path.dirname(__file__), "..", "data", "tilesets")
BYTES_PER_METATILE = 12 * 2  # NUM_TILES_PER_METATILE * sizeof(u16)


def migrate_dir(tileset_dir):
    metatiles_path = os.path.join(tileset_dir, "metatiles.bin")
    comp_path = os.path.join(tileset_dir, "metatile_compositing.bin")

    if not os.path.exists(metatiles_path) or not os.path.exists(comp_path):
        return "no metatiles.bin/metatile_compositing.bin"

    count = os.path.getsize(metatiles_path) // BYTES_PER_METATILE
    comp_size = os.path.getsize(comp_path)

    if comp_size == count * 2:
        return "skip (already u16)"
    if comp_size != count:
        return f"skip (unexpected size {comp_size}, metatiles {count})"

    with open(comp_path, "rb") as f:
        old = f.read()
    new = []
    for b in old:
        selector = (b >> 6) & 0x3
        refl_mask = 0 if selector == 0 else (1 << (selector - 1))
        new.append((b & 0x3F) | (refl_mask << 6))
    with open(comp_path, "wb") as f:
        f.write(struct.pack(f"<{len(new)}H", *new))

    return f"migrated ({count} metatiles)"


def main():
    dirs = sorted(
        os.path.dirname(p)
        for p in glob.glob(os.path.join(TILESETS_DIR, "*", "*", "metatile_compositing.bin"))
    )
    migrated = skipped = 0
    for d in dirs:
        result = migrate_dir(d)
        if result.startswith("migrated"):
            migrated += 1
        else:
            skipped += 1
        rel = os.path.relpath(d, TILESETS_DIR)
        print(f"  {rel:45s} {result}")
    print(f"\nDone: {migrated} migrated, {skipped} skipped.")


if __name__ == "__main__":
    sys.exit(main())
