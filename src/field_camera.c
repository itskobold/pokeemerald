#include "global.h"
#include "berry.h"
#include "bg.h"
#include "bike.h"
#include "field_camera.h"
#include "field_compositor.h"
#include "field_player_avatar.h"
#include "fieldmap.h"
#include "event_object_movement.h"
#include "gpu_regs.h"
#include "main.h"
#include "menu.h"
#include "overworld.h"
#include "rotating_gate.h"
#include "script.h"
#include "sprite.h"
#include "text.h"

// Camera scroll speed (pixels per frame) used by the debug freecam. Must divide
// 16 so the camera always comes to rest aligned to a tile boundary.
#define FREECAM_SPEED 4

// Local id the camera-track tools use to mean "the player". Real NPC local ids start
// at 1, and the engine's own LOCALID_PLAYER (255) is awkward to show in a numeric box,
// so the player is exposed as 0 instead.
#define TRACK_LOCAL_ID_PLAYER 0

EWRAM_DATA bool8 gUnusedBikeCameraAheadPanback = FALSE;

struct FieldCameraOffset
{
    u8 xPixelOffset;
    u8 yPixelOffset;
    u8 xTileOffset;
    u8 yTileOffset;
};

static void RedrawMapSliceNorth(struct FieldCameraOffset *, const struct MapLayout *);
static void RedrawMapSliceSouth(struct FieldCameraOffset *, const struct MapLayout *);
static void RedrawMapSliceEast(struct FieldCameraOffset *, const struct MapLayout *);
static void RedrawMapSliceWest(struct FieldCameraOffset *, const struct MapLayout *);
static s32 MapPosToBgTilemapOffset(struct FieldCameraOffset *, s32, s32);
static void DrawWholeMapViewInternal(int, int, const struct MapLayout *);
static void DrawMetatileAt(const struct MapLayout *, u16, int, int);
static void DrawMetatileAtWithId(const struct MapLayout *, u16, int, int, u16 metatileId);
static void DrawCompositeCell(u16, const u16 *, u32, const u16 *, u32, const u16 *, u32, bool32, s16, s16);
static bool32 IsCliffPromotedCell(int, int);
static void CameraPanningCB_PanAhead(void);
static void UpdateFreecamMovement(struct CameraObject *);
static void UpdateCameraPanMovement(struct CameraObject *);
static void ClearCameraAnchors(void);
static void AnchorCameraToObject(struct ObjectEvent *);
static void DetachCameraForPan(void);
static void ClearFreecam(void);
static void ClearPanState(void);
static void ClampTileToMap(s16 *, s16 *);
static void CenterCameraOnTile(s16, s16);

static struct FieldCameraOffset sFieldCameraOffset;
static s16 sHorizontalCameraPan;
static s16 sVerticalCameraPan;
static bool8 sBikeCameraPanFlag;
static void (*sFieldCameraPanningCallback)(void);

// Debug freecam state: when active the camera detaches from the player and is
// scrolled directly by the D-pad (see UpdateFreecamMovement).
static bool8 sFreecamActive;
static s16 sFreecamMoveX;
static s16 sFreecamMoveY;

// Local id of the object event the camera is currently tracking (see
// TRACK_LOCAL_ID_PLAYER). Defaults to the player, repointed on the fly by the debug
// "Track NPC" tool.
static u8 sCameraTrackedLocalId;

// Camera auto-pan state: the camera detaches and scrolls toward a target, then rests on it. The
// target is an object event (sCameraPanLocalId, re-resolved each tile so a moving target is chased
// then locked onto) or, when sCameraPanToTile is set, a fixed tile (sCameraPanTargetX/Y) it rests on
// (sCameraPanArrived). Mutually exclusive with freecam. sCameraPanSpeed is a constant px/frame held
// for the whole pan, always a divisor of 16 so the camera lands cleanly on tile boundaries.
static bool8 sCameraPanActive;
static bool8 sCameraPanToTile;
static bool8 sCameraPanArrived;
static u8 sCameraPanLocalId;
static s16 sCameraPanTargetX;
static s16 sCameraPanTargetY;
static s16 sCameraPanSpeed;
static s16 sCameraPanMoveX;
static s16 sCameraPanMoveY;

// One-shot request (set when a field move is chosen) to snap the camera back to the player on the
// next return to the field instead of restoring tracking. Field-move animations (Fly, Surf,
// Sweet Scent...) are player-centred, so the camera must be on the player when they play.
static bool8 sResetCameraToPlayerOnFieldReturn;

COMMON_DATA struct CameraObject gFieldCamera = {0};
COMMON_DATA struct Coords16 gCameraPos = {0};
COMMON_DATA u16 gTotalCameraPixelOffsetY = 0;
COMMON_DATA u16 gTotalCameraPixelOffsetX = 0;
// Elevation level and biome of the camera's focus tile, tracked like gPlayerElevation/gPlayerBiome
// but from gCameraPos. Equal to the player's while attached, diverging only while detached.
EWRAM_DATA u8 gCameraElevation = 0;
EWRAM_DATA u8 gCameraBiome = 0;

static void ResetCameraOffset(struct FieldCameraOffset *cameraOffset)
{
    cameraOffset->xTileOffset = 0;
    cameraOffset->yTileOffset = 0;
    cameraOffset->xPixelOffset = 0;
    cameraOffset->yPixelOffset = 0;
}

static void AddCameraTileOffset(struct FieldCameraOffset *cameraOffset, u32 xOffset, u32 yOffset)
{
    cameraOffset->xTileOffset += xOffset;
    cameraOffset->xTileOffset %= 32;
    cameraOffset->yTileOffset += yOffset;
    cameraOffset->yTileOffset %= 32;
}

static void AddCameraPixelOffset(struct FieldCameraOffset *cameraOffset, u32 xOffset, u32 yOffset)
{
    cameraOffset->xPixelOffset += xOffset;
    cameraOffset->yPixelOffset += yOffset;
}

void ResetFieldCamera(void)
{
    ResetCameraOffset(&sFieldCameraOffset);
}

// Set whenever the compositor rewrites a field tilemap cell (DrawCompositeCell); consumed by the
// synchronous flush in FieldUpdateBgTilemapScroll.
static bool8 sFieldTilemapDirty;

// Copy a finished overworld BG tilemap buffer straight to its VRAM map base, synchronously.
static void FlushFieldBgTilemap(u8 bg, const u16 *buffer)
{
    DmaCopy32(3, buffer, (void *)(BG_VRAM + GetBgAttribute(bg, BG_ATTR_MAPBASEINDEX) * BG_SCREEN_SIZE), BG_SCREEN_SIZE);
}

void FieldUpdateBgTilemapScroll(void)
{
    u32 r4, r5;

    // Land this frame's slice redraw in VRAM BEFORE the scroll below reveals it. The compositor also
    // schedules these tilemaps through the DMA3 queue (which covers non-field contexts like map load),
    // but that queue drains after the scroll commits this VBlank and under fast movement can slip the
    // copy past scanout - so the viewport scrolls onto not-yet-resident edge tiles and tears. Flushing
    // synchronously here, ahead of the scroll, guarantees the new edge column is present with no lag.
    if (sFieldTilemapDirty)
    {
        FlushFieldBgTilemap(1, gOverworldTilemapBuffer_Bg1);
        FlushFieldBgTilemap(2, gOverworldTilemapBuffer_Bg2);
        FlushFieldBgTilemap(3, gOverworldTilemapBuffer_Bg3);
        sFieldTilemapDirty = FALSE;
    }

    r5 = sFieldCameraOffset.xPixelOffset + sHorizontalCameraPan;
    r4 = sVerticalCameraPan + sFieldCameraOffset.yPixelOffset + 8;

    SetGpuReg(REG_OFFSET_BG1HOFS, r5);
    SetGpuReg(REG_OFFSET_BG1VOFS, r4);
    SetGpuReg(REG_OFFSET_BG2HOFS, r5);
    SetGpuReg(REG_OFFSET_BG2VOFS, r4);
    SetGpuReg(REG_OFFSET_BG3HOFS, r5);
    SetGpuReg(REG_OFFSET_BG3VOFS, r4);
}


void GetCameraOffsetWithPan(s16 *x, s16 *y)
{
    *x = sFieldCameraOffset.xPixelOffset + sHorizontalCameraPan;
    *y = sFieldCameraOffset.yPixelOffset + sVerticalCameraPan + 8;
}

void DrawWholeMapView(void)
{
    DrawWholeMapViewInternal(gCameraPos.x, gCameraPos.y, gMapHeader.mapLayout);
}

static void DrawWholeMapViewInternal(int x, int y, const struct MapLayout *mapLayout)
{
    u8 i;
    u8 j;
    u32 r6;
    u8 temp;

    for (i = 0; i < 32; i += 2)
    {
        temp = sFieldCameraOffset.yTileOffset + i;
        if (temp >= 32)
            temp -= 32;
        r6 = temp * 32;
        for (j = 0; j < 32; j += 2)
        {
            temp = sFieldCameraOffset.xTileOffset + j;
            if (temp >= 32)
                temp -= 32;
            DrawMetatileAt(mapLayout, r6 + temp, x + j / 2, y + i / 2);
        }
    }
}

// Redraw only the on-screen metatiles in the secondary id range. Used after a location crossing
// swaps the secondary tileset in place: the map grid is unchanged (the player merely stepped over a
// boundary, camera tile-aligned), so every primary metatile still resolves to the exact same
// composite slot and re-resolving it is pure waste. Only secondary metatiles are reinterpreted by
// the new tileset, so only they need re-composing. NOT valid after a camera jump (view content
// changes wholesale) - use DrawWholeMapView there.
void DrawSecondaryMapView(void)
{
    const struct MapLayout *mapLayout = gMapHeader.mapLayout;
    int x = gCameraPos.x;
    int y = gCameraPos.y;
    u8 i;
    u8 j;
    u32 r6;
    u8 temp;

    for (i = 0; i < 32; i += 2)
    {
        temp = sFieldCameraOffset.yTileOffset + i;
        if (temp >= 32)
            temp -= 32;
        r6 = temp * 32;
        for (j = 0; j < 32; j += 2)
        {
            temp = sFieldCameraOffset.xTileOffset + j;
            if (temp >= 32)
                temp -= 32;
            {
                u16 metatileId = MapGridGetMetatileIdAt(x + j / 2, y + i / 2);
                if (metatileId >= NUM_METATILES_IN_PRIMARY)
                    DrawMetatileAtWithId(mapLayout, r6 + temp, x + j / 2, y + i / 2, metatileId);
            }
        }
    }
}

static void RedrawMapSlicesForCameraUpdate(struct FieldCameraOffset *cameraOffset, int x, int y)
{
    const struct MapLayout *mapLayout = gMapHeader.mapLayout;

    if (x > 0)
        RedrawMapSliceWest(cameraOffset, mapLayout);
    if (x < 0)
        RedrawMapSliceEast(cameraOffset, mapLayout);
    if (y > 0)
        RedrawMapSliceNorth(cameraOffset, mapLayout);
    if (y < 0)
        RedrawMapSliceSouth(cameraOffset, mapLayout);
}

static void RedrawMapSliceNorth(struct FieldCameraOffset *cameraOffset, const struct MapLayout *mapLayout)
{
    u8 i;
    u8 temp;
    u32 r7;

    temp = cameraOffset->yTileOffset + 28;
    if (temp >= 32)
        temp -= 32;
    r7 = temp * 32;
    for (i = 0; i < 32; i += 2)
    {
        temp = cameraOffset->xTileOffset + i;
        if (temp >= 32)
            temp -= 32;
        DrawMetatileAt(mapLayout, r7 + temp, gCameraPos.x + i / 2, gCameraPos.y + 14);
    }
}

static void RedrawMapSliceSouth(struct FieldCameraOffset *cameraOffset, const struct MapLayout *mapLayout)
{
    u8 i;
    u8 temp;
    u32 r7 = cameraOffset->yTileOffset * 32;

    for (i = 0; i < 32; i += 2)
    {
        temp = cameraOffset->xTileOffset + i;
        if (temp >= 32)
            temp -= 32;
        DrawMetatileAt(mapLayout, r7 + temp, gCameraPos.x + i / 2, gCameraPos.y);
    }
}

static void RedrawMapSliceEast(struct FieldCameraOffset *cameraOffset, const struct MapLayout *mapLayout)
{
    u8 i;
    u8 temp;
    u32 r6 = cameraOffset->xTileOffset;

    for (i = 0; i < 32; i += 2)
    {
        temp = cameraOffset->yTileOffset + i;
        if (temp >= 32)
            temp -= 32;
        DrawMetatileAt(mapLayout, temp * 32 + r6, gCameraPos.x, gCameraPos.y + i / 2);
    }
}

static void RedrawMapSliceWest(struct FieldCameraOffset *cameraOffset, const struct MapLayout *mapLayout)
{
    u8 i;
    u8 temp;
    u8 r5 = cameraOffset->xTileOffset + 28;

    if (r5 >= 32)
        r5 -= 32;
    for (i = 0; i < 32; i += 2)
    {
        temp = cameraOffset->yTileOffset + i;
        if (temp >= 32)
            temp -= 32;
        DrawMetatileAt(mapLayout, temp * 32 + r5, gCameraPos.x + 14, gCameraPos.y + i / 2);
    }
}

void CurrentMapDrawMetatileAt(int x, int y)
{
    int offset = MapPosToBgTilemapOffset(&sFieldCameraOffset, x, y);

    if (offset >= 0)
        DrawMetatileAt(gMapHeader.mapLayout, offset, x, y);
}

// BG tilemap offsets of a metatile's four 8x8 sub-tiles, in metatile sub-tile order
// (TL, TR, BL, BR). Each metatile spans a 2x2 cell block (rows are 0x20 apart).
static const u16 sSubTileCellOffsets[4] = {0, 1, 0x20, 0x21};

void DrawDoorMetatileAt(int x, int y, u16 *tiles)
{
    int offset = MapPosToBgTilemapOffset(&sFieldCameraOffset, x, y);
    u32 p;

    if (offset >= 0)
    {
        // Door "covered" semantics: the door's lower half (tiles[0..3]) sits in the background
        // plane behind the player, its upper half (tiles[4..7]) in the foreground plane in front.
        for (p = 0; p < 4; p++)
        {
            u16 bg = tiles[p];
            u16 fg = tiles[4 + p];
            DrawCompositeCell(offset + sSubTileCellOffsets[p], &bg, 1, &fg, 1, NULL, 0, FALSE, x, y);
        }
    }
}

static void DrawMetatileAt(const struct MapLayout *mapLayout, u16 offset, int x, int y)
{
    DrawMetatileAtWithId(mapLayout, offset, x, y, MapGridGetMetatileIdAt(x, y));
}

// Per-subtile layer-routing scratch for DrawMetatileAtWithId. EWRAM statics, not locals: that
// function sits under every deep redraw path (DrawWholeMapView during a wave heading flip, the
// scroll-edge redraws) and the system-stack budget is tight (see ProbeStackWatermark in overworld.c).
// Main-loop only, so statics are safe.
static EWRAM_DATA u16 sLayers[3] = {0};
// Each holds up to 4: the bgMaterial background layer plus all three of the tile's own layers. The
// material routes with the bottom layer's plane flags, so it can land in any of the three planes.
static EWRAM_DATA u16 sBg[4] = {0};
static EWRAM_DATA u16 sFg[4] = {0};
static EWRAM_DATA u16 sRefl[4] = {0};

// Layer content at a subtile, for the cliff-promotion scan. Layer 0 also counts the bgMaterial ground
// that fills an empty ground cell: the material stands in for the bottom layer, so it must promote
// with it to occlude a behind-cliff object under material-backed ground. NULL materialTiles = tile only.
static inline bool32 LayerSubtileHasContent(const u16 *tiles, const u16 *materialTiles, u32 layer, u32 sub)
{
    if ((tiles[layer * 4 + sub] & 0x3FF) != 0)
        return TRUE;
    return layer == 0 && materialTiles != NULL && (materialTiles[sub] & 0x3FF) != 0;
}

// As DrawMetatileAt, but with the metatile id already resolved (lets callers that have fetched it —
// e.g. the secondary-only redraw's id filter — skip a redundant map-grid lookup).
static void DrawMetatileAtWithId(const struct MapLayout *mapLayout, u16 offset, int x, int y, u16 metatileId)
{
    const u16 *tiles;
    // Set for flagged metatiles: primary metatile #bgMaterial (0-15)'s ground composites beneath the
    // tile's own layers in the empty bottom-layer cells. NULL = normal render.
    const u16 *materialTiles = NULL;
    // The bgMaterial metatile id (valid when materialTiles != NULL) and whether ITS OWN layer-0
    // reflection flags mark it reflective — the material routes to BG3/BG2 per its own setup, not the
    // overlay tile's, so an overlay whose layer 0 reflects can't drag a plain material onto BG3.
    u16 materialId = 0;
    bool32 materialReflects = FALSE;
    // True when an object stands behind a cliff at this tile: the cell composites using the
    // metatile's "behind cliff" layer flags instead of its "in front of cliff" flags.
    bool32 promote = IsCliffPromotedCell(x, y);
    // 3-bit mask (one bit per layer 0/1/2) selecting which layers go to the foreground plane for
    // this cell's current cliff state; the rest go to the background plane. See METATILE_COMPOSITE_*.
    u32 fgMask;
    u16 attributes;
    // 3-bit mask (one bit per layer 0/1/2) of layers routed to the BG3 reflective plane. They render
    // behind the reflection sprites and punch a hole through the BG2 background composite. Resolved
    // from the compositing byte's reflection-layer selector below, after fgMask is finalised.
    u32 reflMask;
    // When set, reflection layers hole-punch the bg plane; when clear they only draw to BG3.
    bool32 punchHole;
    u32 p;

    if (metatileId > NUM_METATILES_TOTAL)
        metatileId = 0;
    attributes = GetMetatileAttributesById(metatileId);
    // Only metatiles flagged "use bg material" take the bgMaterial render path — fetch the material
    // (a second map-grid read) only then, since the vast majority of metatiles don't use it.
    if (UNPACK_USES_BGMATERIAL(attributes))
    {
        materialId = MapGridGetBgMaterialAt(x, y);
        materialTiles = mapLayout->primaryTileset->metatiles + materialId * NUM_TILES_PER_METATILE;
    }
    if (metatileId < NUM_METATILES_IN_PRIMARY)
        tiles = mapLayout->primaryTileset->metatiles + metatileId * NUM_TILES_PER_METATILE;
    else
        tiles = GetActiveLocationData()->secondaryTileset->metatiles + (metatileId - NUM_METATILES_IN_PRIMARY) * NUM_TILES_PER_METATILE;

    fgMask = promote ? METATILE_COMPOSITE_BEHIND(GetMetatileCompositingById(metatileId))
                     : METATILE_COMPOSITE_FRONT(GetMetatileCompositingById(metatileId));
    // Behind an object, when the tile's promoted foreground layers are all blank — i.e. plain elevated
    // ground with no painted cliff face — promote every layer that HAS content so the whole ground tile
    // occludes the behind-object. (Promoting only the bottom layer left upper-layer content in the
    // background plane, drawn behind the sprite and hidden, so multi-layer ground showed only its base.)
    // When the promoted layers DO have content (a painted cliff face/fringe) that authored foreground
    // does the occluding; forcing extra layers forward would wrongly hide a partly-obscured sprite that
    // should still draw behind the (transparent-in-places) foreground layer.
    if (promote)
    {
        bool32 fgEmpty = TRUE;
        u32 l, sub;
        for (l = 0; l < 3 && fgEmpty; l++)
            if (fgMask & (1 << l))
                for (sub = 0; sub < 4; sub++)
                    if (LayerSubtileHasContent(tiles, materialTiles, l, sub)) { fgEmpty = FALSE; break; }
        if (fgEmpty)
            for (l = 0; l < 3; l++)
                for (sub = 0; sub < 4; sub++)
                    if (LayerSubtileHasContent(tiles, materialTiles, l, sub)) { fgMask |= (1 << l); break; }
    }

    // Reflection layers: the explicit per-layer flag mask wins; each background layer it names goes
    // to BG3 (multiple layers may be flagged). As a fallback, a metatile with a reflection type but
    // no flags routes ALL its background layers to BG3 (the "whole-tile reflective" case). Foreground
    // layers never reflect.
    reflMask = METATILE_COMPOSITE_REFLECTION(GetMetatileCompositingById(metatileId)) & ~fgMask;
    if (reflMask == 0 && UNPACK_REFLECTION(attributes) != METATILE_REFLECTION_NONE)
        reflMask = METATILE_COMPOSITE_LAYER_MASK & ~fgMask;
    // With the punch flag set, reflection layers hole-punch the BG2 composite so the surface shows
    // through opaque bg (the original behaviour). Without it they only draw to BG3, visible solely
    // where BG2 is transparent (BG3 sits behind BG2).
    punchHole = UNPACK_REFLECTION_PUNCH(attributes);

    // The material composites per its OWN reflection flags: its layer 0 reflects if the material
    // metatile flags that layer reflective, or (whole-tile-reflective fallback) it has a reflection
    // type with no explicit per-layer flags. Otherwise it draws to the bg plane, which sits above BG3.
    if (materialTiles)
    {
        u32 mRefl = METATILE_COMPOSITE_REFLECTION(GetMetatileCompositingById(materialId));
        materialReflects = (mRefl & (1 << 0))
                        || (mRefl == 0 && UNPACK_REFLECTION(GetMetatileAttributesById(materialId)) != METATILE_REFLECTION_NONE);
    }

    // For each of the metatile's four sub-tiles, route its three layers (ground/middle/top) into the
    // foreground, background, or reflection plane, preserving bottom->top order within each plane.
    // bgMaterial composites the material metatile's ground as an extra bottom layer (see below).
    for (p = 0; p < 4; p++)
    {
        u32 bgCount = 0, fgCount = 0, reflCount = 0;
        u32 l;

        // bgMaterial: the material metatile's ground layer composites beneath the tile's own three
        // layers (a 4th layer). Gated per-subtile: only EMPTY bottom-layer cells (id 0) get it — a
        // non-empty ground subtile fully occupies the cell. Foreground routing tracks the overlay's
        // BOTTOM layer (so behind a cliff the material promotes with the ground it stands in for);
        // reflection uses the MATERIAL's own flags, so a non-reflective material stays on the bg
        // plane even when the overlay's layer 0 reflects.
        if (materialTiles && (tiles[p] & 0x3FF) == 0)
        {
            if (fgMask & (1 << 0))
                sFg[fgCount++] = materialTiles[p];
            else if (materialReflects)
                sRefl[reflCount++] = materialTiles[p];
            else
                sBg[bgCount++] = materialTiles[p];
        }

        sLayers[0] = tiles[p];
        sLayers[1] = tiles[4 + p];
        sLayers[2] = tiles[8 + p];

        for (l = 0; l < 3; l++)
        {
            if (fgMask & (1 << l))
                sFg[fgCount++] = sLayers[l];
            else if (reflMask & (1 << l))
                sRefl[reflCount++] = sLayers[l];
            else
                sBg[bgCount++] = sLayers[l];
        }
        DrawCompositeCell(offset + sSubTileCellOffsets[p], sBg, bgCount, sFg, fgCount, sRefl, reflCount, punchHole, x, y);
    }
}

// Resolve composite slots for one 8x8 cell and write them to the three plane tilemaps. The cell's
// current slots (stored as the tilemap tile indices) are released after acquiring the new ones, so
// an unchanged recipe keeps its slot alive across the swap. Each plane is independent:
//   BG1 (fg)   = foreground entries
//   BG2 (bg)   = background entries, with holes punched wherever a reflection entry is opaque
//   BG3 (refl) = reflection entries (the reflective surface, drawn behind the reflection sprites)
// With punchHole set the reflection entries drive both BG3 (drawn) and the BG2 mask, so the surface
// shows through the hole it cuts in the background plane. With it clear the reflection only draws to
// BG3, visible solely where BG2 happens to be transparent (BG3 sits behind BG2).
static void DrawCompositeCell(u16 offset, const u16 *bgEntries, u32 bgCount, const u16 *fgEntries, u32 fgCount, const u16 *reflEntries, u32 reflCount, bool32 punchHole, s16 x, s16 y)
{
    u16 oldFg = gOverworldTilemapBuffer_Bg1[offset] & 0x3FF;
    u16 oldBg = gOverworldTilemapBuffer_Bg2[offset] & 0x3FF;
    u16 oldRefl = gOverworldTilemapBuffer_Bg3[offset] & 0x3FF;
    u16 newFg = FieldCompositorAcquire(fgEntries, fgCount, x, y);
    u16 newBg = punchHole ? FieldCompositorAcquireMasked(bgEntries, bgCount, reflEntries, reflCount, x, y)
                          : FieldCompositorAcquire(bgEntries, bgCount, x, y);
    u16 newRefl = FieldCompositorAcquire(reflEntries, reflCount, x, y);

    // On pool exhaustion Acquire returns COMPOSITE_SLOT_KEEP: leave the cell's current tile in place
    // (keep its slot, don't write the sentinel) so it shows stale content for a frame instead of
    // flashing blank. The redraw is retried so it resolves once a slot frees.
    if (newFg != COMPOSITE_SLOT_KEEP)
    {
        FieldCompositorRelease(oldFg);
        gOverworldTilemapBuffer_Bg1[offset] = newFg;
    }
    if (newBg != COMPOSITE_SLOT_KEEP)
    {
        FieldCompositorRelease(oldBg);
        gOverworldTilemapBuffer_Bg2[offset] = newBg;
    }
    if (newRefl != COMPOSITE_SLOT_KEEP)
    {
        FieldCompositorRelease(oldRefl);
        gOverworldTilemapBuffer_Bg3[offset] = newRefl;
    }

    // Flag the synchronous, pre-scroll VRAM flush (see FieldUpdateBgTilemapScroll). While the field
    // VBlank is installed that flush copies the plane tilemaps every frame, so the DMA3-queued copy
    // below would just write the same buffers to the same VRAM a second time - skip it. During map
    // load (field VBlank not yet active) the sync flush doesn't run, so fall back to the queue.
    sFieldTilemapDirty = TRUE;
    if (!IsFieldBgTilemapFlushedInVBlank())
    {
        ScheduleBgCopyTilemapToVram(1);
        ScheduleBgCopyTilemapToVram(2);
        ScheduleBgCopyTilemapToVram(3);
    }
}

// Tiles (map-grid coords, the space DrawMetatileAt and object currentCoords share) whose middle
// layer is currently promoted into the foreground plane to occlude an object hiding behind a cliff.
// Rebuilt each frame from object positions. Sized for each object's full sprite footprint (width x
// height in tiles) at both its current and destination tile while stepping; a handful of tiles each.
#define MAX_CLIFF_PROMOTED_TILES (OBJECT_EVENTS_COUNT * 6)
// In EWRAM: IWRAM is nearly full, and these are referenced while UpdateCliffFacePromotion descends into
// the compositor's deep redraw chain, so keeping them (and the per-frame prev snapshot) off the IWRAM
// stack/bss avoids overflowing into other globals.
EWRAM_DATA static struct Coords16 sCliffPromotedTiles[MAX_CLIFF_PROMOTED_TILES] = {0};
EWRAM_DATA static struct Coords16 sCliffPromotedTilesPrev[MAX_CLIFF_PROMOTED_TILES] = {0};
EWRAM_DATA static u8 sCliffPromotedTileCount = 0;

// Whether a foreground cell should promote its middle layer (see DrawMetatileAt): true when a
// behind-cliff object's sprite covers this tile, so the cliff's middle layer draws over it.
static bool32 IsCliffPromotedCell(int x, int y)
{
    u32 i;

    for (i = 0; i < sCliffPromotedTileCount; i++)
    {
        if (sCliffPromotedTiles[i].x == x && sCliffPromotedTiles[i].y == y)
            return TRUE;
    }
    return FALSE;
}

// Public query: is this tile currently cliff-promoted (a behind-object is hiding under it)? A FRONT
// object overlapping such a tile splits its sprite so the overlapping rows draw over the promotion
// (see UpdateObjectEventFrontSplit) rather than being wrongly hidden by it.
bool32 IsTileCliffPromoted(s16 x, s16 y)
{
    return IsCliffPromotedCell(x, y);
}

static void AddCliffPromotedTile(s16 x, s16 y)
{
    u32 i;

    for (i = 0; i < sCliffPromotedTileCount; i++)
    {
        if (sCliffPromotedTiles[i].x == x && sCliffPromotedTiles[i].y == y)
            return;
    }
    if (sCliffPromotedTileCount < MAX_CLIFF_PROMOTED_TILES)
    {
        sCliffPromotedTiles[sCliffPromotedTileCount].x = x;
        sCliffPromotedTiles[sCliffPromotedTileCount].y = y;
        sCliffPromotedTileCount++;
    }
}

static bool32 WasInTileList(const struct Coords16 *list, u8 count, s16 x, s16 y)
{
    u32 i;

    for (i = 0; i < count; i++)
    {
        if (list[i].x == x && list[i].y == y)
            return TRUE;
    }
    return FALSE;
}

// Promote every tile of the sprite's footprint when the object stands (feet) at tile (cx, cy): the
// sprite is bottom-anchored and horizontally centred on that tile, so it spans ceil(width/16) tile
// columns either side of centre and ceil(height/16) rows up from the feet. Each covered tile higher
// than the frozen base (baseElevation) is a cliff face in front of that band, so its middle layer
// is promoted over the sprite; rows/cols at or below the base are normal ground it stands in front of.
// Only called for objects whose feet are behind a cliff (see UpdateCliffFacePromotion) — a FRONT object
// at the base of a taller cliff is in front of the wall, so its head must NOT be promoted behind it.
static void PromoteSpriteFootprint(struct ObjectEvent *objEvent, const struct ObjectEventGraphicsInfo *graphicsInfo, s16 cx, s16 cy)
{
    s16 half = graphicsInfo->width / 2;
    s16 colStart = (8 - half) >> 4;       // left edge px (centre at +8) -> column, floored
    s16 colEnd = (8 + half - 1) >> 4;     // right edge px (inclusive) -> column, floored
    s16 rows = (graphicsInfo->height + 15) / 16;
    s16 c, r;

    for (r = 0; r < rows; r++)
        for (c = colStart; c <= colEnd; c++)
            if (MapGridGetElevationAt(cx + c, cy - r) > objEvent->baseElevation)
                AddCliffPromotedTile(cx + c, cy - r);
}

// The surf unit (blob + 32-wide surfer) overhangs the surfer's tile by 8px south and to both
// sides. Promote overlapped neighbouring cliff tiles even while the surfer is FRONT, so the
// overhang hides behind them — surfing alongside a wall reads as partially behind it.
// South row: any tile off the surfer's level (face in front while hiding, or the face below at a
// top edge). Side columns: only when the FEET-row side tile rises above the level — then the whole
// wall column beside the surfer promotes. Gating on the feet row keeps a NORTH wall's diagonals
// unpromoted: a surfer south of a wall is in front of it, and their whole head row must stay
// drawn over it (promoting a diagonal would clip the head's side pixels while its center shows).
static void PromoteSurfBlobOverhang(struct ObjectEvent *objEvent, const struct ObjectEventGraphicsInfo *graphicsInfo, s16 cx, s16 cy)
{
    s16 half = graphicsInfo->width / 2;
    s16 colStart = (8 - half) >> 4;
    s16 colEnd = (8 + half - 1) >> 4;
    s16 rows = (graphicsInfo->height + 15) / 16;
    s16 c, r;

    for (c = colStart; c <= colEnd; c++)
    {
        if (MapGridGetElevationAt(cx + c, cy + 1) != objEvent->baseElevation)
            AddCliffPromotedTile(cx + c, cy + 1);
        if (c == 0 || MapGridGetElevationAt(cx + c, cy) <= objEvent->baseElevation)
            continue;
        for (r = 0; r < rows; r++)
            if (MapGridGetElevationAt(cx + c, cy - r) > objEvent->baseElevation)
                AddCliffPromotedTile(cx + c, cy - r);
    }
}

// Redraw a promoted/unpromoted cliff tile, but only when it is safely on-screen. The BG is a 32-tile
// (16-metatile) torus while the view is 15 metatiles wide, so the 16th column (dx == 15, one metatile
// past the right edge) shares its torus cell with the tile one step off the LEFT edge (dx == -1) —
// whose far sub-column is briefly visible as a sliver mid-scroll. An object stays live up to 3
// metatiles off-screen (see RemoveObjectEventIfOutsideView), so its promotion can reach that aliasing
// column: redrawing it there would stamp the object's foreground onto the opposite edge's sliver
// (the reported "stale tile flashes on the far edge" glitch). The row axis (dy == 15) aliases the same
// way. These tiles are off-screen (a 1-column sliver at most), so skip them; the slice redraw refreshes
// their torus cell with the live state when they actually scroll into view.
static void RedrawCliffTile(s16 x, s16 y)
{
    if ((u32)(x - gCameraPos.x) >= 15 || (u32)(y - gCameraPos.y) >= 15)
        return;
    CurrentMapDrawMetatileAt(x, y);
}

// Rebuild the cliff-promoted tile set from current object positions and redraw any tile that
// entered or left it (its foreground recipe changed). Called once per frame from the overworld.
void UpdateCliffFacePromotion(void)
{
    struct Coords16 *prev = sCliffPromotedTilesPrev;
    u8 prevCount = sCliffPromotedTileCount;
    u32 i;

    for (i = 0; i < prevCount; i++)
        prev[i] = sCliffPromotedTiles[i];
    sCliffPromotedTileCount = 0;

    for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
    {
        struct ObjectEvent *objEvent = &gObjectEvents[i];
        const struct ObjectEventGraphicsInfo *graphicsInfo;

        if (!objEvent->active || objEvent->drawAtHighestElevation)
            continue;

        graphicsInfo = GetObjectEventGraphicsInfo(objEvent->graphicsId);

        // Occlusion is gated on the foot-based cliff state, not per-tile elevation: only an object whose
        // feet are behind the cliff (BEHIND/OBSCURED) is promoted. This stops a FRONT object's head from
        // flipping "behind" just because the tile north of it is higher terrain it stands in front of.
        // Cover the sprite's whole footprint at the tile it is leaving and the tile it is entering, so a
        // mid-step sprite straddling both is occluded across its full span (and its destination is set up
        // before it arrives). The two footprints coincide when not stepping; AddCliffPromotedTile dedups.
        if (objEvent->cliffLayer != CLIFF_LAYER_FRONT)
        {
            PromoteSpriteFootprint(objEvent, graphicsInfo, objEvent->currentCoords.x, objEvent->currentCoords.y);
            PromoteSpriteFootprint(objEvent, graphicsInfo, objEvent->previousCoords.x, objEvent->previousCoords.y);
        }

        // The surf unit's overhang needs covering in FRONT too (top edges, walls alongside).
        if (objEvent->isPlayer && (gPlayerAvatar.flags & PLAYER_AVATAR_FLAG_SURFING))
        {
            PromoteSurfBlobOverhang(objEvent, graphicsInfo, objEvent->currentCoords.x, objEvent->currentCoords.y);
            PromoteSurfBlobOverhang(objEvent, graphicsInfo, objEvent->previousCoords.x, objEvent->previousCoords.y);
        }
    }

    // Redraw tiles whose promotion state flipped. Off-screen tiles are skipped by
    // RedrawCliffTile (and pick up the live state via the slice redraw when they scroll into view).
    for (i = 0; i < prevCount; i++)
        if (!IsCliffPromotedCell(prev[i].x, prev[i].y))
            RedrawCliffTile(prev[i].x, prev[i].y);
    for (i = 0; i < sCliffPromotedTileCount; i++)
        if (!WasInTileList(prev, prevCount, sCliffPromotedTiles[i].x, sCliffPromotedTiles[i].y))
            RedrawCliffTile(sCliffPromotedTiles[i].x, sCliffPromotedTiles[i].y);
}

static s32 MapPosToBgTilemapOffset(struct FieldCameraOffset *cameraOffset, s32 x, s32 y)
{
    x -= gCameraPos.x;
    x *= 2;
    if (x >= 32 || x < 0)
        return -1;
    x = x + cameraOffset->xTileOffset;
    if (x >= 32)
        x -= 32;

    y = (y - gCameraPos.y) * 2;
    if (y >= 32 || y < 0)
        return -1;
    y = y + cameraOffset->yTileOffset;
    if (y >= 32)
        y -= 32;

    return y * 32 + x;
}

static void CameraUpdateCallback(struct CameraObject *fieldCamera)
{
    if (sFreecamActive)
    {
        UpdateFreecamMovement(fieldCamera);
        return;
    }
    if (sCameraPanActive)
    {
        UpdateCameraPanMovement(fieldCamera);
        return;
    }
    if (fieldCamera->spriteId != 0)
    {
        fieldCamera->movementSpeedX = gSprites[fieldCamera->spriteId].sCamera_MoveX;
        fieldCamera->movementSpeedY = gSprites[fieldCamera->spriteId].sCamera_MoveY;
    }
}

// Drives the camera from the D-pad while freecam is active. Direction is only (re)evaluated on a
// tile boundary so an in-progress step finishes grid-aligned, and movement is clamped to the map
// bounds. Scrolling halts while the player's field controls are locked, so an open menu takes the
// D-pad without the camera drifting underneath it.
static void UpdateFreecamMovement(struct CameraObject *fieldCamera)
{
    if (fieldCamera->x == 0 && fieldCamera->y == 0)
    {
        u16 keys = ArePlayerFieldControlsLocked() ? 0 : gMain.heldKeys;

        sFreecamMoveX = 0;
        sFreecamMoveY = 0;
        // Roam onto any tile backed by real map data (the home map or a stitched connection), and stop
        // at the edge of the connected plane instead of drifting into empty space. No longer clamped to
        // the home map's own bounds, so the camera crosses connection seams (diagonally too).
        if ((keys & DPAD_RIGHT) && IsCameraTileDefined(gCameraPos.x + 1, gCameraPos.y))
            sFreecamMoveX = FREECAM_SPEED;
        else if ((keys & DPAD_LEFT) && IsCameraTileDefined(gCameraPos.x - 1, gCameraPos.y))
            sFreecamMoveX = -FREECAM_SPEED;
        if ((keys & DPAD_DOWN) && IsCameraTileDefined(gCameraPos.x, gCameraPos.y + 1))
            sFreecamMoveY = FREECAM_SPEED;
        else if ((keys & DPAD_UP) && IsCameraTileDefined(gCameraPos.x, gCameraPos.y - 1))
            sFreecamMoveY = -FREECAM_SPEED;
    }
    fieldCamera->movementSpeedX = sFreecamMoveX;
    fieldCamera->movementSpeedY = sFreecamMoveY;
}

bool8 IsFreecamActive(void)
{
    return sFreecamActive;
}

void SetFreecamActive(bool8 active)
{
    sFreecamActive = active;
    sFreecamMoveX = 0;
    sFreecamMoveY = 0;
    // Freecam takes over from any pan in progress (or one resting on its destination tile).
    if (active)
        ClearPanState();
}

// The camera is "detached" from the player while freecam is running or while tracking
// an object other than the player (local id 0). The overworld uses this to keep the
// player parked (no walking, no field interactions) while still allowing menus.
bool8 IsCameraDetachedFromPlayer(void)
{
    return sFreecamActive || sCameraPanActive || sCameraTrackedLocalId != TRACK_LOCAL_ID_PLAYER;
}

// Drops the camera's object anchor from every object event. The trackedByCamera object is the one
// the culling/redraw scrolls around, so clearing it before a rebuild stops the camera resyncing
// onto the object it is about to stop following.
static void ClearCameraAnchors(void)
{
    u8 i;

    for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
        gObjectEvents[i].trackedByCamera = FALSE;
}

// Makes a spawned object the camera's anchor and follows its sprite. It must be live and moving for
// the camera to track it, so clear any half-finished step and unfreeze it (an on-screen target was
// frozen when the menu opened). The location swap is silent (a deliberate repoint, not the player
// walking; a popup would corrupt the open debug box).
static void AnchorCameraToObject(struct ObjectEvent *tracked)
{
    tracked->trackedByCamera = TRUE;
    tracked->singleMovementActive = FALSE;
    ObjectEventClearHeldMovement(tracked);
    UnfreezeObjectEvent(tracked);
    CameraObjectSetFollowedSpriteId(tracked->spriteId);
    TryUpdateMapLocationSilent(gCameraPos.x + MAP_OFFSET, gCameraPos.y + MAP_OFFSET);
}

// Clears freecam back to inactive and stationary.
static void ClearFreecam(void)
{
    sFreecamActive = FALSE;
    sFreecamMoveX = 0;
    sFreecamMoveY = 0;
}

// Clears all auto-pan state.
static void ClearPanState(void)
{
    sCameraPanActive = FALSE;
    sCameraPanToTile = FALSE;
    sCameraPanArrived = FALSE;
    sCameraPanMoveX = 0;
    sCameraPanMoveY = 0;
}

// Clamps a tile (map coordinates without MAP_OFFSET) to the current map's bounds.
static void ClampTileToMap(s16 *x, s16 *y)
{
    int width = gMapHeader.mapLayout->width;
    int height = gMapHeader.mapLayout->height;

    if (*x < 0)
        *x = 0;
    else if (*x > width - 1)
        *x = width - 1;
    if (*y < 0)
        *y = 0;
    else if (*y > height - 1)
        *y = height - 1;
}

// Jumps the camera focus to a tile (map coordinates without MAP_OFFSET, clamped to the current
// map) and rebuilds a clean, tile-aligned view there: sub-tile scroll offsets zeroed, scene
// reculled/respawned, and every object sprite snapped into the new frame.
static void CenterCameraOnTile(s16 x, s16 y)
{
    bool8 cameraMoved;
    bool8 gfxReloaded;
    u8 i;

    // Clamp to home-map bounds only for undefined tiles; a tracked object on a connected map is
    // outside home bounds but on a defined tile, and must stay reachable (as the freecam roams).
    if (!IsCameraTileDefined(x, y))
        ClampTileToMap(&x, &y);

    // Only rebuild the view when the focus actually moves (or the camera is mid-step). Re-centring
    // on the tile the camera already rests on must not zero the offset and redraw: that would
    // rewrite the BG buffer away from the correct alignment it inherited from the map-entry view,
    // causing a one-off viewport shift. With no movement the BG is already correct, so leave it be.
    cameraMoved = (gCameraPos.x != x || gCameraPos.y != y || gFieldCamera.x != 0 || gFieldCamera.y != 0);

    gCameraPos.x = x;
    gCameraPos.y = y;

    if (cameraMoved)
    {
        gFieldCamera.x = 0;
        gFieldCamera.y = 0;
        gTotalCameraPixelOffsetX = 0;
        gTotalCameraPixelOffsetY = 0;
        ResetFieldCamera();
    }

    // Respawn/cull around the new focus, then snap every live sprite into the new frame so none are
    // left at a stale sub-tile position. UpdateObjectEventsForCameraUpdate culls before it spawns,
    // so a far jump frees the old view's slots before filling them with the new context's objects.
    UpdateObjectEventsForCameraUpdate(0, 0);
    // Re-align the tile buffer with the (possibly jumped) focus before the gfx update and redraw below
    // read it: on a jump back onto the home map this restores the canonical buffer, undoing any roam.
    StitchCameraView();
    for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
    {
        if (gObjectEvents[i].active)
            MoveObjectEventToMapCoords(&gObjectEvents[i], gObjectEvents[i].currentCoords.x, gObjectEvents[i].currentCoords.y);
    }
    // The jump may land in a different location's tileset; apply it synchronously before the redraw
    // so the new surroundings don't flash with the previous focus's tileset for a frame. Safe to do
    // synchronously because a jump lands tile-aligned, with no sub-tile scroll offset to corrupt.
    gfxReloaded = UpdateMapLocationGfxImmediate(gCameraPos.x + MAP_OFFSET, gCameraPos.y + MAP_OFFSET);
    // Redraw when the focus moved or a tileset swap changed the resident graphics; a pure re-centre
    // on the resting tile needs neither.
    if (cameraMoved || gfxReloaded)
        DrawWholeMapView();
    UpdateCameraElevation();
    UpdateCameraBiome();
}

// Finds the spawn position of an object event template by local id, in object-event
// coordinates (i.e. including MAP_OFFSET). Returns FALSE if the current map has no
// such template.
static bool8 GetObjectEventTemplateCoordsByLocalId(u8 localId, s16 *x, s16 *y)
{
    u8 i;

    if (gMapHeader.events == NULL)
        return FALSE;

    for (i = 0; i < gMapHeader.events->objectEventCount; i++)
    {
        struct ObjectEventTemplate *template = &gSaveBlock1Ptr->objectEventTemplates[i];
        if (template->localId == localId)
        {
            *x = template->x + MAP_OFFSET;
            *y = template->y + MAP_OFFSET;
            return TRUE;
        }
    }
    return FALSE;
}

// Returns the currently-spawned object event for a track id (0 = player), or
// OBJECT_EVENTS_COUNT if it isn't spawned right now (e.g. culled off-screen).
static u8 ResolveTrackedObjectEvent(u8 localId)
{
    if (localId == TRACK_LOCAL_ID_PLAYER)
    {
        u8 objectEventId = gPlayerAvatar.objectEventId;
        return gObjectEvents[objectEventId].active ? objectEventId : OBJECT_EVENTS_COUNT;
    }
    // Returns OBJECT_EVENTS_COUNT when the object isn't spawned right now.
    return GetObjectEventIdByLocalIdAndMap(localId, gSaveBlock1Ptr->location.mapNum,
                                           gSaveBlock1Ptr->location.mapGroup);
}

// Tracks an object event by local id (0 = the player), changeable on the fly. Stable across
// culling/respawn: a culled target moves the camera onto its default position (map template, or the
// saved tile for the player), recreating the scene and respawning it; whatever object now bears
// that local id is then followed, so a respawn into a different gObjectEvents slot is transparent.
void SetCameraTrackedLocalId(u8 localId)
{
    u8 objectEventId = ResolveTrackedObjectEvent(localId);
    s16 targetX, targetY;

    // An instant track-jump supersedes any auto-pan in progress.
    sCameraPanActive = FALSE;

    if (objectEventId < OBJECT_EVENTS_COUNT)
    {
        // Currently spawned: aim at where it stands.
        targetX = gObjectEvents[objectEventId].currentCoords.x;
        targetY = gObjectEvents[objectEventId].currentCoords.y;
    }
    else if (localId == TRACK_LOCAL_ID_PLAYER)
    {
        // The player object can be culled like any other; recover its tile from the save.
        targetX = gSaveBlock1Ptr->playerPos.x + MAP_OFFSET;
        targetY = gSaveBlock1Ptr->playerPos.y + MAP_OFFSET;
    }
    else if (GetDisplacedObjectFrameCoordsByLocalId(localId, gSaveBlock1Ptr->location.mapNum,
                                                    gSaveBlock1Ptr->location.mapGroup, &targetX, &targetY))
    {
        // Culled but wandered onto a connected map: aim at its stored position there.
    }
    else if (!GetObjectEventTemplateCoordsByLocalId(localId, &targetX, &targetY))
    {
        return; // No such object event in this map.
    }

    // Drop the previous anchor before the rebuild, so snapping sprites into the new frame
    // doesn't resync the camera onto the object we're about to stop following.
    ClearCameraAnchors();

    // Jump to the target, rebuilding a clean tile-aligned scene around it (also respawns it
    // if it was culled).
    CenterCameraOnTile(targetX - MAP_OFFSET, targetY - MAP_OFFSET);
    sCameraTrackedLocalId = localId;

    // Follow it if it is (now) spawned. A flag-removed object never spawns, so the camera
    // simply rests on its default position with nothing to follow.
    objectEventId = ResolveTrackedObjectEvent(localId);
    if (objectEventId < OBJECT_EVENTS_COUNT)
        AnchorCameraToObject(&gObjectEvents[objectEventId]);
}

u8 GetCameraTrackedLocalId(void)
{
    return sCameraTrackedLocalId;
}

// Returns the object event the camera is currently following (the player by default, or a debug-
// tracked NPC), or NULL when nothing is anchored (freecam, or the target is culled/flag-removed).
struct ObjectEvent *GetCameraTrackedObjectEvent(void)
{
    u8 i;

    for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
    {
        if (gObjectEvents[i].active && gObjectEvents[i].trackedByCamera)
            return &gObjectEvents[i];
    }
    return NULL;
}

// Reattaches the camera to the player, used when leaving freecam.
void RecenterCameraOnPlayer(void)
{
    SetCameraTrackedLocalId(TRACK_LOCAL_ID_PLAYER);
}

// Drops the camera's object anchor where it sits, without snapping anywhere (unlike
// RecenterCameraOnPlayer). Used when freecam takes over from NPC tracking so the freecam
// starts at the tracked object's position with nothing pulling the camera back to it.
void StopCameraObjectTracking(void)
{
    ClearCameraAnchors();
    sCameraTrackedLocalId = TRACK_LOCAL_ID_PLAYER;
    ClearPanState();
}

// Re-establishes the active debug camera mode after a return to the field, which routes through
// SetCameraToTrackPlayer and would otherwise snap the camera back to the player. The tracking
// statics and gCameraPos persist across the trip, so reusing SetCameraTrackedLocalId resumes
// tracking: it re-anchors a spawned target or jumps to and respawns a culled one.
void RestoreCameraTracking(void)
{
    // A field move is about to play; its animation is player-centred, so snap the camera onto the
    // player instead of restoring tracking (SetCameraToTrackPlayer only re-anchors, it doesn't move
    // the focus, which was sitting on the tracked object).
    if (sResetCameraToPlayerOnFieldReturn)
    {
        ResetCameraTracking();
        RecenterCameraOnPlayer();
        return;
    }

    if (sFreecamActive)
    {
        // Freecam is D-pad driven and ignores the followed sprite; drop the player anchor
        // SetCameraToTrackPlayer just added so nothing pulls the camera.
        ClearCameraAnchors();
        return;
    }
    if (sCameraTrackedLocalId != TRACK_LOCAL_ID_PLAYER)
        SetCameraTrackedLocalId(sCameraTrackedLocalId);
}

// Clears all debug camera state back to following the player. Called on a full map load (warp),
// where any tracked object belonged to the old map.
void ResetCameraTracking(void)
{
    ClearFreecam();
    ClearPanState();
    sCameraTrackedLocalId = TRACK_LOCAL_ID_PLAYER;
    sResetCameraToPlayerOnFieldReturn = FALSE;
}

// Requests that the next return to the field snap the camera back to the player (cancelling any
// debug tracking/freecam), consumed by RestoreCameraTracking. Called when a field move is chosen,
// since every field move funnels through the party menu and its animation must play on the player.
void RequestCameraResetToPlayerOnFieldReturn(void)
{
    sResetCameraToPlayerOnFieldReturn = TRUE;
}

// Resolves the tile the pan is heading for, in gCameraPos space (no MAP_OFFSET). A spawned target
// gives its live tile (so a moving NPC is chased); a culled one gives its default position so the
// camera heads there until the scroll spawns it. Returns the spawned object event id, or
// OBJECT_EVENTS_COUNT if the target isn't spawned right now.
static u8 GetPanTargetTile(u8 localId, s16 *x, s16 *y)
{
    u8 objectEventId = ResolveTrackedObjectEvent(localId);
    s16 templateX, templateY;

    if (objectEventId < OBJECT_EVENTS_COUNT)
    {
        *x = gObjectEvents[objectEventId].currentCoords.x - MAP_OFFSET;
        *y = gObjectEvents[objectEventId].currentCoords.y - MAP_OFFSET;
        return objectEventId;
    }

    if (localId == TRACK_LOCAL_ID_PLAYER)
    {
        *x = gSaveBlock1Ptr->playerPos.x;
        *y = gSaveBlock1Ptr->playerPos.y;
    }
    else if (GetDisplacedObjectFrameCoordsByLocalId(localId, gSaveBlock1Ptr->location.mapNum,
                                                    gSaveBlock1Ptr->location.mapGroup, &templateX, &templateY))
    {
        // Culled but wandered onto a connected map: head for its stored position there.
        *x = templateX - MAP_OFFSET;
        *y = templateY - MAP_OFFSET;
    }
    else if (GetObjectEventTemplateCoordsByLocalId(localId, &templateX, &templateY))
    {
        *x = templateX - MAP_OFFSET;
        *y = templateY - MAP_OFFSET;
    }
    else
    {
        // No such object event in this map: stay put.
        *x = gCameraPos.x;
        *y = gCameraPos.y;
    }
    return OBJECT_EVENTS_COUNT;
}

// Ends the pan and hands the camera off to ordinary sprite-following of the object it arrived at.
static void LockCameraOntoPanTarget(u8 objectEventId)
{
    ClearPanState();
    AnchorCameraToObject(&gObjectEvents[objectEventId]);
}

// Picks the pan scroll speed: the divisor of 16 (1, 2, 4, 8 or 16 px/frame) nearest to `ideal`.
// A speed that evenly divides a 16px tile lands the camera on tile boundaries (where CameraUpdate
// redraws); one that didn't would skip boundaries and tear. Clamped to [1, 16].
static s16 SnapPanSpeed(s32 ideal)
{
    if (ideal < 2)        // midpoint of 1 and 2 (1.5) rounded
        return 1;
    else if (ideal < 3)   // midpoint of 2 and 4
        return 2;
    else if (ideal < 6)   // midpoint of 4 and 8
        return 4;
    else if (ideal < 12)  // midpoint of 8 and 16
        return 8;
    else
        return 16;
}

// Chooses sCameraPanSpeed to reach (targetX, targetY) from the current focus tile in `panFrames`
// frames: the longer axis distance divided by the frame budget gives the ideal px/frame, snapped to
// a tile divisor. Call after the camera is tile-aligned so gCameraPos is the true origin.
static void SetPanSpeedForReach(s16 targetX, s16 targetY, u8 panFrames)
{
    s16 dx = targetX - gCameraPos.x;
    s16 dy = targetY - gCameraPos.y;
    s16 reachTiles;
    u16 frames = (panFrames == 0) ? 1 : panFrames;

    if (dx < 0)
        dx = -dx;
    if (dy < 0)
        dy = -dy;
    reachTiles = (dx > dy) ? dx : dy;
    sCameraPanSpeed = SnapPanSpeed((s32)reachTiles * 16 / frames);
}

// Drives the camera toward the pan target while a pan is active, reusing CameraUpdate's tile-
// stepping at the constant sCameraPanSpeed. Direction is only (re)evaluated on a tile boundary so
// each step finishes grid-aligned. For an object pan the target tile is re-resolved every boundary
// (so a moving NPC is chased, and a culled target is picked up once the scroll spawns it) and on
// arrival the camera locks onto it and the pan ends. For a tile pan there is nothing to follow, so
// the camera rests on the fixed target tile (sCameraPanArrived) until another mode takes over.
static void UpdateCameraPanMovement(struct CameraObject *fieldCamera)
{
    if (fieldCamera->x == 0 && fieldCamera->y == 0)
    {
        s16 targetX, targetY;
        u8 objectEventId = OBJECT_EVENTS_COUNT;

        if (sCameraPanToTile)
        {
            targetX = sCameraPanTargetX;
            targetY = sCameraPanTargetY;
        }
        else
        {
            objectEventId = GetPanTargetTile(sCameraPanLocalId, &targetX, &targetY);
        }

        sCameraPanMoveX = 0;
        sCameraPanMoveY = 0;
        if (gCameraPos.x < targetX)
            sCameraPanMoveX = sCameraPanSpeed;
        else if (gCameraPos.x > targetX)
            sCameraPanMoveX = -sCameraPanSpeed;
        if (gCameraPos.y < targetY)
            sCameraPanMoveY = sCameraPanSpeed;
        else if (gCameraPos.y > targetY)
            sCameraPanMoveY = -sCameraPanSpeed;

        // Focus tile now matches the target tile.
        if (sCameraPanMoveX == 0 && sCameraPanMoveY == 0)
        {
            if (sCameraPanToTile)
            {
                // No object to follow: rest on the tile, latching arrival so callers can react.
                sCameraPanArrived = TRUE;
            }
            else
            {
                // A culled target may not be spawned yet, and with the camera at rest no CameraMove
                // runs the spawn pass; force one here (as CenterCameraOnTile does) so the pan doesn't
                // deadlock on an empty tile. The forced pass spawns the target if its tile is in view.
                if (objectEventId >= OBJECT_EVENTS_COUNT)
                {
                    UpdateObjectEventsForCameraUpdate(0, 0);
                    objectEventId = ResolveTrackedObjectEvent(sCameraPanLocalId);
                }
                if (objectEventId < OBJECT_EVENTS_COUNT)
                    LockCameraOntoPanTarget(objectEventId);
            }
        }
    }
    fieldCamera->movementSpeedX = sCameraPanMoveX;
    fieldCamera->movementSpeedY = sCameraPanMoveY;
}

// Takes the camera over from freecam or a track/pan already running, ready to start a new pan:
// clears freecam and drops every anchor so the upcoming scroll isn't resynced onto the old target,
// then re-centres on the current focus tile. The camera is often mid-step when a pan is triggered
// (still following the previously-tracked object, whose sprite isn't input-gated to tile
// boundaries); UpdateCameraPanMovement only (re)evaluates direction on a boundary, so a held
// sub-tile offset would never return to 0 and the camera would stall. Re-centring zeroes the offset.
static void DetachCameraForPan(void)
{
    ClearFreecam();
    ClearCameraAnchors();
    CenterCameraOnTile(gCameraPos.x, gCameraPos.y);
}

// Smoothly pans the camera to an object event (0 = the player), as an alternative to
// SetCameraTrackedLocalId's instant jump; on arrival it locks on the same way. `panFrames` is the
// target duration, mapped to a constant scroll speed by the distance (see SnapPanSpeed); a budget of
// 0 means fastest. Detaches the camera from whatever it is tracking, then scrolls toward the target,
// chasing it across culling/respawn. Does nothing if the target isn't present in the current map.
void PanCameraToLocalId(u8 localId, u8 panFrames)
{
    s16 targetX, targetY;

    // Reject a target the current map doesn't have (neither spawned nor a template), matching
    // SetCameraTrackedLocalId.
    if (localId != TRACK_LOCAL_ID_PLAYER
        && ResolveTrackedObjectEvent(localId) >= OBJECT_EVENTS_COUNT
        && !GetObjectEventTemplateCoordsByLocalId(localId, &targetX, &targetY))
        return;

    DetachCameraForPan();

    // Track the new id for the whole pan, so the overworld keeps the player parked while it travels.
    sCameraTrackedLocalId = localId;
    sCameraPanLocalId = localId;
    sCameraPanToTile = FALSE;
    sCameraPanArrived = FALSE;

    GetPanTargetTile(localId, &targetX, &targetY);
    SetPanSpeedForReach(targetX, targetY, panFrames);

    sCameraPanMoveX = 0;
    sCameraPanMoveY = 0;
    sCameraPanActive = TRUE;
}

// Smoothly pans the camera to a fixed tile (map coordinates, without MAP_OFFSET) in the current map,
// using the same constant-speed glide as PanCameraToLocalId. The target is clamped to the map
// bounds. Unlike the object pan there is nothing to follow on arrival, so the camera rests on the
// tile, detached, until another mode takes over; HasCameraPanArrived reports when it has settled.
void PanCameraToTile(s16 x, s16 y, u8 panFrames)
{
    ClampTileToMap(&x, &y);
    DetachCameraForPan();

    // A tile pan has no object anchor; keep the tracked id as the player, while sCameraPanActive is
    // what keeps the camera detached and parked on the tile meanwhile.
    sCameraTrackedLocalId = TRACK_LOCAL_ID_PLAYER;
    sCameraPanToTile = TRUE;
    sCameraPanArrived = FALSE;
    sCameraPanTargetX = x;
    sCameraPanTargetY = y;
    SetPanSpeedForReach(x, y, panFrames);

    sCameraPanMoveX = 0;
    sCameraPanMoveY = 0;
    sCameraPanActive = TRUE;
}

// Whether an auto-pan (PanCameraToLocalId / PanCameraToTile) is currently running or resting.
bool8 IsCameraPanActive(void)
{
    return sCameraPanActive;
}

// Whether a tile pan has reached its destination and is resting there. Stays FALSE for object pans
// (which lock onto their target and end). Used by the debug "Pan to Tile" test to start freecam once
// the camera has settled.
bool8 HasCameraPanArrived(void)
{
    return sCameraPanArrived;
}

void ResetCameraUpdateInfo(void)
{
    gFieldCamera.movementSpeedX = 0;
    gFieldCamera.movementSpeedY = 0;
    gFieldCamera.x = 0;
    gFieldCamera.y = 0;
    gFieldCamera.spriteId = 0;
    gFieldCamera.callback = NULL;
}

u32 InitCameraUpdateCallback(u8 trackedSpriteId)
{
    if (gFieldCamera.spriteId != 0)
        DestroySprite(&gSprites[gFieldCamera.spriteId]);
    gFieldCamera.spriteId = AddCameraObject(trackedSpriteId);
    gFieldCamera.callback = CameraUpdateCallback;
    return 0;
}

// Tracks the camera's elevation level from its focus tile, mirroring gPlayerElevation.
void UpdateCameraElevation(void)
{
    gCameraElevation = MapGridGetElevationAt(gCameraPos.x + MAP_OFFSET, gCameraPos.y + MAP_OFFSET);
}

// Tracks the biome of the camera's focus tile, mirroring gPlayerBiome.
void UpdateCameraBiome(void)
{
    gCameraBiome = MapGridGetMetatileBiomeAt(gCameraPos.x + MAP_OFFSET, gCameraPos.y + MAP_OFFSET);
}

void CameraUpdate(void)
{
    int deltaX;
    int deltaY;
    int curMovementOffsetY;
    int curMovementOffsetX;
    int movementSpeedX;
    int movementSpeedY;

    if (gFieldCamera.callback != NULL)
        gFieldCamera.callback(&gFieldCamera);
    movementSpeedX = gFieldCamera.movementSpeedX;
    movementSpeedY = gFieldCamera.movementSpeedY;
    deltaX = 0;
    deltaY = 0;
    curMovementOffsetX = gFieldCamera.x;
    curMovementOffsetY = gFieldCamera.y;


    if (curMovementOffsetX == 0 && movementSpeedX != 0)
    {
        if (movementSpeedX > 0)
            deltaX = 1;
        else
            deltaX = -1;
    }
    if (curMovementOffsetY == 0 && movementSpeedY != 0)
    {
        if (movementSpeedY > 0)
            deltaY = 1;
        else
            deltaY = -1;
    }
    if (curMovementOffsetX != 0 && curMovementOffsetX == -movementSpeedX)
    {
        if (movementSpeedX > 0)
            deltaX = 1;
        else
            deltaX = -1;
    }
    if (curMovementOffsetY != 0 && curMovementOffsetY == -movementSpeedY)
    {
        // A Y-axis mid-step reversal must step the camera on the Y axis (upstream wrote deltaX,
        // redrawing the wrong slice). The player never reverses mid-step, but a tracked free-roaming
        // NPC turning around between tiles does, which caused the brief tile flicker while tracking.
        if (movementSpeedY > 0)
            deltaY = 1;
        else
            deltaY = -1;
    }

    gFieldCamera.x += movementSpeedX;
    gFieldCamera.x %= 16;
    gFieldCamera.y += movementSpeedY;
    gFieldCamera.y %= 16;

    // Advance the sprite pixel offset here, in lockstep with gFieldCamera.x above, rather than at the
    // end of the frame. The spawn inside UpdateObjectEventsForCameraUpdate positions via
    // SetSpritePosToMapCoords, which assumes gFieldCamera.x == -gTotalCameraPixelOffset (mod 16). If the
    // decrement lags until end-of-frame the two are movementSpeed px out of phase during the spawn, so a
    // freshly spawned object bakes a sub-tile offset (up to Mach Bike speed) that persists until respawn.
    gTotalCameraPixelOffsetX -= movementSpeedX;
    gTotalCameraPixelOffsetY -= movementSpeedY;

    if (deltaX != 0 || deltaY != 0)
    {
        CameraMove(deltaX, deltaY);
        UpdateObjectEventsForCameraUpdate(deltaX, deltaY);
        // Rebuild the in-view set/anchor (above) then re-center the tile buffer on the camera BEFORE
        // the focus tile is read below, so location/elevation/biome and the slice redraw all see the
        // stitched foreign-map data as the camera crosses a connection seam.
        StitchCameraView();
        // Location is driven by the camera, not the player: re-evaluate the active map location from
        // the camera's new focus tile, so transitions stay correct even while detached.
        TryUpdateMapLocation(gCameraPos.x + MAP_OFFSET, gCameraPos.y + MAP_OFFSET);
        UpdateCameraElevation();
        UpdateCameraBiome();
        RotatingGatePuzzleCameraUpdate(deltaX, deltaY);
        SetBerryTreesSeen();
        AddCameraTileOffset(&sFieldCameraOffset, deltaX * 2, deltaY * 2);
        RedrawMapSlicesForCameraUpdate(&sFieldCameraOffset, deltaX * 2, deltaY * 2);
    }

    AddCameraPixelOffset(&sFieldCameraOffset, movementSpeedX, movementSpeedY);
}

void SetCameraPanningCallback(void (*callback)(void))
{
    sFieldCameraPanningCallback = callback;
}

void SetCameraPanning(s16 horizontal, s16 vertical)
{
    sHorizontalCameraPan = horizontal;
    sVerticalCameraPan = vertical + 32;
}

void InstallCameraPanAheadCallback(void)
{
    sFieldCameraPanningCallback = CameraPanningCB_PanAhead;
    sBikeCameraPanFlag = FALSE;
    sHorizontalCameraPan = 0;
    sVerticalCameraPan = 32;
}

void UpdateCameraPanning(void)
{
    if (sFieldCameraPanningCallback != NULL)
        sFieldCameraPanningCallback();
    //Update sprite offset of overworld objects
    gSpriteCoordOffsetX = gTotalCameraPixelOffsetX - sHorizontalCameraPan;
    gSpriteCoordOffsetY = gTotalCameraPixelOffsetY - sVerticalCameraPan - 8;
}

static void CameraPanningCB_PanAhead(void)
{
    u8 var;

    if (gUnusedBikeCameraAheadPanback == FALSE)
    {
        InstallCameraPanAheadCallback();
    }
    else
    {
        // this code is never reached
        if (gPlayerAvatar.tileTransitionState == T_TILE_TRANSITION)
        {
            sBikeCameraPanFlag ^= 1;
            if (sBikeCameraPanFlag == FALSE)
                return;
        }
        else
        {
            sBikeCameraPanFlag = FALSE;
        }

        var = GetPlayerMovementDirection();
        if (var == 2)
        {
            if (sVerticalCameraPan > -8)
                sVerticalCameraPan -= 2;
        }
        else if (var == 1)
        {
            if (sVerticalCameraPan < 72)
                sVerticalCameraPan += 2;
        }
        else if (sVerticalCameraPan < 32)
        {
            sVerticalCameraPan += 2;
        }
        else if (sVerticalCameraPan > 32)
        {
            sVerticalCameraPan -= 2;
        }
    }
}
