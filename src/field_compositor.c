#include "global.h"
#include "field_compositor.h"
#include "fieldmap.h"
#include "malloc.h"

// Software tile compositor backing the overworld renderer. See field_compositor.h for the
// high-level design. This file owns:
//   1. Pointers to the resident tilesets' 4BPP graphics, read straight from ROM. Field tilesets
//      are stored uncompressed for this, so no 32 KB EWRAM copy is needed and the 8BPP composite
//      output gets char blocks 0-1 to itself (a full 512-slot pool).
//   2. A small writable override for animated tiles only: ROM is immutable, so the tileset
//      animation system writes each frame's animated tiles here and the compositor reads them from
//      here instead of ROM.
//   3. The composite slot pool (sPool) over the 512 8BPP tiles in char blocks 0-1, each holding the
//      recipe (the metatile sub-tile entries it was built from) and a refcount.
//   4. A bucket-chained hash so identical recipes share one VRAM slot (heavy real-world tile reuse
//      keeps the working set well under the pool size).
// The pool/hash/free-list/override tables are heap-owned (~15 KB) and allocated on field init.

#define SUBTILE_ENTRY_TILE_MASK 0x03FF
#define SUBTILE_ENTRY_HFLIP      0x0400
#define SUBTILE_ENTRY_VFLIP      0x0800
#define SUBTILE_ENTRY_PAL_SHIFT  12

#define BUCKET_EMPTY 0xFFFF

// Animated source tiles change every 16 frames; recompositing every affected on-screen slot in
// that one frame is a periodic spike. Instead the slots are marked dirty and recomposited over the
// following frames, this many per frame (drains a water spike in a few frames, well before the next
// update at +16 frames, so the animation never visibly falls behind).
#define COMPOSITE_ANIM_RECOMPOSE_BUDGET 12

// Animated-tile override: separate primary/secondary regions so a secondary-tileset swap (location
// crossing) can be reset without disturbing the primary's animations. Each region holds the distinct
// animated tile ids of one tileset (water is the largest at ~30 tiles).
#define ANIM_PRIMARY_BASE      0
#define ANIM_PRIMARY_CAP       64
#define ANIM_SECONDARY_BASE    64
#define ANIM_SECONDARY_CAP     64
// Door animation frames are injected (by field_door.c) at reserved tile ids at the very top of the
// secondary range. They get a dedicated, fixed override region instead of drawing from the shared
// secondary counter: a tileset with many animated secondary tiles could otherwise exhaust that
// counter, leaving the door tiles un-overridden so they read past the tileset in ROM and rendered
// garbage on the first door of such a map. Direct-mapped (tile id -> slot), so it never overflows.
#define ANIM_DOOR_TILE_COUNT   16
#define ANIM_DOOR_BASE         (ANIM_SECONDARY_BASE + ANIM_SECONDARY_CAP)
#define ANIM_DOOR_FIRST_TILE   (NUM_TILES_TOTAL - ANIM_DOOR_TILE_COUNT)
#define ANIM_TILE_CAPACITY     (ANIM_DOOR_BASE + ANIM_DOOR_TILE_COUNT)
#define ANIM_NOT_OVERRIDDEN    0xFF // max override index is 143, so a byte index fits with room to spare

struct CompositeSlot
{
    // Normalized recipe: non-zero sub-tile entries, left-packed (entries[1] set implies entries[0]
    // set). The layer count is therefore derivable (SlotCount), so it isn't stored.
    u16 entries[COMPOSITE_MAX_LAYERS];
    u16 refcount;
    u16 nextInBucket; // BUCKET_EMPTY-terminated singly linked list within its hash bucket
};

static inline u32 SlotCount(const struct CompositeSlot *s)
{
    return s->entries[2] ? 3 : (s->entries[1] ? 2 : (s->entries[0] ? 1 : 0));
}

static const u8 *sPrimaryTiles;   // ROM, uncompressed 8BPP; tile ids 0..NUM_TILES_IN_PRIMARY-1
static const u8 *sSecondaryTiles; // ROM, uncompressed 8BPP; tile ids NUM_TILES_IN_PRIMARY..
static u8 *sAnimTiles;             // [ANIM_TILE_CAPACITY * TILE_SIZE_8BPP], heap
// Per-bank base offset added to each non-zero source pixel (see ComposeSlot). Indexed by the
// metatile subtile entry's palette field. Banks 0..NUM_PALS_IN_PRIMARY-1 come from the primary
// tileset, NUM_PALS_IN_PRIMARY..NUM_PALS_TOTAL-1 from the secondary; default = bank*16.
static u8 sBankOffset[16];
static u8 *sAnimOverrideIndex;     // [NUM_TILES_TOTAL]; ANIM_NOT_OVERRIDDEN or index into sAnimTiles
static u16 sAnimPrimaryCount;      // next free override slot in the primary region
static u16 sAnimSecondaryCount;    // next free override slot in the secondary region (from BASE)
static struct CompositeSlot *sPool; // [COMPOSITE_SLOT_COUNT]
static u16 *sHashHead; // [COMPOSITE_HASH_SIZE]
static u16 *sFreeList; // [COMPOSITE_SLOT_COUNT]
static u16 sFreeCount;
// Slots whose refcount hit zero THIS frame. They can't be re-handed-out until next frame: a freed
// slot's tilemap cell only reaches VRAM at the coming VBlank (the copy is queued, see
// DoScheduledBgTilemapCopiesToVram), but ComposeSlot rewrites a slot's pixels the instant it's reused.
// Reusing a slot the same frame it was freed would overwrite the pixels a still-displayed cell points
// at, flashing one frame of garbage. Drained into the free list once per frame (FieldCompositorReclaimFreedSlots).
static u16 *sPendingFree; // [COMPOSITE_SLOT_COUNT]
static u16 sPendingFreeCount;
static bool8 sActive;

// Slots awaiting an animation recomposite (see COMPOSITE_ANIM_RECOMPOSE_BUDGET). A bitset keyed by
// slot index, drained a few per frame by FieldCompositorTickAnim. sDirtyCursor rings through the
// pool so the drain is fair; sDirtyCount lets the drain early-out when empty.
static u32 sDirtyBits[(COMPOSITE_SLOT_COUNT + 31) / 32];
static u16 sDirtyCount;
static u16 sDirtyCursor;

static void MarkSlotDirty(u16 slot)
{
    u32 mask = 1u << (slot & 31);

    if (!(sDirtyBits[slot >> 5] & mask))
    {
        sDirtyBits[slot >> 5] |= mask;
        sDirtyCount++;
    }
}

static void ClearSlotDirty(u16 slot)
{
    u32 mask = 1u << (slot & 31);

    if (sDirtyBits[slot >> 5] & mask)
    {
        sDirtyBits[slot >> 5] &= ~mask;
        sDirtyCount--;
    }
}

static void RecomposeSourceRange(u16 firstTileId, u16 lastTileId, bool32 deferred);

// Source pixels for a tile id: the animated override if one exists, else straight from the
// uncompressed tileset in ROM. NULL if the owning tileset isn't resident yet.
static const u8 *GetSourceTilePtr(u16 tileId)
{
    u16 ov = sAnimOverrideIndex[tileId];

    if (ov != ANIM_NOT_OVERRIDDEN)
        return sAnimTiles + ov * TILE_SIZE_8BPP;
    // Reserved door tile ids have no ROM backing (they only exist via the door override); reading them
    // from the tileset would run past its graphics. Treat un-overridden ones as transparent.
    if (tileId >= ANIM_DOOR_FIRST_TILE)
        return NULL;
    if (tileId < NUM_TILES_IN_PRIMARY)
        return sPrimaryTiles ? sPrimaryTiles + tileId * TILE_SIZE_8BPP : NULL;
    return sSecondaryTiles ? sSecondaryTiles + (tileId - NUM_TILES_IN_PRIMARY) * TILE_SIZE_8BPP : NULL;
}

// Drop the animation overrides for a tile-id range (a tileset is being swapped in there). The new
// tileset's animation init re-populates them on its next frame.
static void ClearAnimOverrides(u16 firstTileId, u16 lastTileId, bool32 primary)
{
    u32 t;

    for (t = firstTileId; t <= lastTileId; t++)
        sAnimOverrideIndex[t] = ANIM_NOT_OVERRIDDEN;
    if (primary)
        sAnimPrimaryCount = 0;
    else
        sAnimSecondaryCount = 0;
}

bool8 IsFieldCompositorActive(void)
{
    return sActive;
}

static void ResetPool(void)
{
    u32 i;

    for (i = 0; i < COMPOSITE_HASH_SIZE; i++)
        sHashHead[i] = BUCKET_EMPTY;
    for (i = 0; i < COMPOSITE_SLOT_COUNT; i++)
    {
        u32 j;
        for (j = 0; j < COMPOSITE_MAX_LAYERS; j++)
            sPool[i].entries[j] = 0;
        sPool[i].refcount = 0;
        sPool[i].nextInBucket = BUCKET_EMPTY;
    }
    for (i = 0; i < ARRAY_COUNT(sDirtyBits); i++)
        sDirtyBits[i] = 0;
    sDirtyCount = 0;
    sDirtyCursor = 1; // slot 0 is the permanent blank tile, never recomposited
    // Slot 0 is the permanent blank tile; never allocated to a recipe. Hand out 1..N-1.
    sFreeCount = 0;
    for (i = COMPOSITE_SLOT_COUNT - 1; i >= 1; i--)
        sFreeList[sFreeCount++] = i;
    sPendingFreeCount = 0;
}

void FieldCompositorInit(void)
{
    u32 i;

    // Allocate fresh every map init, like the BG tilemap buffers in InitOverworldBgs. A full map
    // load (warp) resets the whole heap (MoveSaveBlocks_ResetHeap) WITHOUT calling
    // FieldCompositorFree, so these statics are left non-NULL but dangling. Don't NULL-guard (it
    // would skip the realloc and we'd use freed heap) and don't free them here (freeing a dangling
    // pointer would corrupt the allocator) - just claim new blocks; the heap reset reclaimed the old.
    sPool = AllocZeroed(COMPOSITE_SLOT_COUNT * sizeof(struct CompositeSlot));
    sHashHead = AllocZeroed(COMPOSITE_HASH_SIZE * sizeof(u16));
    sFreeList = AllocZeroed(COMPOSITE_SLOT_COUNT * sizeof(u16));
    sPendingFree = AllocZeroed(COMPOSITE_SLOT_COUNT * sizeof(u16));
    sAnimTiles = AllocZeroed(ANIM_TILE_CAPACITY * TILE_SIZE_8BPP);
    sAnimOverrideIndex = AllocZeroed(NUM_TILES_TOTAL * sizeof(u8));
    for (i = 0; i < NUM_TILES_TOTAL; i++)
        sAnimOverrideIndex[i] = ANIM_NOT_OVERRIDDEN;
    for (i = 0; i < ARRAY_COUNT(sBankOffset); i++)
        sBankOffset[i] = i * 16;
    sAnimPrimaryCount = 0;
    sAnimSecondaryCount = 0;
    ResetPool();
    // Zero the blank composite tile in VRAM so empty foreground cells (slot 0) are transparent.
    DmaFill32(3, 0, (void *)(BG_VRAM + COMPOSITE_BLANK_SLOT * TILE_SIZE_8BPP), TILE_SIZE_8BPP);
    sActive = TRUE;
}

void FieldCompositorFree(void)
{
    sActive = FALSE;
    sPrimaryTiles = NULL;
    sSecondaryTiles = NULL;
    TRY_FREE_AND_SET_NULL(sPool);
    TRY_FREE_AND_SET_NULL(sHashHead);
    TRY_FREE_AND_SET_NULL(sFreeList);
    TRY_FREE_AND_SET_NULL(sPendingFree);
    TRY_FREE_AND_SET_NULL(sAnimTiles);
    TRY_FREE_AND_SET_NULL(sAnimOverrideIndex);
}

// Point the compositor at a tileset's (uncompressed, ROM-resident) 4BPP graphics. A location
// crossing can swap a different secondary tileset into the same id range while composites built from
// the old graphics are still live, so clear that range's animation overrides and recomposite the
// live slots from the new ROM source. Both are no-ops on first load (nothing composited yet).
// Cache a tileset's bank base-offsets into sBankOffset for the banks it owns. A NULL
// paletteOffsets table (the common case) means default banks (base = bank*16).
static void LoadBankOffsets(const struct Tileset *tileset, u32 firstBank, u32 lastBank)
{
    const u8 *po = tileset->paletteOffsets;
    u32 i;

    for (i = firstBank; i <= lastBank; i++)
        sBankOffset[i] = po ? po[i] : i * 16;
}

void FieldCompositorLoadPrimaryTiles(const struct Tileset *tileset)
{
    if (tileset == NULL)
        return;
    sPrimaryTiles = (const u8 *)tileset->tiles;
    LoadBankOffsets(tileset, 0, NUM_PALS_IN_PRIMARY - 1);
    ClearAnimOverrides(0, NUM_TILES_IN_PRIMARY - 1, TRUE);
    FieldCompositorInvalidateSourceRange(0, NUM_TILES_IN_PRIMARY - 1);
}

void FieldCompositorLoadSecondaryTiles(const struct Tileset *tileset)
{
    if (tileset == NULL)
        return;
    sSecondaryTiles = (const u8 *)tileset->tiles;
    LoadBankOffsets(tileset, NUM_PALS_IN_PRIMARY, NUM_PALS_TOTAL - 1);
    ClearAnimOverrides(NUM_TILES_IN_PRIMARY, NUM_TILES_TOTAL - 1, FALSE);
    FieldCompositorInvalidateSourceRange(NUM_TILES_IN_PRIMARY, NUM_TILES_TOTAL - 1);
}

// Flatten a slot's recipe (bottom->top) into one 8BPP tile and DMA it to VRAM. The 8BPP source
// pixels are relative indices; each non-zero one becomes (pix + sBankOffset[palField]) against the
// unified 256-colour overworld palette. For legacy 16-colour art (pix 0..15, base a multiple of
// 16) this is identical to the old (palBank << 4) | pix. Zero stays transparent so upper layers
// show the layers below.
static void ComposeSlot(u16 slot)
{
    ALIGNED(4) u8 out[TILE_SIZE_8BPP];
    struct CompositeSlot *s = &sPool[slot];
    u32 layer, row, b;

    for (row = 0; row < TILE_SIZE_8BPP / 4; row++)
        ((u32 *)out)[row] = 0;

    for (layer = 0; layer < SlotCount(s); layer++)
    {
        u16 e = s->entries[layer];
        const u8 *src = GetSourceTilePtr(e & SUBTILE_ENTRY_TILE_MASK);
        u32 palBase = sBankOffset[(e >> SUBTILE_ENTRY_PAL_SHIFT) & 0xF];
        bool32 hflip = e & SUBTILE_ENTRY_HFLIP;
        bool32 vflip = e & SUBTILE_ENTRY_VFLIP;

        if (src == NULL)
            continue;
        // Each source byte is one 8BPP pixel. Specialise on hflip so the per-pixel flip branch
        // leaves the inner loop; vflip just picks the source row.
        for (row = 0; row < 8; row++)
        {
            const u8 *srcRow = src + (vflip ? 7 - row : row) * 8;
            u8 *o = out + row * 8;

            if (!hflip)
            {
                for (b = 0; b < 8; b++)
                {
                    u8 pix = srcRow[b];
                    if (pix) o[b] = pix + palBase;
                }
            }
            else
            {
                for (b = 0; b < 8; b++)
                {
                    u8 pix = srcRow[7 - b];
                    if (pix) o[b] = pix + palBase;
                }
            }
        }
    }

    DmaCopy32(3, out, (void *)(BG_VRAM + slot * TILE_SIZE_8BPP), TILE_SIZE_8BPP);
}

static u32 HashRecipe(const u16 *entries, u32 count)
{
    u32 h = count;
    u32 i;

    for (i = 0; i < count; i++)
        h = h * 33 + entries[i];
    return h & (COMPOSITE_HASH_SIZE - 1);
}

static bool32 RecipeMatches(const struct CompositeSlot *s, const u16 *entries, u32 count)
{
    u32 i;

    if (SlotCount(s) != count)
        return FALSE;
    for (i = 0; i < count; i++)
        if (s->entries[i] != entries[i])
            return FALSE;
    return TRUE;
}

u16 FieldCompositorAcquire(const u16 *entries, u32 count)
{
    u16 norm[COMPOSITE_MAX_LAYERS];
    u32 n = 0;
    u32 bucket;
    u16 slot;
    u32 i;

    // Drop transparent (tile id 0) entries; an empty stack is the shared blank tile.
    for (i = 0; i < count; i++)
    {
        if ((entries[i] & SUBTILE_ENTRY_TILE_MASK) != 0 && n < COMPOSITE_MAX_LAYERS)
            norm[n++] = entries[i];
    }
    if (n == 0)
        return COMPOSITE_BLANK_SLOT;

    bucket = HashRecipe(norm, n);
    for (slot = sHashHead[bucket]; slot != BUCKET_EMPTY; slot = sPool[slot].nextInBucket)
    {
        if (RecipeMatches(&sPool[slot], norm, n))
        {
            sPool[slot].refcount++;
            return slot;
        }
    }

    if (sFreeCount == 0)
    {
        // Pool exhausted (scene needs >COMPOSITE_SLOT_COUNT unique tiles this frame). Tell the
        // caller to keep whatever tile the cell already shows rather than blanking it - a brief
        // stale tile reads far better than the flash a blank cell produces, and the cell is retried
        // every redraw so it self-corrects as soon as a slot frees.
        DebugPrintf("FieldCompositor: slot pool exhausted");
        return COMPOSITE_SLOT_KEEP;
    }

    slot = sFreeList[--sFreeCount];
    // Left-pack the recipe and zero the unused layers so SlotCount reads back n.
    for (i = 0; i < COMPOSITE_MAX_LAYERS; i++)
        sPool[slot].entries[i] = (i < n) ? norm[i] : 0;
    sPool[slot].refcount = 1;
    sPool[slot].nextInBucket = sHashHead[bucket];
    sHashHead[bucket] = slot;
    ComposeSlot(slot);
    return slot;
}

void FieldCompositorRelease(u16 slot)
{
    u32 bucket, i;
    u16 cur, prev;

    if (slot == COMPOSITE_BLANK_SLOT || slot >= COMPOSITE_SLOT_COUNT)
        return;
    if (sPool[slot].refcount == 0)
        return;
    if (--sPool[slot].refcount != 0)
        return;

    // Refcount hit zero: unlink from its hash bucket and return the slot to the free list.
    bucket = HashRecipe(sPool[slot].entries, SlotCount(&sPool[slot]));
    prev = BUCKET_EMPTY;
    for (cur = sHashHead[bucket]; cur != BUCKET_EMPTY; cur = sPool[cur].nextInBucket)
    {
        if (cur == slot)
        {
            if (prev == BUCKET_EMPTY)
                sHashHead[bucket] = sPool[slot].nextInBucket;
            else
                sPool[prev].nextInBucket = sPool[slot].nextInBucket;
            break;
        }
        prev = cur;
    }
    for (i = 0; i < COMPOSITE_MAX_LAYERS; i++)
        sPool[slot].entries[i] = 0;
    ClearSlotDirty(slot);
    // Hold the slot until next frame (see sPendingFree) so its pixels survive one frame for the
    // still-displayed tilemap cell that referenced it before this redraw's VRAM copy lands.
    sPendingFree[sPendingFreeCount++] = slot;
}

// Return this frame's freed slots to the allocatable free list. Call once per frame BEFORE any cell
// redraws (so slots freed during the frame stay out of circulation until the next frame), and after
// the previous frame's tilemap copy has reached VRAM. See sPendingFree for why the one-frame hold is
// needed. Total slots across live/free/pending never exceeds the pool, so the free list can't overflow.
void FieldCompositorReclaimFreedSlots(void)
{
    if (!sActive)
        return;
    while (sPendingFreeCount != 0)
        sFreeList[sFreeCount++] = sPendingFree[--sPendingFreeCount];
}

// Assign (or reuse) an override slot for an animated tile id and return a pointer to it, or NULL if
// that tileset's override region is full. Allocation is sticky: the same animated tile ids recur
// every frame, so a region fills once per tileset and is reset on tileset swap.
static u8 *AcquireAnimOverride(u16 tileId)
{
    u16 idx = sAnimOverrideIndex[tileId];

    if (idx == ANIM_NOT_OVERRIDDEN)
    {
        if (tileId >= ANIM_DOOR_FIRST_TILE)
        {
            // Reserved door tile id: fixed slot, independent of the shared counters (never overflows).
            idx = ANIM_DOOR_BASE + (tileId - ANIM_DOOR_FIRST_TILE);
        }
        else if (tileId < NUM_TILES_IN_PRIMARY)
        {
            if (sAnimPrimaryCount >= ANIM_PRIMARY_CAP)
                return NULL;
            idx = ANIM_PRIMARY_BASE + sAnimPrimaryCount++;
        }
        else
        {
            if (sAnimSecondaryCount >= ANIM_SECONDARY_CAP)
                return NULL;
            idx = ANIM_SECONDARY_BASE + sAnimSecondaryCount++;
        }
        sAnimOverrideIndex[tileId] = idx;
    }
    return sAnimTiles + idx * TILE_SIZE_8BPP;
}

// ROM source is immutable, so dynamic tile graphics (animation frames, door frames) land in the
// writable override buffer the compositor reads from.
static void WriteSourceOverride(u16 firstTileId, const void *src, u16 numTiles)
{
    const u8 *in = src;
    u32 k;

    for (k = 0; k < numTiles; k++)
    {
        u8 *dest = AcquireAnimOverride(firstTileId + k);
        if (dest != NULL)
            CpuFastCopy(in + k * TILE_SIZE_8BPP, dest, TILE_SIZE_8BPP);
    }
}

void FieldCompositorUpdateSourceTiles(u16 firstTileId, const void *src, u16 numTiles)
{
    if (!sActive || sAnimTiles == NULL || numTiles == 0)
        return;
    WriteSourceOverride(firstTileId, src, numTiles);
    // Defer the recomposite: animations fire every frame, so spread the work (see TickAnim) instead
    // of recompositing the whole affected set in this one frame.
    RecomposeSourceRange(firstTileId, firstTileId + numTiles - 1, TRUE);
}

// Like FieldCompositorUpdateSourceTiles but recomposites immediately. Used by the door animation:
// it's only a handful of cells, played as a deliberate short sequence, so it should update the same
// frame rather than drip in over the next few.
void FieldCompositorUpdateSourceTilesImmediate(u16 firstTileId, const void *src, u16 numTiles)
{
    if (!sActive || sAnimTiles == NULL || numTiles == 0)
        return;
    WriteSourceOverride(firstTileId, src, numTiles);
    RecomposeSourceRange(firstTileId, firstTileId + numTiles - 1, FALSE);
}

// Recomposite (or, when deferred, mark dirty) every live slot whose recipe references a source tile
// in [firstTileId, lastTileId].
static void RecomposeSourceRange(u16 firstTileId, u16 lastTileId, bool32 deferred)
{
    u16 slot;
    u32 i;

    if (sPool == NULL)
        return;
    for (slot = 1; slot < COMPOSITE_SLOT_COUNT; slot++)
    {
        if (sPool[slot].refcount == 0)
            continue;
        for (i = 0; i < SlotCount(&sPool[slot]); i++)
        {
            u16 tileId = sPool[slot].entries[i] & SUBTILE_ENTRY_TILE_MASK;
            if (tileId >= firstTileId && tileId <= lastTileId)
            {
                if (deferred)
                    MarkSlotDirty(slot);
                else
                    ComposeSlot(slot);
                break;
            }
        }
    }
}

// Immediate recomposite, used on a tileset swap (location crossing) where the new graphics must be
// on screen right away rather than fading in over the next few frames.
void FieldCompositorInvalidateSourceRange(u16 firstTileId, u16 lastTileId)
{
    RecomposeSourceRange(firstTileId, lastTileId, FALSE);
}

// Drains a bounded number of animation-dirtied slots per frame (call once per frame). The cursor
// rings through the pool so no slot is starved. A slot freed/reused before its turn had its dirty
// bit cleared, so only currently-live recipes are recomposited.
void FieldCompositorTickAnim(void)
{
    u32 budget = COMPOSITE_ANIM_RECOMPOSE_BUDGET;
    u32 scanned = 0;

    if (!sActive || sDirtyCount == 0)
        return;
    while (budget != 0 && sDirtyCount != 0 && scanned < COMPOSITE_SLOT_COUNT)
    {
        u16 slot = sDirtyCursor;

        if (++sDirtyCursor >= COMPOSITE_SLOT_COUNT)
            sDirtyCursor = 1;
        scanned++;
        if (sDirtyBits[slot >> 5] & (1u << (slot & 31)))
        {
            if (sPool[slot].refcount != 0)
                ComposeSlot(slot);
            ClearSlotDirty(slot);
            budget--;
        }
    }
}
