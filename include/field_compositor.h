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
// plane (see METATILE_COMPOSITE_*). A bgMaterial tile adds a 4th, background-only layer (the ground
// material composites beneath the tile's own three layers), so the background plane can need four.
#define COMPOSITE_MAX_LAYERS 4

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

// Open / close a reband window around a full-view re-band (see RebandPhasedMapView). Begin pins and indexes
// every on-screen banked phased slot so each per-cell FieldCompositorRebandSlot is a direct table read (no
// hash probe) and no sibling is freed/recreated mid-walk; End drops the pins. Must be paired.
void FieldCompositorBeginReband(void);
void FieldCompositorEndReband(void);

// Re-band one on-screen phased cell for the current phase gradient. Given the slot a tilemap cell currently
// points at and that cell's map (x, y), returns the sibling slot for the cell's new band (refcount ++'d), or
// COMPOSITE_SLOT_KEEP when nothing changes (blank/non-phased cell, or pool full - leave the cell as-is). The
// caller releases the old slot and writes the returned one. Re-points only among the recipe's existing
// band-slots, so a wind-heading change re-bands the water surface without a whole-view metatile redraw.
u16 FieldCompositorRebandSlot(u16 slot, s16 x, s16 y);

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
// `frameCount` pointers, each to `tileCount` 8BPP tiles in ROM; `phaseFn(x, y)` returns a raw SIGNED
// phase (floor-modded by `bandCount`). The chosen band is folded into the composite slot key, so cells in
// the same band share a slot - on-screen slots (and per-column compose cost while scrolling) stay
// bounded by `bandCount` per recipe. `bandCount` is the perf/quality knob: fewer bands = cheaper but
// a coarser wave; pass frameCount for the finest wave, or a divisor of it (e.g. frameCount/2) to
// halve the cost seamlessly.
void FieldCompositorRegisterPhasedGroup(u32 index, u16 firstTileId, u8 tileCount, const u16 *const *frames, u8 frameCount, u8 bandCount, s16 (*phaseFn)(s16 x, s16 y));

// Flag an already-registered group to animate on the fixed, wind-independent clock (steady 1/16) instead of
// the wind-coupled water step (see FieldCompositorTickPhased's fixedStep). Only sound when the group's tiles
// never share a composited slot with a wind-clock group; the terrain waterfall (overlays static cliff only)
// is the intended user.
void FieldCompositorSetPhasedGroupFixedClock(u32 index);

// Swap an already-registered group's frame set at runtime (frames/frameCount may change). Repairs the live
// banks that reference it and marks the affected slots dirty (recomposed gradually by FieldCompositorTickAnim).
// Register the group with a fixed bandCount > every set's frameCount so phases are swap-invariant and no
// re-phasing redraw is needed. See the .c comment. Used by the terrain water waves scaling with the wind.
void FieldCompositorReloadPhasedGroup(u32 index, u16 firstTileId, u8 tileCount, const u16 *const *frames, u8 frameCount, u8 bandCount, s16 (*phaseFn)(s16 x, s16 y));

// Drop all phased groups (call when a primary tileset that owns them is being swapped out; the new
// tileset re-registers its own). Phased groups live in primary-tileset tile-id space.
void FieldCompositorClearPhasedGroups(void);

// Shift the position-dependent phase's evaluation origin by a coordinate-frame delta, so the wave surface
// stays coherent when the map coordinate frame re-bases under the on-screen slots (a map-connection
// transition; see CameraMove). (frameDx, frameDy) is how much a physical tile's coordinate changed. Keeps
// baked slots and cells drawn after the crossing in phase with no full re-acquire.
void FieldCompositorShiftPhaseOrigin(s16 frameDx, s16 frameDy);

// Advance the phased animation and refresh every phased slot to its new frame; call once per frame. Two
// clocks: `windStep` drives the wind-coupled water groups, `fixedStep` (e.g. timer / 16) drives fixedClock
// groups (the waterfall) at a steady rate; each slot picks its clock by its recipe. Banked slots
// (pre-composited at slot creation) refresh via a cheap copy, so the surface flips together without the
// per-slot re-flatten spike. Only a real change to either step does work.
void FieldCompositorTickPhased(u16 windStep, u16 fixedStep);

#endif // GUARD_FIELD_COMPOSITOR_H
