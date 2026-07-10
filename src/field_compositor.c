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
    // set). The layer count is therefore derivable (SlotCount), so it isn't stored. Draw entries are
    // packed first, then any mask entries; maskBits flags which packed entries are masks (hole-punch,
    // see ComposeSlot) rather than drawn. maskBits 0 is an ordinary all-draw recipe.
    u16 entries[COMPOSITE_MAX_LAYERS];
    u8 maskBits;
    // Animation band (see PhasedAnimGroup): the phase the recipe's phased tile is composed at, so two
    // cells in different bands don't dedup to one slot. 0 for recipes with no phased tile, so ordinary
    // tiles dedup exactly as before. Part of the recipe key (hash + match).
    u8 phase;
    // Pre-composited frame bank this slot draws from (see struct FrameBank). BANK_NONE for ordinary
    // (non-phased) slots, which flatten live; BANK_UNBANKED for phased slots that couldn't get a bank
    // (pool/heap full), which also flatten live; else an index into sBanks and the slot's animation is a
    // cheap per-step copy from that bank instead of a re-flatten.
    u8 bank;
    // TRUE if this slot's phased group runs on the fixed clock (see PhasedAnimGroup.fixedClock). Copied
    // from the recipe at acquire so ComposeSlot picks sPhasedStepFixed vs sPhasedStep with one field read.
    u8 fixedClock;
    u16 refcount;
    u16 nextInBucket; // BUCKET_EMPTY-terminated singly linked list within its hash bucket
};

// A pre-composited animation for one phased recipe. Water metatiles are 2 phased layers (a water base +
// a wave overlay) flattened into one tile; re-flattening every on-screen water slot each time the
// animation frame advances is the recompose spike. Instead, the first slot of a given recipe flattens
// all `period` frames of that recipe ONCE into `sBankTiles`, and every step advance is then just a
// 64-byte copy of the current frame into each slot's VRAM. Banks are keyed by (entries, maskBits) - the
// recipe WITHOUT its band/phase, since all bands of a recipe share the same frame set and differ only in
// which frame index they display - so all 12 bands of a water surface share one bank.
// Upper bound on banks (distinct phased recipes cached at once); the actual pool (sBankCount) shrinks to
// fit the heap. Sized to cover the terrain tileset's water recipes (base water type x subtile x wave/
// reflection variant) with margin. THIS is the coverage knob: if the pool-full diagnostic fires, more
// distinct water recipes are on screen than fit, and the overflow flattens live each step (the update
// spike) - raise this (costs COMPOSITE_BANK_FRAMES * 64 = 768 B of heap each). Add-more-animations grows
// the recipe count, so bump it alongside new terrain animations.
#define COMPOSITE_BANK_MAX    64 // raised from 48: the pool-full diagnostic fired on the big water maps
#define COMPOSITE_BANK_FRAMES 12 // frames stored per bank; a recipe's period (lcm of its groups' frame counts) must fit
#define BANK_BUILD_BUDGET     8  // bank frames flattened per frame in the background, to spread build cost off the entry frame
#define BANK_NONE     0xFF       // slot has no phased tile
#define BANK_UNBANKED 0xFE       // slot is phased but unbanked (pool/heap full or period too long): flatten live

struct FrameBank
{
    u16 entries[COMPOSITE_MAX_LAYERS];
    u8 maskBits;
    u8 period;      // distinct frames in the animation = lcm of the recipe's phased groups' frame counts
    u16 builtMask;  // bit m set once frame m has been flattened into the bank (frames build lazily, see below)
    u16 rebuildMask; // bit m set when frame m's source changed and needs re-flattening, but its old content
                     // stays displayable until BankBuildTick replaces it (the live wave-set swap path)
    u16 refcount;   // live slots (across all bands) using this bank; 0 = free for reuse
};

// A run of animated source tile ids whose *displayed* frame is chosen per cell from that cell's map
// position (see PhaseForRecipe), so the animation travels across the map instead of every cell showing
// the same frame in unison. Frames are read straight from ROM at compose time - no per-frame override
// copy. The chosen phase (0..frameCount-1) is folded into the slot recipe key, so cells in the same
// band still share a slot; distinct on-screen slots stay bounded by frameCount per recipe, not by the
// number of animated cells - that bound is what keeps large water areas off the 512-slot pool.
#define COMPOSITE_PHASED_GROUP_COUNT 13

struct PhasedAnimGroup
{
    const u16 *const *frames;     // [frameCount]; each entry -> tileCount 8BPP tiles in ROM
    s16 (*phaseFn)(s16 x, s16 y); // maps a cell's map position to a raw SIGNED phase (floor-modded by bandCount)
    u16 firstTileId;
    u8 tileCount;
    u8 frameCount;
    // bandCount is the perf knob: the number of DISTINCT phases a group can show on screen, so it caps
    // the composite slots (and the per-column compose cost while scrolling) a group costs. A cell's stored
    // phase is band * bandStride. bandCount <= frameCount samples the frame cycle (bandStride = frameCount
    // / bandCount): fewer bands space the frames further apart - seamless when it divides frameCount;
    // bandCount == frameCount is the finest. bandCount > frameCount uses stride 1, so the phase is just
    // band = phaseFn % bandCount and does NOT depend on frameCount - letting a group hot-swap its frame
    // set (frames/frameCount) while keeping every cell's phase, so no re-phasing redraw is needed.
    u8 bandCount;
    u8 bandStride;
    bool8 active;
    // TRUE = this group animates on the fixed (wind-independent) clock (sPhasedStepFixed) instead of the
    // wind-coupled water step. Only valid when the group's tiles never share a composited slot with a
    // wind-clock group (a bank has one frame index, so it can't mix clocks); the waterfall qualifies.
    bool8 fixedClock;
};

// Layer count of a left-packed (transparent entries dropped) recipe; derivable, so it isn't stored.
static inline u32 EntryCount(const u16 *e)
{
    return e[3] ? 4 : (e[2] ? 3 : (e[1] ? 2 : (e[0] ? 1 : 0)));
}

static inline u32 SlotCount(const struct CompositeSlot *s)
{
    return EntryCount(s->entries);
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
static struct PhasedAnimGroup sPhasedGroups[COMPOSITE_PHASED_GROUP_COUNT];
// Fast reject for FindPhasedGroup: the min/max animated tile id across all active groups. Most tiles
// aren't phased, so a range check skips the group scan entirely for them. Set to the empty range
// (min > max, so every tile is rejected) by FieldCompositorClearPhasedGroups at init.
static u16 sPhasedMinTile;
static u16 sPhasedMaxTile;
// Direct map from (tileId - sPhasedMinTile) to owning group index (0xFF = none), so the per-layer
// lookup in FlattenRecipe is one load instead of a scan of all groups. Rebuilt on (re)register; if the
// active groups ever span more tile ids than this, the map is disabled and lookups fall back to the scan.
// EWRAM to spare the IWRAM stack headroom; the flatten it serves reads its 64+ source bytes from
// ROM/EWRAM anyway, so the slower load is noise.
// Must cover the span of all active phased tile ids (max - min). The terrain groups run 0x170..0x1CD (the
// water_edge/water_fresh surfaces end the range), a span of 93; 128 keeps the fast map valid with headroom. Grow this
// (a cheap EWRAM byte each) if a tileset ever adds phased tiles past that, else the map disables and every
// phased-tile lookup falls back to the group scan.
#define PHASED_TILE_SPAN 128
static EWRAM_DATA u8 sPhasedGroupMap[PHASED_TILE_SPAN] = {0};
static bool8 sPhasedMapValid;
// Current animation step shared by all phased groups (advances every 16 frames). The displayed frame of
// a phased slot is (sPhasedStep + slot.phase) % period; folded here rather than per-group so a bank's
// single frame index is unambiguous.
static u16 sPhasedStep;
// Second phased clock for groups flagged fixedClock (the waterfall): advances at a constant 1/16 instead of
// the wind-coupled water cadence, so those slots animate at a steady speed regardless of wind. A slot picks
// this vs sPhasedStep by its own fixedClock bit; the two clocks never meet in one bank (see fixedClock).
static u16 sPhasedStepFixed;
// Origin of the "world" frame the position-dependent phase is evaluated in (see PhaseForRecipe). A slot bakes
// its phase from the map coordinate at acquire and never re-evaluates it, so if the coordinate frame shifts
// underneath the on-screen slots - which it does on a map-connection transition, where gCameraPos re-bases
// onto the new map (see CameraMove) - a physical tile's coordinate changes and freshly drawn cells would
// disagree with the still-baked ones, seaming the surface. Phase is evaluated at (x - origin, y - origin)
// instead, and FieldCompositorShiftPhaseOrigin folds each frame shift into this origin so a physical tile
// keeps the same phase argument across the shift: the surface stays coherent with no full re-acquire. Reset
// to 0 on a full map load / phased-group re-register (a fresh reference frame); persists across connections.
static s16 sPhaseOriginX;
static s16 sPhaseOriginY;
// In EWRAM, not the default IWRAM: this file's statics live in scarce IWRAM, and the bank metadata is
// touched per-slot (not per-pixel), so the slower EWRAM read is fine and the IWRAM is better spent.
static EWRAM_DATA struct FrameBank sBanks[COMPOSITE_BANK_MAX] = {0};
static u8 sBankCount;  // banks actually backed by sBankTiles (<= COMPOSITE_BANK_MAX; may be trimmed to fit the heap)
// Reband fast path (FieldCompositorBeginReband..EndReband): during a wind-heading re-band every on-screen
// BANKED phased slot is pinned (refcount +1, so no sibling is freed/recreated mid-walk) and indexed here by
// (bank, phase). Re-pointing a water cell to its new band is then a direct table read, not a hash probe. Only
// valid while sRebandActive; entries are BUCKET_EMPTY-cleared at BeginReband and drained at EndReband.
static EWRAM_DATA u16 sRebandBandSlot[COMPOSITE_BANK_MAX][COMPOSITE_BANK_FRAMES] = {0};
static bool8 sRebandActive;
static bool8 sBankPoolFull; // latched when the pool overflows, so the diagnostic prints once per episode
// Diagnostics raised at the deepest stack of the frame (ResolveBank/AcquireHashedSlot run under the
// full acquire chain). Printing there pushed the system stack over its budget - MgbaPrintf adds ~150 B
// of frames - so they only set a bit here and FieldCompositorReclaimFreedSlots (the shallow frame top)
// prints them.
#define DIAG_BANK_POOL_FULL   (1 << 0)
#define DIAG_SLOT_POOL_EMPTY  (1 << 1)
static u8 sPendingDiagnostics;
static u8 *sBankTiles; // [sBankCount * COMPOSITE_BANK_FRAMES * TILE_SIZE_8BPP], heap; NULL disables banking
// ComposeSlot's flatten scratch. In EWRAM, not on the stack: ComposeSlot runs from the main loop on top of an
// already-deep frame, and a 64-byte on-stack tile buffer here overran the top of IWRAM (corrupting
// gTransparentTileNumber / gWindowBgTilemapBuffers just past the .bss end).
static EWRAM_DATA ALIGNED(4) u8 sComposeScratch[TILE_SIZE_8BPP] = {0};
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

// Live phased slots (bank != BANK_NONE), so the per-step refresh scan can stop as soon as it's visited them
// all (and skip entirely on step ticks with no water on screen).
static u16 sPhasedSlotCount;
// TRUE while some bank may have unbuilt/rebuild frames, so the idle-frame BankBuildTick can skip its scan.
static bool8 sBankBuildPending;

// Deferred source-range recomposites accumulated this frame. Each deferred RecomposeSourceRange used
// to walk the whole slot pool; a frame with several queued animations repeated that walk per range.
// Instead the ranges collect here (adjacent/overlapping ones merge) and FieldCompositorTickAnim marks
// the affected slots in ONE pool scan per frame.
#define PENDING_RECOMPOSE_MAX 8
static EWRAM_DATA u16 sPendingRecompose[PENDING_RECOMPOSE_MAX][2] = {0}; // [i] = {firstTileId, lastTileId}; EWRAM (cold)
static u8 sPendingRecomposeCount;

// Modulo for the small operands the phased paths produce (dividend < ~64 + max band, modulus a frame
// count of 6/12): a subtract loop beats the software-division call the % operator emits (the GBA has
// no divide instruction), and these run per slot / per layer on the animation hot paths.
static inline u32 FastMod(u32 v, u32 m)
{
    while (v >= m)
        v -= m;
    return v;
}

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
        sPool[i].maskBits = 0;
        sPool[i].bank = BANK_NONE;
        sPool[i].refcount = 0;
        sPool[i].nextInBucket = BUCKET_EMPTY;
    }
    for (i = 0; i < COMPOSITE_BANK_MAX; i++)
    {
        sBanks[i].refcount = 0;
        sBanks[i].builtMask = 0;
        sBanks[i].rebuildMask = 0;
    }
    sBankPoolFull = FALSE;
    sBankBuildPending = FALSE;
    sPhasedSlotCount = 0;
    sPendingRecomposeCount = 0;
    sPendingDiagnostics = 0;
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
    // Pre-composited water frame banks (see struct FrameBank). Ask for COMPOSITE_BANK_MAX but shrink to
    // whatever the heap can spare, rather than failing outright - a smaller pool just means more recipes
    // fall back to live compositing. 0 disables banking entirely (all water flattens live).
    // Step down one bank at a time (not halving) so we land on the largest count the heap can spare,
    // never overshooting far below the cap. One-time at map load; failed Allocs are side-effect-free.
    sBankCount = COMPOSITE_BANK_MAX;
    sBankTiles = NULL;
    while (sBankCount > 0
        && (sBankTiles = Alloc(sBankCount * COMPOSITE_BANK_FRAMES * TILE_SIZE_8BPP)) == NULL)
        sBankCount--;
    // WARN level so it isn't filtered as INFO. Positive confirmation banking is live + the pool size.
    if (sBankTiles == NULL)
        DebugPrintfLevel(MGBA_LOG_WARN, "FieldCompositor: NO heap for frame banks; water flattens live");
    else
        DebugPrintfLevel(MGBA_LOG_WARN, "FieldCompositor: %d frame banks (%d KB)%s", sBankCount,
                         sBankCount * COMPOSITE_BANK_FRAMES * TILE_SIZE_8BPP / 1024,
                         sBankCount < COMPOSITE_BANK_MAX ? " [trimmed]" : "");
    sBankPoolFull = FALSE;
    sPhasedStep = 0;
    sPhasedStepFixed = 0;
    sPhaseOriginX = 0; // fresh reference frame on a full map load (a warp re-acquires the whole view)
    sPhaseOriginY = 0;
    sAnimOverrideIndex = AllocZeroed(NUM_TILES_TOTAL * sizeof(u8));
    for (i = 0; i < NUM_TILES_TOTAL; i++)
        sAnimOverrideIndex[i] = ANIM_NOT_OVERRIDDEN;
    for (i = 0; i < ARRAY_COUNT(sBankOffset); i++)
        sBankOffset[i] = i * 16;
    sAnimPrimaryCount = 0;
    sAnimSecondaryCount = 0;
    sRebandActive = FALSE;
    FieldCompositorClearPhasedGroups();
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
    TRY_FREE_AND_SET_NULL(sBankTiles);
    TRY_FREE_AND_SET_NULL(sAnimOverrideIndex);
}

// Point the compositor at a tileset's (uncompressed, ROM-resident) 4BPP graphics. A location
// crossing can swap a different secondary tileset into the same id range while composites built from
// the old graphics are still live, so clear that range's animation overrides and mark the live slots
// referencing it dirty. Every tileset swap is immediately followed by a full DrawWholeMapView, which
// re-resolves each visible cell against the new metatile definitions: changed recipes acquire fresh
// (correctly composed) slots and free the stale ones, so the dirty mark only has to cover the rare
// slot whose recipe survives the swap unchanged - those drain via FieldCompositorTickAnim within a
// frame or two. Recompositing every live slot up front here instead just redoes work the redraw is
// about to discard. All no-ops on first load (nothing composited yet).
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
    RecomposeSourceRange(0, NUM_TILES_IN_PRIMARY - 1, TRUE);
}

void FieldCompositorLoadSecondaryTiles(const struct Tileset *tileset)
{
    if (tileset == NULL)
        return;
    sSecondaryTiles = (const u8 *)tileset->tiles;
    LoadBankOffsets(tileset, NUM_PALS_IN_PRIMARY, NUM_PALS_TOTAL - 1);
    ClearAnimOverrides(NUM_TILES_IN_PRIMARY, NUM_TILES_TOTAL - 1, FALSE);
    RecomposeSourceRange(NUM_TILES_IN_PRIMARY, NUM_TILES_TOTAL - 1, TRUE);
}

// Flatten a slot's recipe (bottom->top) into one 8BPP tile and DMA it to VRAM. The 8BPP source
// pixels are relative indices; each non-zero one becomes (pix + sBankOffset[palField]) against the
// unified 256-colour overworld palette. For legacy 16-colour art (pix 0..15, base a multiple of
// 16) this is identical to the old (palBank << 4) | pix. Zero stays transparent so upper layers
// show the layers below.
// If tileId belongs to an active phased group, return it and the tile's sub-index within a frame;
// else NULL. Cheap range reject first, since the overwhelming majority of tiles aren't phased; tiles
// in range resolve through the direct map (one load) rather than a scan of all groups - this runs per
// layer on every flatten.
static struct PhasedAnimGroup *FindPhasedGroup(u16 tileId, u8 *subOut)
{
    u32 g;

    if (tileId < sPhasedMinTile || tileId > sPhasedMaxTile)
        return NULL;
    if (sPhasedMapValid)
    {
        g = sPhasedGroupMap[tileId - sPhasedMinTile];
        if (g == 0xFF)
            return NULL;
        *subOut = tileId - sPhasedGroups[g].firstTileId;
        return &sPhasedGroups[g];
    }
    for (g = 0; g < COMPOSITE_PHASED_GROUP_COUNT; g++)
    {
        struct PhasedAnimGroup *grp = &sPhasedGroups[g];

        if (grp->active && tileId >= grp->firstTileId && tileId < grp->firstTileId + grp->tileCount)
        {
            *subOut = tileId - grp->firstTileId;
            return grp;
        }
    }
    return NULL;
}

// Recompute the tile-id range and direct map from the active groups. Called on every (re)register and
// clear - both rare - so registration order and removals never leave stale entries.
static void RebuildPhasedTileMap(void)
{
    u32 g, t;

    sPhasedMinTile = 0xFFFF;
    sPhasedMaxTile = 0;
    for (g = 0; g < COMPOSITE_PHASED_GROUP_COUNT; g++)
    {
        struct PhasedAnimGroup *grp = &sPhasedGroups[g];

        if (!grp->active)
            continue;
        if (grp->firstTileId < sPhasedMinTile)
            sPhasedMinTile = grp->firstTileId;
        if (grp->firstTileId + grp->tileCount - 1 > sPhasedMaxTile)
            sPhasedMaxTile = grp->firstTileId + grp->tileCount - 1;
    }
    sPhasedMapValid = sPhasedMinTile <= sPhasedMaxTile
                   && sPhasedMaxTile - sPhasedMinTile < PHASED_TILE_SPAN;
    if (!sPhasedMapValid)
        return;
    for (t = 0; t < PHASED_TILE_SPAN; t++)
        sPhasedGroupMap[t] = 0xFF;
    for (g = 0; g < COMPOSITE_PHASED_GROUP_COUNT; g++)
    {
        struct PhasedAnimGroup *grp = &sPhasedGroups[g];

        if (grp->active)
            for (t = 0; t < grp->tileCount; t++)
                sPhasedGroupMap[grp->firstTileId + t - sPhasedMinTile] = g;
    }
}

// The animation band a recipe falls in: the phaseFn of its first phased tile evaluated at the cell's
// map position, floor-modded by that group's bandCount so only bandCount distinct bands ever exist. 0 when
// the recipe has no phased tile (the common case), which keeps ordinary recipes deduping as before.
// The phase is SIGNED and reduced on its true value (not truncated to u8 first): a floor mod is seamless
// for negative phases (the y>x half of the map) and across every 256-tile step, where (raw & 0xFF) % band
// would seam because 256 is not a multiple of bandCount.
static u8 PhaseForRecipe(const u16 *norm, u32 n, s16 x, s16 y)
{
    struct PhasedAnimGroup *chosen = NULL;
    u32 i;

    // Prefer a fixedClock group (the waterfall): in a mixed waterfall+water slot its phase must drive the
    // recipe so the fall stays in coherent vertical columns (PhaseFn_Terrain_Waterfall) instead of inheriting
    // water's diagonal ripple. Otherwise the first phased group wins - the common single-group case takes it
    // on the first hit. This keeps clock (RecipeUsesFixedClock) and phase picking the same group.
    for (i = 0; i < n; i++)
    {
        u8 sub;
        struct PhasedAnimGroup *grp = FindPhasedGroup(norm[i] & SUBTILE_ENTRY_TILE_MASK, &sub);

        if (grp != NULL)
        {
            if (chosen == NULL)
                chosen = grp;
            if (grp->fixedClock)
            {
                chosen = grp;
                break;
            }
        }
    }
    if (chosen != NULL)
    {
        // Evaluate in the frame-independent world origin (see sPhaseOriginX/Y) so a coordinate-frame
        // shift under the on-screen slots doesn't seam the surface.
        s32 band = chosen->phaseFn(x - sPhaseOriginX, y - sPhaseOriginY) % chosen->bandCount;
        if (band < 0)
            band += chosen->bandCount;
        return band * chosen->bandStride;
    }
    return 0;
}

// Whether a recipe animates on the fixed clock: TRUE iff it carries at least one fixedClock phased group
// (the waterfall / its spray). When a waterfall subtile co-composites with wind-clock water in one slot the
// waterfall wins - the whole slot rides the steady 1/16 fixed step instead of drifting onto the wind-coupled
// water cycle. A bank uses one clock for all its layers, so the blended water there just rides it too; that's
// fine, the waterfall is the dominant feature. PhaseForRecipe picks the matching (fixedClock) phase.
static bool32 RecipeUsesFixedClock(const u16 *norm, u32 n)
{
    u32 i;

    for (i = 0; i < n; i++)
    {
        u8 sub;
        struct PhasedAnimGroup *grp = FindPhasedGroup(norm[i] & SUBTILE_ENTRY_TILE_MASK, &sub);
        if (grp != NULL && grp->fixedClock)
            return TRUE;
    }
    return FALSE;
}

// Per-byte "nonzero -> 0xFF" mask of a 4-pixel word, so an 8BPP layer can be composited four pixels at
// a time: opaque pixels get the mask, transparent (zero) pixels get 0 and leave the layer below intact.
// Carry-safe SWAR - (byte & 0x7F) + 0x7F never overflows its byte, so per-byte flags don't contaminate
// neighbours - then the isolated 0x80 flag is smeared down to fill the byte.
static inline u32 NonzeroByteMask(u32 v)
{
    u32 t = ((v & 0x7F7F7F7Fu) + 0x7F7F7F7Fu) | v;
    t &= 0x80808080u;
    t |= t >> 1; t |= t >> 2; t |= t >> 4;
    return t;
}

// Assemble a little-endian 32-bit word (4 source pixels) from a 2-byte-aligned tile pointer. Tile bases
// (ROM u16 frame arrays, the EWRAM tile caches) and the 8*row + 4*half offset are all even, so a pair of
// halfword loads is always safe - and far cheaper than the four byte loads + branches it replaces.
static inline u32 ReadTileWord(const u8 *p)
{
    const u16 *h = (const u16 *)p;
    return (u32)h[0] | ((u32)h[1] << 16);
}

// Flatten a normalized recipe into `out` (one 8BPP tile). `frameSel` picks each phased layer's source
// frame (frame = frameSel % group frameCount); it's ignored by non-phased layers. Live phased slots pass
// sPhasedStep + phase (reproducing the old per-slot compose exactly); bank building passes the raw frame
// index. This is the only place recipe pixels are produced - ComposeSlot and bank building share it.
static void FlattenRecipe(const u16 *entries, u32 n, u8 maskBits, u16 frameSel, u8 *out)
{
    u32 layer, row, b;
    // The first drawn layer writes every output pixel (transparent source pixels write 0), so the
    // buffer needs no pre-zero and that layer skips the read-modify-write against pixels below -
    // the bulk of the work for the common 1-layer recipe. Later layers blend against `out` as usual.
    bool32 outValid = FALSE;

    // Draw entries are packed before mask entries, so a single in-order pass composites all draws
    // first, then lets the mask entries punch holes in the finished pixels.
    for (layer = 0; layer < n; layer++)
    {
        u16 e = entries[layer];
        u16 tileId = e & SUBTILE_ENTRY_TILE_MASK;
        bool32 isMask = (maskBits >> layer) & 1;
        // A phased tile is read straight from its group's ROM frame array; everything else comes from
        // the source cache (override or ROM) as usual.
        u8 sub;
        struct PhasedAnimGroup *grp = FindPhasedGroup(tileId, &sub);
        const u8 *src = grp != NULL
            ? (const u8 *)grp->frames[FastMod(frameSel, grp->frameCount)] + sub * TILE_SIZE_8BPP
            : GetSourceTilePtr(tileId);
        u32 palBase = sBankOffset[(e >> SUBTILE_ENTRY_PAL_SHIFT) & 0xF];
        u32 palW = palBase * 0x01010101u; // palBase broadcast to all four bytes of a word
        bool32 hflip = e & SUBTILE_ENTRY_HFLIP;
        bool32 vflip = e & SUBTILE_ENTRY_VFLIP;

        if (src == NULL)
            continue;
        if (!outValid && isMask)
        {
            // Mask with nothing drawn beneath (the acquire paths never build this): the output is
            // all-transparent either way.
            break;
        }
        // Each source byte is one 8BPP pixel. vflip just picks the source row. A mask entry clears
        // (holes) every pixel it covers instead of drawing it, so the layers below show through on the
        // plane above. The common (no-hflip) path composites four pixels per word, branchless (see
        // NonzeroByteMask); hflip is rare, so it keeps the per-pixel reversed loop.
        for (row = 0; row < 8; row++)
        {
            const u8 *srcRow = src + (vflip ? 7 - row : row) * 8;

            if (hflip)
            {
                u8 *o = out + row * 8;
                for (b = 0; b < 8; b++)
                {
                    u8 pix = srcRow[7 - b];
                    if (!outValid) o[b] = pix ? pix + palBase : 0;
                    else if (isMask) { if (pix) o[b] = 0; }
                    else if (pix) o[b] = pix + palBase;
                }
            }
            else
            {
                u32 *o = (u32 *)(out + row * 8);

                for (b = 0; b < 2; b++)
                {
                    u32 sw = ReadTileWord(srcRow + b * 4);
                    u32 nz = NonzeroByteMask(sw);
                    if (!outValid)
                        o[b] = (sw + palW) & nz;
                    else if (isMask)
                        o[b] &= ~nz;
                    else
                        o[b] = (o[b] & ~nz) | ((sw + palW) & nz);
                }
            }
        }
        outValid = TRUE;
    }

    if (!outValid)
        for (row = 0; row < TILE_SIZE_8BPP / 4; row++)
            ((u32 *)out)[row] = 0;
}

// Flatten bank frame `idx` into its tile region and mark it built.
static void BuildBankFrame(struct FrameBank *bk, u32 bank, u32 idx)
{
    FlattenRecipe(bk->entries, EntryCount(bk->entries), bk->maskBits, idx,
                  sBankTiles + (bank * COMPOSITE_BANK_FRAMES + idx) * TILE_SIZE_8BPP);
    bk->builtMask |= (1u << idx);
}

// Fill a slot's VRAM tile with its current frame. A banked slot whose frame is already built (the steady
// state) is a straight 64-byte copy - no flatten. If the frame isn't built yet (the recipe appeared in
// the last frame or two, before BankBuildTick finished it) this one slot flattens live - the same cost a
// non-banked cell always pays - so revealing water never triggers a burst of bank builds, just the
// inherent per-cell composite. Non-phased slots always flatten live.
// `scratch` is a TILE_SIZE_8BPP flatten buffer supplied by the caller (main loop vs VBlank use separate
// EWRAM buffers, see sComposeScratch) instead of a stack local - this runs in the VBlank handler on the
// shared system stack and a 64-byte frame here overflowed the top of IWRAM.
static void ComposeSlot(u16 slot, u8 *scratch)
{
    struct CompositeSlot *s = &sPool[slot];
    const void *src;
    // Fixed-clock slots (the waterfall) run off the wind-independent step; everything else off the water step.
    u16 step = s->fixedClock ? sPhasedStepFixed : sPhasedStep;

    if (s->bank < sBankCount)
    {
        struct FrameBank *bk = &sBanks[s->bank];
        u32 idx = FastMod(step + s->phase, bk->period);

        if (bk->builtMask & (1u << idx))
            src = sBankTiles + (s->bank * COMPOSITE_BANK_FRAMES + idx) * TILE_SIZE_8BPP;
        else
        {
            FlattenRecipe(bk->entries, EntryCount(bk->entries), bk->maskBits, idx, scratch);
            src = scratch;
        }
    }
    else
    {
        FlattenRecipe(s->entries, SlotCount(s), s->maskBits, step + s->phase, scratch);
        src = scratch;
    }

    // CPU copy, not DmaCopy32: this runs in the main loop during active display, and the field VBlank
    // handler also drives DMA3 (FlushFieldBgTilemap, sprite/palette transfers). A VBlank landing between
    // the DMA register writes here would leave DMA3's source pointing at the VBlank transfer, so this
    // copy would flatten the wrong source into the slot - one corrupted 8x8 tile, timing-dependent and
    // worst under the heavy slot churn of fast movement. A 64-byte CpuFastCopy shares no channel with
    // VBlank, so it can't be clobbered.
    CpuFastCopy(src, (void *)(BG_VRAM + slot * TILE_SIZE_8BPP), TILE_SIZE_8BPP);
}

// Flatten a bounded number of not-yet-built bank frames each frame (called every frame). A newly-appeared
// recipe's frame set fills in over the next frame or two in the background, off the frame it was revealed
// on; until then its slots flatten live (see ComposeSlot). Building runs far ahead of the 16-frame step
// cadence, so by the time the animation advances the frames are ready and the whole refresh is copies.
static void BankBuildTick(void)
{
    u32 budget = BANK_BUILD_BUDGET;
    u32 i, m;
    bool32 remaining = FALSE;

    // sBankBuildPending is set wherever unbuilt/rebuild frames can appear (a new bank in ResolveBank, a
    // frame-set swap in FieldCompositorReloadPhasedGroup), so idle frames skip the bank scan entirely.
    if (sBankTiles == NULL || !sBankBuildPending)
        return;
    for (i = 0; i < sBankCount; i++)
    {
        struct FrameBank *bk = &sBanks[i];
        u16 need;

        if (bk->refcount == 0)
            continue;
        // Frames to (re)flatten: never-built ones, plus any whose source changed under a live swap. A
        // rebuild frame keeps showing its old content (a valid copy) until replaced here, so a wave-set
        // swap never storms live flattens - the whole surface just refreshes over the next few frames.
        need = (~bk->builtMask | bk->rebuildMask) & (u16)((1u << bk->period) - 1);
        if (need == 0)
            continue;
        for (m = 0; m < bk->period && budget != 0; m++)
        {
            if (need & (1u << m))
            {
                BuildBankFrame(bk, i, m); // sets builtMask bit m
                bk->rebuildMask &= ~(1u << m);
                need &= ~(1u << m);
                budget--;
            }
        }
        if (need != 0)
            remaining = TRUE;
        if (budget == 0)
            break;
    }
    // Only a full clean pass clears the flag; running out of budget mid-scan keeps it set.
    sBankBuildPending = remaining || budget == 0;
}

// Greatest common divisor / least common multiple, for a bank's animation period (the lcm of its phased
// groups' frame counts).
static u32 Gcd(u32 a, u32 b)
{
    while (b != 0) { u32 t = a % b; a = b; b = t; }
    return a;
}

// A recipe's animation period from current group state: the lcm of its phased groups' frame counts (1 if
// none). Used to refresh a bank's period when a live group's frameCount changes (see the reload path).
static u32 ComputeRecipePeriod(const u16 *entries, u32 n)
{
    u32 i, period = 1;

    for (i = 0; i < n; i++)
    {
        u8 sub;
        struct PhasedAnimGroup *grp = FindPhasedGroup(entries[i] & SUBTILE_ENTRY_TILE_MASK, &sub);
        if (grp != NULL)
            period = period / Gcd(period, grp->frameCount) * grp->frameCount;
    }
    return period;
}

// Resolve the frame bank for a normalized recipe. Returns BANK_NONE if the recipe has no phased tile,
// BANK_UNBANKED if it's phased but can't be banked (period too long, pool full, or no bank heap) - both
// flatten live. Otherwise returns the bank index; its frames are NOT built here, they fill in lazily on
// first display and via BankBuildTick, so a new recipe doesn't cost a burst of flattens up front. Bank
// refcount tracks the live slots (across all bands) sharing it.
static u8 ResolveBank(const u16 *entries, u32 n, u8 maskBits)
{
    u32 i, period = 1;
    bool32 phased = FALSE;
    s32 freeBank = -1;

    for (i = 0; i < n; i++)
    {
        u8 sub;
        struct PhasedAnimGroup *grp = FindPhasedGroup(entries[i] & SUBTILE_ENTRY_TILE_MASK, &sub);
        if (grp != NULL)
        {
            phased = TRUE;
            period = period / Gcd(period, grp->frameCount) * grp->frameCount;
        }
    }
    if (!phased)
        return BANK_NONE;
    if (sBankTiles == NULL || period > COMPOSITE_BANK_FRAMES)
        return BANK_UNBANKED;

    // Reuse an identical live bank (same recipe, any band); else remember a free slot for a new one.
    for (i = 0; i < sBankCount; i++)
    {
        if (sBanks[i].refcount == 0)
        {
            if (freeBank < 0)
                freeBank = i;
            continue;
        }
        if (sBanks[i].maskBits == maskBits
         && sBanks[i].entries[0] == (n > 0 ? entries[0] : 0)
         && sBanks[i].entries[1] == (n > 1 ? entries[1] : 0)
         && sBanks[i].entries[2] == (n > 2 ? entries[2] : 0)
         && sBanks[i].entries[3] == (n > 3 ? entries[3] : 0))
        {
            sBanks[i].refcount++;
            return i;
        }
    }
    if (freeBank < 0)
    {
        // Pool full this scene: this recipe (and any others over the cap) flatten live each step - the
        // step-update spike. If this fires, raise COMPOSITE_BANK_MAX (heap permitting). Latched so it
        // reports once per full episode rather than every acquire; deferred print (see sPendingDiagnostics).
        if (!sBankPoolFull)
        {
            sBankPoolFull = TRUE;
            sPendingDiagnostics |= DIAG_BANK_POOL_FULL;
        }
        return BANK_UNBANKED;
    }

    for (i = 0; i < COMPOSITE_MAX_LAYERS; i++)
        sBanks[freeBank].entries[i] = (i < n) ? entries[i] : 0;
    sBanks[freeBank].maskBits = maskBits;
    sBanks[freeBank].period = period;
    sBanks[freeBank].builtMask = 0; // frames build lazily
    sBanks[freeBank].rebuildMask = 0;
    sBanks[freeBank].refcount = 1;
    sBankBuildPending = TRUE; // the new bank's frames need building (see BankBuildTick)
    return freeBank;
}

static void ReleaseBank(u8 bank)
{
    if (bank < sBankCount && sBanks[bank].refcount != 0)
    {
        if (--sBanks[bank].refcount == 0)
            sBankPoolFull = FALSE; // a bank freed up; allow the pool-full diagnostic to fire again
    }
}

static u32 HashRecipe(const u16 *entries, u32 count, u8 maskBits, u8 phase)
{
    u32 h = (count * 33 + maskBits) * 33 + phase;
    u32 i;

    for (i = 0; i < count; i++)
        h = h * 33 + entries[i];
    return h & (COMPOSITE_HASH_SIZE - 1);
}

static bool32 RecipeMatches(const struct CompositeSlot *s, const u16 *entries, u32 count, u8 maskBits, u8 phase)
{
    u32 i;

    if (SlotCount(s) != count || s->maskBits != maskBits || s->phase != phase)
        return FALSE;
    for (i = 0; i < count; i++)
        if (s->entries[i] != entries[i])
            return FALSE;
    return TRUE;
}

// Find-or-create a deduplicated slot for a normalized recipe at a given animation phase. Cells sharing
// (recipe, phase) share a slot; a phase differs only when the recipe contains a phased tile.
static u16 AcquireHashedSlot(const u16 *norm, u32 n, u8 maskBits, u8 phase)
{
    u32 bucket;
    u16 slot;
    u32 i;

    bucket = HashRecipe(norm, n, maskBits, phase);
    for (slot = sHashHead[bucket]; slot != BUCKET_EMPTY; slot = sPool[slot].nextInBucket)
    {
        if (RecipeMatches(&sPool[slot], norm, n, maskBits, phase))
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
        // every redraw so it self-corrects as soon as a slot frees. Deferred print (see sPendingDiagnostics).
        sPendingDiagnostics |= DIAG_SLOT_POOL_EMPTY;
        return COMPOSITE_SLOT_KEEP;
    }

    slot = sFreeList[--sFreeCount];
    // Left-pack the recipe and zero the unused layers so SlotCount reads back n.
    for (i = 0; i < COMPOSITE_MAX_LAYERS; i++)
        sPool[slot].entries[i] = (i < n) ? norm[i] : 0;
    sPool[slot].maskBits = maskBits;
    sPool[slot].phase = phase;
    // Resolve the frame bank before the first compose so ComposeSlot copies from it (banked recipes) or
    // flattens live (BANK_NONE/BANK_UNBANKED). Only new slots resolve a bank; a hash hit above keeps the
    // existing slot's bank and its single ref, so refcounts stay balanced.
    sPool[slot].bank = ResolveBank(norm, n, maskBits);
    sPool[slot].fixedClock = RecipeUsesFixedClock(norm, n);
    if (sPool[slot].bank != BANK_NONE)
        sPhasedSlotCount++;
    sPool[slot].refcount = 1;
    sPool[slot].nextInBucket = sHashHead[bucket];
    sHashHead[bucket] = slot;
    ComposeSlot(slot, sComposeScratch);
    return slot;
}

// Find-or-create a slot for an already-normalized (left-packed, transparent entries dropped) recipe at
// the animation band for map position (x, y).
static u16 AcquireNormalized(const u16 *norm, u32 n, u8 maskBits, s16 x, s16 y)
{
    if (n == 0)
        return COMPOSITE_BLANK_SLOT;
    return AcquireHashedSlot(norm, n, maskBits, PhaseForRecipe(norm, n, x, y));
}

u16 FieldCompositorAcquire(const u16 *entries, u32 count, s16 x, s16 y)
{
    u16 norm[COMPOSITE_MAX_LAYERS];
    u32 n = 0;
    u32 i;

    // Drop transparent (tile id 0) entries; an empty stack is the shared blank tile.
    for (i = 0; i < count; i++)
    {
        if ((entries[i] & SUBTILE_ENTRY_TILE_MASK) != 0 && n < COMPOSITE_MAX_LAYERS)
            norm[n++] = entries[i];
    }
    return AcquireNormalized(norm, n, 0, x, y);
}

u16 FieldCompositorAcquireMasked(const u16 *draw, u32 drawCount, const u16 *mask, u32 maskCount, s16 x, s16 y)
{
    u16 norm[COMPOSITE_MAX_LAYERS];
    u32 n = 0;
    u8 maskBits = 0;
    u32 i;

    // Draws first (transparent ones dropped). No draw content -> blank cell, mask is irrelevant.
    for (i = 0; i < drawCount; i++)
    {
        if ((draw[i] & SUBTILE_ENTRY_TILE_MASK) != 0 && n < COMPOSITE_MAX_LAYERS)
            norm[n++] = draw[i];
    }
    if (n == 0)
        return COMPOSITE_BLANK_SLOT;

    // Mask entries fill the remaining slots (draw + mask never exceeds COMPOSITE_MAX_LAYERS).
    for (i = 0; i < maskCount; i++)
    {
        if ((mask[i] & SUBTILE_ENTRY_TILE_MASK) != 0 && n < COMPOSITE_MAX_LAYERS)
        {
            maskBits |= (1u << n);
            norm[n++] = mask[i];
        }
    }
    return AcquireNormalized(norm, n, maskBits, x, y);
}

// Open a reband window: pin every on-screen BANKED phased slot (refcount +1) and index it by (bank, phase),
// so FieldCompositorRebandSlot can re-point a cell to its new band by a direct table read and no sibling is
// freed/recreated while the walk re-points cells among them. Must be paired with FieldCompositorEndReband.
void FieldCompositorBeginReband(void)
{
    u16 slot;
    u32 b, p;

    if (!sActive || sPool == NULL)
        return;
    for (b = 0; b < sBankCount; b++)
        for (p = 0; p < COMPOSITE_BANK_FRAMES; p++)
            sRebandBandSlot[b][p] = BUCKET_EMPTY;
    for (slot = 1; slot < COMPOSITE_SLOT_COUNT; slot++)
    {
        u8 bank = sPool[slot].bank;

        if (sPool[slot].refcount == 0 || bank >= sBankCount) // skip free, non-phased (BANK_NONE), and unbanked
            continue;
        sPool[slot].refcount++; // pin so the walk can't free it out from under a later cell
        sRebandBandSlot[bank][sPool[slot].phase] = slot;
    }
    sRebandActive = TRUE;
}

// Close the reband window: drop each pinned slot's pin. A band no cell landed on this reband hits refcount 0
// here and is freed (one-frame held, like any release).
void FieldCompositorEndReband(void)
{
    u32 b, p;

    if (!sActive || sPool == NULL)
        return;
    sRebandActive = FALSE;
    for (b = 0; b < sBankCount; b++)
    {
        for (p = 0; p < COMPOSITE_BANK_FRAMES; p++)
        {
            u16 slot = sRebandBandSlot[b][p];
            if (slot != BUCKET_EMPTY)
            {
                sRebandBandSlot[b][p] = BUCKET_EMPTY;
                FieldCompositorRelease(slot);
            }
        }
    }
}

// Re-band one on-screen phased cell for the CURRENT phase gradient: given the slot a tilemap cell points at
// and that cell's map (x, y), return the sibling slot for the cell's new band (refcount incremented), or
// COMPOSITE_SLOT_KEEP when there's nothing to do - the cell is blank or non-phased (its band can't have moved)
// or the pool is full. Inside a reband window the sibling is a direct (bank, phase) table read; a band not
// present when the window opened (or an unbanked slot) falls back to a full recipe acquire. The stored recipe
// (entries/maskBits) is already the normalized form Acquire wants, so no map/attribute/reflection re-resolve.
u16 FieldCompositorRebandSlot(u16 slot, s16 x, s16 y)
{
    u8 bank;

    if (!sActive || sPool == NULL || slot == COMPOSITE_BLANK_SLOT)
        return COMPOSITE_SLOT_KEEP;
    bank = sPool[slot].bank;
    if (bank == BANK_NONE)
        return COMPOSITE_SLOT_KEEP;

    if (sRebandActive && bank < sBankCount)
    {
        u8 newPhase = PhaseForRecipe(sPool[slot].entries, SlotCount(&sPool[slot]), x, y);
        u16 sib = sRebandBandSlot[bank][newPhase];

        if (sib != BUCKET_EMPTY)
        {
            sPool[sib].refcount++;
            return sib;
        }
        // Band absent when the window opened: create it once, pin + index it so later cells in this band hit
        // the fast path above (and EndReband drops the pin uniformly).
        sib = AcquireNormalized(sPool[slot].entries, SlotCount(&sPool[slot]), sPool[slot].maskBits, x, y);
        if (sib != COMPOSITE_SLOT_KEEP && sib != COMPOSITE_BLANK_SLOT)
        {
            sPool[sib].refcount++;
            sRebandBandSlot[bank][newPhase] = sib;
        }
        return sib;
    }
    return AcquireNormalized(sPool[slot].entries, SlotCount(&sPool[slot]), sPool[slot].maskBits, x, y);
}

// Return a slot to the pool (one-frame held) and clear its recipe. The caller has already unlinked it
// from any hash bucket. One-frame held via sPendingFree so its pixels survive for the still-displayed
// tilemap cell that referenced it before this redraw's VRAM copy lands.
static void FreeSlot(u16 slot)
{
    u32 i;

    for (i = 0; i < COMPOSITE_MAX_LAYERS; i++)
        sPool[slot].entries[i] = 0;
    sPool[slot].maskBits = 0;
    sPool[slot].phase = 0;
    ReleaseBank(sPool[slot].bank);
    if (sPool[slot].bank != BANK_NONE)
        sPhasedSlotCount--;
    sPool[slot].bank = BANK_NONE;
    sPool[slot].refcount = 0;
    ClearSlotDirty(slot);
    sPendingFree[sPendingFreeCount++] = slot;
}

void FieldCompositorRelease(u16 slot)
{
    u32 bucket;
    u16 cur, prev;

    if (slot == COMPOSITE_BLANK_SLOT || slot >= COMPOSITE_SLOT_COUNT)
        return;
    if (sPool[slot].refcount == 0)
        return;
    if (--sPool[slot].refcount != 0)
        return;

    // Refcount hit zero: unlink from its hash bucket and return the slot to the free list.
    bucket = HashRecipe(sPool[slot].entries, SlotCount(&sPool[slot]), sPool[slot].maskBits, sPool[slot].phase);
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
    FreeSlot(slot);
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
    // Diagnostics raised at deep stack during the previous frame print here, at the frame's
    // shallowest point, so MgbaPrintf's frames can't contribute to a stack overflow.
    if (sPendingDiagnostics)
    {
        if (sPendingDiagnostics & DIAG_BANK_POOL_FULL)
            DebugPrintfLevel(MGBA_LOG_WARN, "FieldCompositor: bank pool full (%d); extra water recipes flatten live", sBankCount);
        if (sPendingDiagnostics & DIAG_SLOT_POOL_EMPTY)
            DebugPrintf("FieldCompositor: slot pool exhausted");
        sPendingDiagnostics = 0;
    }
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

// Register a phased animation group at `index` (0..COMPOSITE_PHASED_GROUP_COUNT-1). The frames array
// and phaseFn must outlive the group (they're ROM/const in practice). Overwrites any prior group at
// that index. See FieldCompositorClearPhasedGroups for the lifecycle.
void FieldCompositorRegisterPhasedGroup(u32 index, u16 firstTileId, u8 tileCount, const u16 *const *frames, u8 frameCount, u8 bandCount, s16 (*phaseFn)(s16, s16))
{
    struct PhasedAnimGroup *grp;

    if (index >= COMPOSITE_PHASED_GROUP_COUNT)
        return;
    // bandStride spaces the frames a band apart along the wave. bandCount <= frameCount samples the frame
    // cycle (stride = frameCount/bandCount; uniform, seamless when it divides). bandCount > frameCount is
    // allowed with stride 1: the stored phase is then band = phaseFn % bandCount, independent of
    // frameCount, so a group can keep the SAME per-cell phase while its frame set (and frameCount) is
    // hot-swapped - the terrain waves rely on this to change wind strength without re-phasing the map.
    if (bandCount == 0)
        bandCount = frameCount;
    grp = &sPhasedGroups[index];
    grp->frames = frames;
    grp->phaseFn = phaseFn;
    grp->firstTileId = firstTileId;
    grp->tileCount = tileCount;
    grp->frameCount = frameCount;
    grp->bandCount = bandCount;
    grp->bandStride = (bandCount <= frameCount) ? (frameCount / bandCount) : 1;
    grp->active = TRUE;
    grp->fixedClock = FALSE; // wind clock by default; opt in via FieldCompositorSetPhasedGroupFixedClock
    RebuildPhasedTileMap();
}

// Mark an already-registered phased group as running on the fixed (wind-independent) clock: its slots
// animate at a steady 1/16 (sPhasedStepFixed) rather than the wind-coupled water step. Call after
// registering. Only sound when the group's tiles never co-composite with a wind-clock group (a shared bank
// carries one frame index, so it can't mix clocks); the terrain waterfall qualifies - it only overlays
// static cliff tiles. A mixed cell would just fall back to the wind clock (see RecipeUsesFixedClock).
void FieldCompositorSetPhasedGroupFixedClock(u32 index)
{
    if (index < COMPOSITE_PHASED_GROUP_COUNT)
        sPhasedGroups[index].fixedClock = TRUE;
}

// Re-point an already-registered phased group at a new frame set (frames/frameCount may change) while the
// map is live - e.g. the terrain water waves switching animation strength with the wind. The group must be
// registered with a fixed bandCount > any set's frameCount (stride 1), so every cell's stored phase is
// band = phaseFn % bandCount and does NOT change when the frame set swaps: the existing composite slots
// stay valid, so no cell re-acquire / DrawWholeMapView (a full-screen recompose = a frame-time spike) is
// needed. Only the frame *content* changes, so we just repair the banks and mark the affected slots dirty;
// FieldCompositorTickAnim then recomposes them a few per frame, spreading the cost so the swap never spikes.
//
// Bank repair: a recipe's period is the lcm of its groups' frame counts, which for a mixed cell (e.g. a
// waves overlay on the 12-frame water) can exceed the frames a bank holds. Writing that back would make
// ComposeSlot/BankBuildTick index past the bank's tile region and corrupt VRAM/heap, so cap the period to
// what a bank can store (a capped cell re-syncs its overlay a touch early - imperceptible next to the
// dominant water motion).
//
// The old code cleared builtMask (invalidating even the frame on screen) and dirtied every slot, so the
// same frame's step tick recomposed the whole visible surface with live flattens - the swap spike. Instead
// we keep the built frames displayable and just set rebuildMask: BankBuildTick re-flattens them from the
// new array a few per frame, replacing each in place, and the on-screen slots pick up the new content as
// cheap bank copies on their next step tick. Nothing flattens live and no slot is dirtied, so the swap
// costs only bounded background work and never spikes.
void FieldCompositorReloadPhasedGroup(u32 index, u16 firstTileId, u8 tileCount, const u16 *const *frames, u8 frameCount, u8 bandCount, s16 (*phaseFn)(s16, s16))
{
    u16 lastTileId = firstTileId + tileCount - 1;
    u32 i, e;

    FieldCompositorRegisterPhasedGroup(index, firstTileId, tileCount, frames, frameCount, bandCount, phaseFn);
    if (sPool == NULL)
        return;
    for (i = 0; i < sBankCount; i++)
    {
        struct FrameBank *bk = &sBanks[i];

        if (bk->refcount == 0)
            continue;
        for (e = 0; e < COMPOSITE_MAX_LAYERS; e++)
        {
            u16 tileId = bk->entries[e] & SUBTILE_ENTRY_TILE_MASK;
            if (tileId >= firstTileId && tileId <= lastTileId)
            {
                u32 period = ComputeRecipePeriod(bk->entries, EntryCount(bk->entries));
                if (period > COMPOSITE_BANK_FRAMES)
                    period = COMPOSITE_BANK_FRAMES;
                bk->period = period;
                // Re-flatten every frame from the new source, but lazily: keep the current content on
                // screen until BankBuildTick replaces each frame, so no live-flatten storm this frame.
                bk->rebuildMask = (u16)((1u << period) - 1);
                sBankBuildPending = TRUE;
                break;
            }
        }
    }
}

// Drop all phased groups. Called before a primary tileset re-registers its own (phased groups live in
// primary-tileset tile-id space, so a map load resets them); the map's full redraw reacquires cells at
// fresh phases afterward.
void FieldCompositorClearPhasedGroups(void)
{
    u32 g;

    for (g = 0; g < COMPOSITE_PHASED_GROUP_COUNT; g++)
        sPhasedGroups[g].active = FALSE;
    RebuildPhasedTileMap(); // resets to the empty range (min > max), so every lookup rejects
    sPhasedStep = 0;
    sPhasedStepFixed = 0;
    // The primary tileset re-registers its groups right after this and the map fully redraws, baking every
    // cell fresh - so the accumulated connection-frame origin is stale; start the new phase field at zero.
    sPhaseOriginX = 0;
    sPhaseOriginY = 0;
}

// Shift the phase evaluation origin by a coordinate-frame delta (frameDx, frameDy) = how much a physical
// tile's map coordinate changed when the frame re-based (gCamera.x/y at a connection transition; see
// CameraMove). Applied so PhaseForRecipe evaluates the same physical tile at the same phase argument before
// and after the shift: already-baked on-screen slots and cells drawn after the crossing stay coherent, with
// no full-screen re-acquire. Call once per frame re-origin, before the post-shift cells are drawn.
void FieldCompositorShiftPhaseOrigin(s16 frameDx, s16 frameDy)
{
    // Physical coord Xnew = Xold - frameDx, so arg = X - origin stays put when origin -= frameDx.
    sPhaseOriginX -= frameDx;
    sPhaseOriginY -= frameDy;
}

// Recompose every live phased slot into its VRAM tile at the current step. A built bank is a cheap 64-byte
// copy; only a not-yet-built/unbanked slot flattens live. Called from the main-loop step tick, where the
// per-frame delta is tiny so the mid-scanout writes don't visibly tear.
static void FieldCompositorRefreshPhasedSlots(void)
{
    u16 slot;
    u32 remaining = sPhasedSlotCount;

    if (!sActive || sPool == NULL)
        return;
    // Stop once every live phased slot has been visited (and skip outright when there are none, e.g.
    // a step tick with no water on screen).
    for (slot = 1; slot < COMPOSITE_SLOT_COUNT && remaining != 0; slot++)
    {
        if (sPool[slot].bank != BANK_NONE && sPool[slot].refcount != 0)
        {
            ComposeSlot(slot, sComposeScratch);
            remaining--;
        }
    }
}

// Advance the phased (water) animation to `step` (all groups share it) and refresh every phased slot to
// its new frame. A banked slot (the common water case) is a cheap 64-byte copy from its pre-composited
// bank; only unbanked-phased slots re-flatten. Call once per frame; only a real step change does work.
void FieldCompositorTickPhased(u16 windStep, u16 fixedStep)
{
    if (!sActive)
        return;
    BankBuildTick(); // spread new recipes' frame builds across frames (runs every frame, not just on step change)
    if (windStep == sPhasedStep && fixedStep == sPhasedStepFixed)
        return;
    sPhasedStep = windStep;
    sPhasedStepFixed = fixedStep;
    FieldCompositorRefreshPhasedSlots();
}

// Recomposite (immediately) or queue for dirty-marking (deferred) every live slot whose recipe
// references a source tile in [firstTileId, lastTileId]. The immediate path walks the pool now (doors
// need same-frame updates); deferred ranges just collect in sPendingRecompose - several animations
// updating in one frame used to walk the whole pool once per range, now FlushPendingRecompose marks
// them all in a single walk.
static void RecomposeSourceRange(u16 firstTileId, u16 lastTileId, bool32 deferred)
{
    u16 slot;
    u32 i;

    if (sPool == NULL)
        return;
    if (deferred)
    {
        u32 r, best;
        u32 bestCost = ~0u;

        // Merge with an adjacent/overlapping pending range if one exists.
        for (r = 0; r < sPendingRecomposeCount; r++)
        {
            if (firstTileId <= sPendingRecompose[r][1] + 1 && lastTileId + 1 >= sPendingRecompose[r][0])
            {
                if (firstTileId < sPendingRecompose[r][0])
                    sPendingRecompose[r][0] = firstTileId;
                if (lastTileId > sPendingRecompose[r][1])
                    sPendingRecompose[r][1] = lastTileId;
                return;
            }
        }
        if (sPendingRecomposeCount < PENDING_RECOMPOSE_MAX)
        {
            sPendingRecompose[sPendingRecomposeCount][0] = firstTileId;
            sPendingRecompose[sPendingRecomposeCount][1] = lastTileId;
            sPendingRecomposeCount++;
            return;
        }
        // Table full: widen the nearest range instead. Over-marking just costs some extra deferred
        // recomposes; correctness is unaffected.
        best = 0;
        for (r = 0; r < PENDING_RECOMPOSE_MAX; r++)
        {
            u32 cost = (firstTileId > sPendingRecompose[r][1] ? firstTileId - sPendingRecompose[r][1] : 0)
                     + (lastTileId < sPendingRecompose[r][0] ? sPendingRecompose[r][0] - lastTileId : 0);
            if (cost < bestCost)
            {
                bestCost = cost;
                best = r;
            }
        }
        if (firstTileId < sPendingRecompose[best][0])
            sPendingRecompose[best][0] = firstTileId;
        if (lastTileId > sPendingRecompose[best][1])
            sPendingRecompose[best][1] = lastTileId;
        return;
    }
    for (slot = 1; slot < COMPOSITE_SLOT_COUNT; slot++)
    {
        if (sPool[slot].refcount == 0)
            continue;
        for (i = 0; i < SlotCount(&sPool[slot]); i++)
        {
            u16 tileId = sPool[slot].entries[i] & SUBTILE_ENTRY_TILE_MASK;
            if (tileId >= firstTileId && tileId <= lastTileId)
            {
                ComposeSlot(slot, sComposeScratch);
                break;
            }
        }
    }
}

// One pool walk marking every live slot that references any pending deferred range, then the ranges
// are dropped. Runs at most once per frame, from FieldCompositorTickAnim.
static void FlushPendingRecompose(void)
{
    u16 slot;
    u32 i, r;

    if (sPendingRecomposeCount == 0 || sPool == NULL)
        return;
    for (slot = 1; slot < COMPOSITE_SLOT_COUNT; slot++)
    {
        if (sPool[slot].refcount == 0)
            continue;
        for (i = 0; i < SlotCount(&sPool[slot]); i++)
        {
            u16 tileId = sPool[slot].entries[i] & SUBTILE_ENTRY_TILE_MASK;

            for (r = 0; r < sPendingRecomposeCount; r++)
            {
                if (tileId >= sPendingRecompose[r][0] && tileId <= sPendingRecompose[r][1])
                {
                    MarkSlotDirty(slot);
                    goto nextSlot;
                }
            }
        }
nextSlot:;
    }
    sPendingRecomposeCount = 0;
}

// Drains a bounded number of animation-dirtied slots per frame (call once per frame). The cursor
// rings through the pool so no slot is starved. A slot freed/reused before its turn had its dirty
// bit cleared, so only currently-live recipes are recomposited.
void FieldCompositorTickAnim(void)
{
    u32 budget = COMPOSITE_ANIM_RECOMPOSE_BUDGET;
    u32 scanned = 0;

    if (!sActive)
        return;
    FlushPendingRecompose(); // convert this frame's deferred source updates into dirty marks (one pool walk)
    if (sDirtyCount == 0)
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
                ComposeSlot(slot, sComposeScratch);
            ClearSlotDirty(slot);
            budget--;
        }
    }
}

