#ifndef GUARD_FIELD_COMPOSITOR_H
#define GUARD_FIELD_COMPOSITOR_H

#include "global.h"

// The overworld map is no longer drawn as three static 4BPP BG layers referencing a
// VRAM-resident tileset. Instead the visible area is composited, per 8x8 tile, into two
// 8BPP planes (foreground + background) built from a small pool of unique composite tiles.
// Source tileset graphics live decompressed in EWRAM; this module reads them, flattens the
// requested metatile sub-tile layers into 8BPP tiles (pixel = (palBank << 4) | pix4), and
// hands back a deduplicated, reference-counted VRAM slot index for the BG tilemaps to point at.

// 512 8BPP composite tiles fill char blocks 0-1 (512 * 64 = 0x8000 bytes). Slot 0 is a
// permanently-blank tile that empty foreground cells point at. Must not exceed 512: slot 512
// would start at BG_CHAR_ADDR(2) = 0x8000, which is BG0's character base (font/window tiles), so
// higher slots alias text VRAM and get overwritten by the map-name popup, menus, etc.
#define COMPOSITE_SLOT_COUNT 512 // char blocks 0-1
#define COMPOSITE_HASH_SIZE  512 // power of two, >= COMPOSITE_SLOT_COUNT
#define COMPOSITE_BLANK_SLOT 0
// Returned by FieldCompositorAcquire when the pool is full: the caller must keep the cell's current
// tile rather than write this value. Out of the 10-bit tilemap tile range, so never a real slot.
#define COMPOSITE_SLOT_KEEP  0xFFFF

// Maximum metatile sub-tile entries composited into one output tile. A metatile has three layers
// (ground/middle/top) and the per-metatile compositing flags can route all three into the same
// plane (see METATILE_COMPOSITE_*), so a single plane's cell can need all three.
#define COMPOSITE_MAX_LAYERS 3

void FieldCompositorInit(void);
void FieldCompositorFree(void);
bool8 IsFieldCompositorActive(void);

// Fill the source-tile cache from a tileset's (possibly compressed) 4BPP tile graphics.
// Primary occupies absolute tile ids 0..NUM_TILES_IN_PRIMARY-1, secondary the rest.
void FieldCompositorLoadPrimaryTiles(const struct Tileset *tileset);
void FieldCompositorLoadSecondaryTiles(const struct Tileset *tileset);

// Resolve a composite VRAM slot for an ordered (bottom->top) stack of metatile sub-tile
// entries. Entries whose tile id is 0 are treated as transparent and skipped; an empty stack
// resolves to COMPOSITE_BLANK_SLOT. Increments the slot's refcount. (x, y) is the cell's map
// position, used to pick the animation band when the recipe contains a phased tile (see below).
u16 FieldCompositorAcquire(const u16 *entries, u32 count, s16 x, s16 y);

// Like FieldCompositorAcquire, but after compositing the `draw` stack it punches holes: every pixel
// where any `mask` entry is opaque is forced transparent in the result. Used for the BG2 background
// composite of cells that have a reflection layer, so the reflective (BG3) surface shows through the
// masked-out region. With maskCount 0 this is identical to FieldCompositorAcquire. An empty draw
// stack resolves to COMPOSITE_BLANK_SLOT regardless of the mask.
u16 FieldCompositorAcquireMasked(const u16 *draw, u32 drawCount, const u16 *mask, u32 maskCount, s16 x, s16 y);

// Release a slot previously returned by FieldCompositorAcquire (no-op for the blank slot). The slot
// isn't reusable until the next FieldCompositorReclaimFreedSlots (a one-frame hold; see below).
void FieldCompositorRelease(u16 slot);

// Return slots freed during the previous frame to the allocatable pool. Must be called once per frame
// before any cell redraws: a slot's pixels are rewritten the moment it's reused, but the tilemap cell
// that pointed at it only reaches VRAM at VBlank, so a freed slot must survive one frame before reuse
// or a still-displayed cell flashes garbage. Without this, promotion flips (which free/reuse slots
// on-screen) tear for a frame.
void FieldCompositorReclaimFreedSlots(void);

// Write `numTiles` 4BPP tiles of new graphics into the source cache at `firstTileId`, then
// recomposite any on-screen tile that uses them. The tileset animation system calls this with a
// frame's worth of animated tiles in place of its old DMA-to-VRAM. No-op if the compositor is idle.
void FieldCompositorUpdateSourceTiles(u16 firstTileId, const void *src, u16 numTiles);

// As above, but recomposites the affected slots immediately instead of over the next frames. For
// short, deliberate sequences (door animations) that must appear the same frame they're drawn.
void FieldCompositorUpdateSourceTilesImmediate(u16 firstTileId, const void *src, u16 numTiles);

// Recomposite a bounded number of animation-dirtied slots. Call once per frame: animation source
// updates only mark affected slots dirty (cheap); this spreads the actual recompositing out so a
// frame where many tiles animate at once doesn't spike.
void FieldCompositorTickAnim(void);

// Phased tileset animation: a group is a run of `tileCount` animated source tile ids (starting at
// `firstTileId`) whose displayed frame is chosen per cell from the cell's map position, so the
// animation travels across the map instead of every cell showing the same frame. `frames` holds
// `frameCount` pointers, each to `tileCount` 8BPP tiles in ROM; `phaseFn(x, y)` returns a raw band
// index (taken mod `bandCount`). The chosen band is folded into the composite slot key, so cells in
// the same band share a slot - on-screen slots (and per-column compose cost while scrolling) stay
// bounded by `bandCount` per recipe. `bandCount` is the perf/quality knob: fewer bands = cheaper but
// a coarser wave; pass frameCount for the finest wave, or a divisor of it (e.g. frameCount/2) to
// halve the cost seamlessly.
void FieldCompositorRegisterPhasedGroup(u32 index, u16 firstTileId, u8 tileCount, const u16 *const *frames, u8 frameCount, u8 bandCount, u8 (*phaseFn)(s16 x, s16 y));

// Drop all phased groups (call when a primary tileset that owns them is being swapped out; the new
// tileset re-registers its own). Phased groups live in primary-tileset tile-id space.
void FieldCompositorClearPhasedGroups(void);

// Advance the phased (water) animation to `step` (e.g. timer / 16) and refresh every phased slot to its
// new frame; call once per frame. Banked slots (pre-composited at slot creation) refresh via a cheap
// copy, so the whole surface flips together each step without the per-slot re-flatten spike. Only a real
// step change does work.
void FieldCompositorTickPhased(u16 step);

#endif // GUARD_FIELD_COMPOSITOR_H
