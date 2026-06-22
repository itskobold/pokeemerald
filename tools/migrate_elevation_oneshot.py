#!/usr/bin/env python3
"""One-shot migration: old overloaded elevation byte -> pure collision-bit + 7-bit level.

Old attributes.bin / event elevation semantics:
  0 = transition, 1 = collision, 2 = surf water, 3 = multi-level, 4+ = real levels
New: bit 7 = collision (impassable), bits 0-6 = elevation level (0-127), no special values.
Movement semantics moved to metatile behaviors (stairs gate levels, surf MBs gate water).

Run once from the repo root, then rebuild. Safe to delete afterwards.
"""
import json, os, glob
from collections import Counter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT = 5
COLLISION = 0x80
EMASK = 0x7F


def load_layouts():
    """layout id -> (dir, w, h); also dir -> (w, h)."""
    data = json.load(open(os.path.join(ROOT, 'data/layouts/layouts.json')))['layouts']
    by_id, by_dir = {}, {}
    for L in data:
        d = os.path.join(ROOT, os.path.dirname(L['blockdata_filepath']))
        by_id[L['id']] = (d, L['width'], L['height'])
        by_dir[d] = (L['width'], L['height'])
    return by_id, by_dir


def neighbor_level(orig, i, w, h):
    """Most common real (>=4) neighbour level, else DEFAULT."""
    x, y = i % w, i // w
    levels = []
    for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
        if 0 <= nx < w and 0 <= ny < h:
            v = orig[ny * w + nx] & EMASK
            if v >= 4:
                levels.append(v)
    if levels:
        return Counter(levels).most_common(1)[0][0]
    return DEFAULT


def migrate_attr(orig, w, h):
    out = bytearray(len(orig))
    for i, raw in enumerate(orig):
        v = raw & EMASK
        if v == 1:                       # collision -> impassable bit + ground level
            out[i] = COLLISION | DEFAULT
        elif v == 2:                     # surf water -> ground level (surf MB blocks walking)
            out[i] = DEFAULT
        elif v == 0 or v == 3:           # transition / multi -> a neighbour's real level
            out[i] = neighbor_level(orig, i, w, h)
        else:                            # 4..127 already a real level
            out[i] = v
    return bytes(out)


def migrate_event_elev(e, attr, w, h, x, y):
    if e >= 4:
        return e
    if e == 2:
        return DEFAULT
    # 0 (any-level wildcard), 1, 3 -> the underlying tile's real level, else DEFAULT
    if attr is not None and 0 <= x < w and 0 <= y < h:
        t = attr[y * w + x] & EMASK
        if t >= 4:
            return t
    return DEFAULT


def main():
    by_id, by_dir = load_layouts()

    # cache of ORIGINAL attribute bytes per dir (for event underlying-tile lookups)
    orig_attr = {}
    for d in by_dir:
        ap = os.path.join(d, 'attributes.bin')
        if os.path.exists(ap):
            orig_attr[d] = open(ap, 'rb').read()

    # 1) migrate map.json event elevations (uses ORIGINAL attributes)
    EVENT_KEYS = ('object_events', 'warp_events', 'coord_events', 'bg_events')
    maps_changed = 0
    for f in glob.glob(os.path.join(ROOT, 'data/maps/*/map.json')):
        d = json.load(open(f))
        info = by_id.get(d.get('layout'))
        attr = w = h = None
        if info:
            ldir, w, h = info
            attr = orig_attr.get(ldir)
        changed = False
        for k in EVENT_KEYS:
            for ev in d.get(k) or []:
                if 'elevation' not in ev:
                    continue
                ne = migrate_event_elev(ev['elevation'], attr, w, h,
                                        ev.get('x', 0), ev.get('y', 0))
                if ne != ev['elevation']:
                    ev['elevation'] = ne
                    changed = True
        if changed:
            open(f, 'w').write(json.dumps(d, indent=2, ensure_ascii=False) + '\n')
            maps_changed += 1

    # 2) migrate attributes.bin
    attrs_changed = 0
    for d, (w, h) in by_dir.items():
        ap = os.path.join(d, 'attributes.bin')
        if not os.path.exists(ap):
            continue
        orig = orig_attr[d]
        new = migrate_attr(orig, w, h)
        if new != orig:
            open(ap, 'wb').write(new)
            attrs_changed += 1

    print(f'attributes.bin migrated: {attrs_changed}/{len(by_dir)}')
    print(f'map.json files changed:  {maps_changed}')


if __name__ == '__main__':
    main()
