#!/usr/bin/env python3
"""
One-shot: bake the DrawMetatileAtWithId "fgEmpty auto-promote" heuristic
(src/field_camera.c) into each metatile_compositing.bin's BEHIND mask, so the
heuristic can be deleted without changing any on-screen occlusion.

The heuristic (behind-cliff / promoted cells only): if none of the layers named
by the metatile's BEHIND mask have content, force every content-bearing layer
into the foreground. We reproduce that per metatile and OR the promoted layers
into the stored BEHIND field. FRONT (bits 0-2) and reflection (bits 6-8) are
untouched — the heuristic only ran in the promote branch, which uses BEHIND.

"Content" for a layer matches LayerSubtileHasContent: any subtile id != 0, and
layer 0 additionally counts as content when the metatile uses bgMaterial
(attribute bit 8) — the material fills empty ground cells and promotes with it.

Terrain tiles the user has already re-authored (working tree != HEAD) are their
new intent and are left exactly as authored — never baked.

Idempotent by construction: a mask that already names a content layer has
fgEmpty == False, so nothing is added on a second run.
"""
import glob
import os
import struct
import subprocess
import sys

ROOT = os.path.join(os.path.dirname(__file__), "..")
TILESETS_DIR = os.path.join(ROOT, "data", "tilesets")
BYTES_PER_METATILE = 12 * 2          # NUM_TILES_PER_METATILE * sizeof(u16)
METATILE_ATTR_BGMATERIAL = 0x0100    # attribute bit 8


def content_mask(mt, attrs, mid):
    """3-bit mask of layers that have content on metatile `mid`."""
    off = mid * BYTES_PER_METATILE
    v = struct.unpack("<12H", mt[off:off + BYTES_PER_METATILE])
    mask = 0
    for l in range(3):
        if any((t & 0x3FF) for t in v[l * 4:l * 4 + 4]):
            mask |= (1 << l)
    # layer 0 also counts the bgMaterial ground that fills empty cells
    if attrs is not None and (attrs[mid] & METATILE_ATTR_BGMATERIAL):
        mask |= (1 << 0)
    return mask


def bake_value(comp, cmask):
    behind = (comp >> 3) & 7
    fg_empty = (behind & cmask) == 0        # no BEHIND-named layer has content
    if fg_empty:
        behind |= cmask                     # override force-promotes content layers
    return (comp & ~0x38) | (behind << 3)   # keep front (0-2) + refl (6-8)


def head_blob(relpath):
    r = subprocess.run(["git", "-C", ROOT, "show", f"HEAD:{relpath}"],
                       capture_output=True)
    return r.stdout if r.returncode == 0 else None


def process(tileset_dir):
    metatiles_path = os.path.join(tileset_dir, "metatiles.bin")
    comp_path = os.path.join(tileset_dir, "metatile_compositing.bin")
    attrs_path = os.path.join(tileset_dir, "metatile_attributes.bin")
    if not (os.path.exists(metatiles_path) and os.path.exists(comp_path)):
        return "no metatiles/compositing"

    mt = open(metatiles_path, "rb").read()
    comp = open(comp_path, "rb").read()
    count = len(mt) // BYTES_PER_METATILE
    if len(comp) != count * 2:
        return f"skip (compositing not u16: {len(comp)} vs {count*2})"

    attrs = None
    if os.path.exists(attrs_path):
        ab = open(attrs_path, "rb").read()
        if len(ab) == count * 2:
            attrs = struct.unpack(f"<{count}H", ab)

    cur = list(struct.unpack(f"<{count}H", comp))

    # Preserve user-authored (working tree != HEAD) tiles verbatim.
    rel = os.path.relpath(comp_path, ROOT)
    head = head_blob(rel)
    edited = set()
    if head is not None and len(head) == len(comp):
        h = struct.unpack(f"<{count}H", head)
        edited = {i for i in range(count) if h[i] != cur[i]}

    changed = 0
    for mid in range(count):
        if mid in edited:
            continue
        baked = bake_value(cur[mid], content_mask(mt, attrs, mid))
        if baked != cur[mid]:
            cur[mid] = baked
            changed += 1

    if changed:
        with open(comp_path, "wb") as f:
            f.write(struct.pack(f"<{count}H", *cur))
    return f"baked {changed}/{count}" + (f", kept {len(edited)} authored" if edited else "")


def main():
    dirs = sorted(os.path.dirname(p) for p in
                  glob.glob(os.path.join(TILESETS_DIR, "*", "*", "metatile_compositing.bin")))
    for d in dirs:
        print(f"  {os.path.relpath(d, TILESETS_DIR):45s} {process(d)}")


if __name__ == "__main__":
    sys.exit(main())
