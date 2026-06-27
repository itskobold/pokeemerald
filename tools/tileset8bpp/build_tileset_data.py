#!/usr/bin/env python3
"""One-shot migration: per-tileset single .pal + 256-colour tiles.png.

For every tileset (a directory containing a `palettes/` folder) this:
  * merges the per-bank JASC files into one `palette.pal`
      - primary   : banks 0-5  -> 96 colours
      - secondary : banks 6-12 -> 112 colours
  * re-encodes `tiles.png` to an 8-bit colormap PNG with ABSOLUTE indices into the
    combined palette (each tile's bank, taken from the metatiles, is folded in as
    bank*16 + relative) so the PNG displays in its true colours. The build's
    png->8bpp step is told `-palette_mod 16`, which reduces each pixel back to the
    per-bank relative index (0-15) the runtime wants - byte-identical to the old
    4bpp tiles. unknown_tiles.png and anim/** stay relative (bank 0).
  * deletes the now-redundant `palettes/` folder.

It then rewrites the gTilesetPalettes_* arrays in src/graphics.c and
src/data/tilesets/graphics.h to read the single file (flat arrays; Tileset.palettes
is a flat const u16 *) and appends `-palette_mod 16` to every tiles.png INCGFX.

Run from the repo root: python3 tools/tileset8bpp/build_tileset_data.py
"""
import os, re, sys, glob, struct
from collections import Counter
from PIL import Image

ROOT = os.getcwd()
TILESETS = os.path.join("data", "tilesets")
PRIMARY_BANKS = range(0, 6)    # 6 banks -> 96 colours
SECONDARY_BANKS = range(6, 13) # 7 banks -> 112 colours

# The primary secret-base tileset is special: the decoration preview UI copies
# palettes straight out of its array by metatile palette field (up to bank 11),
# bypassing the field engine's 6-bank limit. Keep those banks so that path is
# byte-identical. See CopyPalette() in src/decoration.c.
SPECIAL_PRIMARY_BANKS = {
    os.path.join("data", "tilesets", "primary", "secret_base"): range(0, 12),
}


def read_jasc(path):
    """Return list of (r,g,b), padded to 16."""
    with open(path, "r", newline="") as f:
        lines = [l.rstrip("\r\n") for l in f]
    n = int(lines[2])
    cols = []
    for i in range(n):
        r, g, b = (int(x) for x in lines[3 + i].split())
        cols.append((r, g, b))
    while len(cols) < 16:
        cols.append((0, 0, 0))
    return cols[:16]


def write_jasc(path, colors):
    with open(path, "wb") as f:
        f.write(b"JASC-PAL\r\n0100\r\n")
        f.write(("%d\r\n" % len(colors)).encode())
        for r, g, b in colors:
            f.write(("%d %d %d\r\n" % (r, g, b)).encode())


def tileset_dirs():
    """Every directory that owns a palettes/ folder."""
    return sorted(os.path.dirname(p) for p in glob.glob(
        os.path.join(TILESETS, "**", "palettes"), recursive=True))


def is_secondary(tdir):
    return os.path.normpath(tdir).split(os.sep)[2] == "secondary"


def combined_palette(tdir):
    banks = [read_jasc(os.path.join(tdir, "palettes", "%02d.pal" % i))
             for i in range(16)]
    if is_secondary(tdir):
        use = SECONDARY_BANKS
    else:
        use = SPECIAL_PRIMARY_BANKS.get(os.path.normpath(tdir), PRIMARY_BANKS)
    out = []
    for b in use:
        out.extend(banks[b])
    return out


def owner_tileset(png, tdirs):
    """Nearest ancestor directory that is a tileset."""
    d = os.path.dirname(os.path.normpath(png))
    tset = set(tdirs)
    while True:
        if d in tset:
            return d
        nd = os.path.dirname(d)
        if nd == d:
            return None
        d = nd


def find_metatiles(tdir):
    """metatiles.bin for this tileset, walking up for shared ones (secret_base/*)."""
    d = tdir
    for _ in range(3):
        mt = os.path.join(d, "metatiles.bin")
        if os.path.exists(mt):
            return mt
        d = os.path.dirname(d)
    return None


def bank_map(tdir, secondary, ntiles):
    """tile-local-id -> local bank (0-5 primary / 0-6 secondary), via the most
    common palette field across the metatiles. Only this sheet's own tiles."""
    mt = find_metatiles(tdir)
    if mt is None:
        return {}
    with open(mt, "rb") as f:
        data = f.read()
    vals = struct.unpack("<%dH" % (len(data) // 2), data)
    base = 512 if secondary else 0
    lo, hi = (6, 12) if secondary else (0, 5)
    counts = {}
    for v in vals:
        tid, pal = v & 0x3FF, (v >> 12) & 0xF
        if base <= tid < base + ntiles and lo <= pal <= hi:
            counts.setdefault(tid - base, Counter())[pal] += 1
    return {t: c.most_common(1)[0][0] - lo for t, c in counts.items()}


def reencode_png(png, palette, banks=None):
    """Rewrite as 8-bit colormap with `palette`. Relative indices are preserved;
    if `banks` (tile-id -> local bank) is given, fold bank*16 in so the tile
    displays in its true colours (the build reduces it back via -palette_mod 16)."""
    im = Image.open(png)
    if im.mode != "P":
        raise SystemExit("%s is not a palette image (mode %s)" % (png, im.mode))
    w, h = im.size
    px = bytearray(b % 16 for b in im.getdata())  # heal to relative
    if banks:
        tw = w // 8
        for t, lb in banks.items():
            if not lb:
                continue
            ty, tx = (t // tw) * 8, (t % tw) * 8
            for y in range(ty, ty + 8):
                for x in range(tx, tx + 8):
                    px[y * w + x] += lb * 16
    flat = []
    for r, g, b in palette:
        flat += [r, g, b]
    out = Image.new("P", im.size)
    out.putpalette(flat)            # >16 entries forces 8-bit on save
    out.putdata(list(px))
    out.save(png, "PNG", bits=8, optimize=False)


def convert_data():
    tdirs = tileset_dirs()
    palettes = {d: combined_palette(d) for d in tdirs}
    print("Tilesets: %d" % len(tdirs))

    # 1. write combined palette.pal
    for d in tdirs:
        write_jasc(os.path.join(d, "palette.pal"), palettes[d])

    # 2. re-encode every PNG against its owning tileset's combined palette.
    #    The main tiles.png gets absolute (per-bank) indices for display; everything
    #    else (unknown_tiles.png, anim/**) stays relative.
    pngs = glob.glob(os.path.join(TILESETS, "**", "*.png"), recursive=True)
    for png in sorted(pngs):
        owner = owner_tileset(png, tdirs)
        if owner is None:
            print("  WARN no tileset owns %s" % png)
            continue
        banks = None
        if os.path.basename(png) == "tiles.png":
            im = Image.open(png)
            ntiles = (im.size[1] // 8) * (im.size[0] // 8)
            banks = bank_map(owner, is_secondary(owner), ntiles)
        reencode_png(png, palettes[owner], banks)
    print("PNGs re-encoded: %d" % len(pngs))

    # 3. drop the per-bank palette files
    for d in tdirs:
        pdir = os.path.join(d, "palettes")
        for f in glob.glob(os.path.join(pdir, "*.pal")):
            os.remove(f)
        os.rmdir(pdir)


# ---- C source rewrite -------------------------------------------------------

# matches: const u16 gTilesetPalettes_Name[][16] =\n{ ...16 INCGFX lines... };
# INCGFX always emits one brace-enclosed group, which can't fill a [][16] row
# from a single file, so the array becomes a flat 1-D array and Tileset.palettes
# is a plain (const u16 *). Secondary arrays hold banks 6-12 starting at index 0;
# LoadTilesetPalette / CopyPalette index them accordingly.
ARRAY_RE = re.compile(
    r'const u16 (gTilesetPalettes_\w+)\[\]\[16\] =\s*\{(.*?)\};',
    re.DOTALL)
PATH_RE = re.compile(r'INCGFX_U16\("([^"]*?)/palettes/00\.pal"')


def rewrite_c(path):
    with open(path) as f:
        src = f.read()

    def repl(m):
        name, body = m.group(1), m.group(2)
        pm = PATH_RE.search(body)
        if not pm:
            return m.group(0)
        base = pm.group(1)  # e.g. data/tilesets/secondary/petalburg
        return ('const u16 %s[] = INCGFX_U16("%s/palette.pal", ".gbapal");'
                % (name, base))

    new = ARRAY_RE.sub(repl, src)
    new = add_palette_mod(new)
    if new != src:
        with open(path, "w") as f:
            f.write(new)
        print("Rewrote %s" % path)


# tiles.png are absolute-indexed for display; tell png->8bpp to reduce modulo 16.
TILES_INCGFX_RE = re.compile(
    r'INCGFX_U(8|16|32)\("(data/tilesets/[^"]*tiles\.png)", "\.8bpp"(?:, "([^"]*)")?\)')


def add_palette_mod(src):
    def repl(m):
        u, path, args = m.group(1), m.group(2), m.group(3)
        if args and "-palette_mod" in args:
            return m.group(0)
        args = (args + " " if args else "") + "-palette_mod 16"
        return 'INCGFX_U%s("%s", ".8bpp", "%s")' % (u, path, args)
    return TILES_INCGFX_RE.sub(repl, src)


# ---- anim frames ------------------------------------------------------------
# Anim frames are DMA'd into fixed tile slots by AppendTilesetAnimToBuffer; the
# slot's metatile palette field gives the bank, so each frame can be displayed in
# its true colours too (build reduces it back with -palette_mod 16, like tiles.png).
TILESET_ANIMS = os.path.join("src", "tileset_anims.c")
ANIM_INCGFX_RE = re.compile(
    r'INCGFX_U(8|16|32)\("(data/tilesets/[^"]*\.png)", "\.8bpp"(?:, "([^"]*)")?\)')


def full_bank_map(tdir, secondary):
    """tile-id (absolute) -> local bank, over all metatile references."""
    mt = find_metatiles(tdir)
    if not mt:
        return {}
    with open(mt, "rb") as f:
        vals = struct.unpack("<%dH" % (os.path.getsize(mt) // 2), f.read())
    lo, hi = (6, 12) if secondary else (0, 5)
    c = {}
    for v in vals:
        tid, pal = v & 0x3FF, (v >> 12) & 0xF
        if lo <= pal <= hi:
            c.setdefault(tid, Counter())[pal - lo] += 1
    return {t: cc.most_common(1)[0][0] for t, cc in c.items()}


def remap_anim_frames():
    text = open(TILESET_ANIMS).read()
    frame_png = dict(re.findall(
        r'const u16 (\w+)\[\]\s*=\s*INCGFX_U(?:8|16|32)\("([^"]+\.png)"', text))
    ptr_arrays = {}
    for m in re.finditer(r'const u16 \*const (\w+)\[\]\s*=\s*\{([^}]*)\};', text):
        ptr_arrays[m.group(1)] = [x for x in re.findall(r'\b(\w+)\b', m.group(2))
                                  if x in frame_png]

    def ev(expr):
        return eval(expr.replace("NUM_TILES_IN_PRIMARY", "512"), {"__builtins__": {}}, {})
    vdests = {}
    for m in re.finditer(r'u16 \*const (\w+_VDests)\[\]\s*=\s*\{(.*?)\};', text, re.DOTALL):
        vdests[m.group(1)] = [ev(e) for e in
                              re.findall(r'TILE_OFFSET_4BPP\(([^)]+)\)', m.group(2))]
    funcs = [(m.start(), m.group(1)) for m in
             re.finditer(r'static void (QueueAnimTiles_\w+)\s*\(', text)]

    def tset_of(pos):
        name = None
        for s, n in funcs:
            if s <= pos:
                name = n
            else:
                break
        if not name:
            return None
        tok = name[len("QueueAnimTiles_"):].split("_")[0]
        return re.sub(r'(?<!^)(?=[A-Z])', '_', tok).lower()

    count = 0
    for m in re.finditer(
            r'AppendTilesetAnimToBuffer\(\s*(\w+)\[[^\]]*\]\s*,\s*'
            r'(?:\(u16 \*\)\(BG_VRAM \+ TILE_OFFSET_4BPP\(([^)]+)\)\)|(\w+_VDests)\[[^\]]*\])'
            r'\s*,\s*(\d+) \* TILE_SIZE_4BPP\)', text):
        arr, destexpr, vd, _cnt = m.groups()
        dest = ev(destexpr) if destexpr else vdests[vd][0]
        tset = tset_of(m.start())
        if not tset:
            continue
        secondary = dest >= 512
        tdir = os.path.join(TILESETS, "secondary" if secondary else "primary", tset)
        if not os.path.isdir(tdir):
            continue
        palette = read_pal_file(os.path.join(tdir, "palette.pal"))
        bm = full_bank_map(tdir, secondary)
        for frame in ptr_arrays.get(arr, []):
            png = frame_png[frame]
            if ("/%s/" % tset) not in png.replace("\\", "/"):
                continue
            banks = {}
            im = Image.open(png)
            tw = im.size[0] // 8
            for j in range((im.size[1] // 8) * tw):
                banks[j] = bm.get(dest + j, 0)
            reencode_png(png, palette, banks)
            count += 1
    print("Anim frames remapped: %d" % count)


def read_pal_file(pp):
    """Read a JASC palette.pal back into a list of (r,g,b)."""
    lines = [l.rstrip("\r\n") for l in open(pp, newline="")]
    n = int(lines[2])
    return [tuple(int(x) for x in lines[3 + i].split()) for i in range(n)]


def main():
    if not os.path.isdir(TILESETS):
        raise SystemExit("run from repo root")
    convert_data()
    remap_anim_frames()
    rewrite_c(os.path.join("src", "graphics.c"))
    rewrite_c(os.path.join("src", "data", "tilesets", "graphics.h"))
    # anim frames are absolute too -> reduce modulo 16 at build time
    anim = open(TILESET_ANIMS).read()
    new = ANIM_INCGFX_RE.sub(_mod_repl, anim)
    if new != anim:
        with open(TILESET_ANIMS, "w") as f:
            f.write(new)
        print("Rewrote %s" % TILESET_ANIMS)


def _mod_repl(m):
    u, path, args = m.group(1), m.group(2), m.group(3)
    if args and "-palette_mod" in args:
        return m.group(0)
    args = (args + " " if args else "") + "-palette_mod 16"
    return 'INCGFX_U%s("%s", ".8bpp", "%s")' % (u, path, args)


if __name__ == "__main__":
    main()
