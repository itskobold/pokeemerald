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
// resolves to COMPOSITE_BLANK_SLOT. Increments the slot's refcount.
u16 FieldCompositorAcquire(const u16 *entries, u32 count);

// Release a slot previously returned by FieldCompositorAcquire (no-op for the blank slot). The slot
// isn't reusable until the next FieldCompositorReclaimFreedSlots (a one-frame hold; see below).
void FieldCompositorRelease(u16 slot);

// Return slots freed during the previous frame to the allocatable pool. Must be called once per frame
// before any cell redraws: a slot's pixels are rewritten the moment it's reused, but the tilemap cell
// that pointed at it only reaches VRAM at VBlank, so a freed slot must survive one frame before reuse
// or a still-displayed cell flashes garbage. Without this, promotion flips (which free/reuse slots
// on-screen) tear for a frame.
void FieldCompositorReclaimFreedSlots(void);

// Mark every composite slot whose recipe references a source tile in [firstTileId, lastTileId]
// as needing a recomposite, and rebuild them from the (newly updated) source cache. Used by the
// tileset animation system after it writes a new frame into the source cache.
void FieldCompositorInvalidateSourceRange(u16 firstTileId, u16 lastTileId);

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

#endif // GUARD_FIELD_COMPOSITOR_H
