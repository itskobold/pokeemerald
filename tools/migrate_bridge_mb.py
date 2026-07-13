#!/usr/bin/env python3
"""
One-shot: fold the removed MB_BRIDGE_OVER_* behaviours into the single MB_BRIDGE
(value 0x70), and un-bake the cliff-promotion compositing heuristic for the
resulting MB_BRIDGE tiles (they keep the runtime heuristic — see field_camera.c —
so their authored BEHIND mask must be the pre-bake value, not the baked one).

  attributes: behaviour field (bits 0-7) in {0x71,0x72,0x73,0x7A,0x7B,0x7C,0x7D}
              -> 0x70 (MB_BRIDGE). Existing 0x70 tiles already are MB_BRIDGE.
  compositing: every metatile whose (migrated) behaviour == 0x70 has its
               compositing value restored to HEAD (undoing tools/bake_cliff_promote.py
               for bridge tiles only). Non-bridge tiles keep their bake.

Idempotent: re-running finds no old values and the compositing already equals HEAD
for bridge tiles.
"""
import glob
import os
import struct
import subprocess

ROOT = os.path.join(os.path.dirname(__file__), "..")
OLD_BRIDGE = {0x71, 0x72, 0x73, 0x7A, 0x7B, 0x7C, 0x7D}
MB_BRIDGE = 0x70


def head_blob(relpath):
    r = subprocess.run(["git", "-C", ROOT, "show", f"HEAD:{relpath}"], capture_output=True)
    return r.stdout if r.returncode == 0 else None


def process(tileset_dir):
    attrs_path = os.path.join(tileset_dir, "metatile_attributes.bin")
    comp_path = os.path.join(tileset_dir, "metatile_compositing.bin")
    if not os.path.exists(attrs_path):
        return "no attributes"

    ab = open(attrs_path, "rb").read()
    n = len(ab) // 2
    attrs = list(struct.unpack(f"<{n}H", ab))

    migrated = 0
    for i in range(n):
        if (attrs[i] & 0xFF) in OLD_BRIDGE:
            attrs[i] = (attrs[i] & ~0xFF) | MB_BRIDGE
            migrated += 1
    if migrated:
        with open(attrs_path, "wb") as f:
            f.write(struct.pack(f"<{n}H", *attrs))

    # Un-bake compositing for every MB_BRIDGE tile: restore HEAD value.
    reverted = 0
    if os.path.exists(comp_path):
        comp = list(struct.unpack(f"<{n}H", open(comp_path, "rb").read()))
        head = head_blob(os.path.relpath(comp_path, ROOT))
        if head is not None and len(head) == n * 2:
            hc = struct.unpack(f"<{n}H", head)
            for i in range(n):
                if (attrs[i] & 0xFF) == MB_BRIDGE and comp[i] != hc[i]:
                    comp[i] = hc[i]
                    reverted += 1
            if reverted:
                with open(comp_path, "wb") as f:
                    f.write(struct.pack(f"<{n}H", *comp))

    if migrated or reverted:
        return f"migrated {migrated} attrs, un-baked {reverted} bridge comp"
    return "no change"


def main():
    dirs = sorted(os.path.dirname(p) for p in
                  glob.glob(os.path.join(ROOT, "data", "tilesets", "*", "*", "metatile_attributes.bin")))
    for d in dirs:
        res = process(d)
        if res != "no change":
            print(f"  {os.path.relpath(d, os.path.join(ROOT, 'data', 'tilesets')):40s} {res}")


if __name__ == "__main__":
    main()
