#include "global.h"
#include "berry.h"
#include "bike.h"
#include "field_camera.h"
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
    bool8 copyBGToVRAM;
};

static void RedrawMapSliceNorth(struct FieldCameraOffset *, const struct MapLayout *);
static void RedrawMapSliceSouth(struct FieldCameraOffset *, const struct MapLayout *);
static void RedrawMapSliceEast(struct FieldCameraOffset *, const struct MapLayout *);
static void RedrawMapSliceWest(struct FieldCameraOffset *, const struct MapLayout *);
static s32 MapPosToBgTilemapOffset(struct FieldCameraOffset *, s32, s32);
static void DrawWholeMapViewInternal(int, int, const struct MapLayout *);
static void DrawMetatileAt(const struct MapLayout *, u16, int, int);
static void DrawMetatile(s32, const u16 *, u16);
static void CameraPanningCB_PanAhead(void);
static void UpdateFreecamMovement(struct CameraObject *);
static void UpdateCameraPanMovement(struct CameraObject *);
void MoveCameraAndRedrawMap(int deltaX, int deltaY);

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

// Debug "pan to NPC" state: when active the camera detaches from the player and auto-scrolls
// toward the object event with local id sCameraPanLocalId (re-resolved each tile so a moving,
// or culled-then-spawned, target is chased), then locks onto it like SetCameraTrackedLocalId.
// Mutually exclusive with freecam. See PanCameraToLocalId / UpdateCameraPanMovement.
static bool8 sCameraPanActive;
static u8 sCameraPanLocalId;
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
// The camera's actual elevation level, tracked exactly like gPlayerElevation but from the
// camera's focus tile (gCameraPos). Since the camera follows the player most of the
// time, this usually equals gPlayerElevation; it diverges only while the camera is detached.
EWRAM_DATA u8 gCameraElevation = 0;
// The biome of the camera's focus tile, tracked like gPlayerBiome but from the camera. Usually
// equals gPlayerBiome; diverges only while the camera is detached.
EWRAM_DATA u8 gCameraBiome = 0;

static void ResetCameraOffset(struct FieldCameraOffset *cameraOffset)
{
    cameraOffset->xTileOffset = 0;
    cameraOffset->yTileOffset = 0;
    cameraOffset->xPixelOffset = 0;
    cameraOffset->yPixelOffset = 0;
    cameraOffset->copyBGToVRAM = TRUE;
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

void FieldUpdateBgTilemapScroll(void)
{
    u32 r4, r5;
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
    sFieldCameraOffset.copyBGToVRAM = TRUE;
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
    cameraOffset->copyBGToVRAM = TRUE;
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
    {
        DrawMetatileAt(gMapHeader.mapLayout, offset, x, y);
        sFieldCameraOffset.copyBGToVRAM = TRUE;
    }
}

void DrawDoorMetatileAt(int x, int y, u16 *tiles)
{
    int offset = MapPosToBgTilemapOffset(&sFieldCameraOffset, x, y);

    if (offset >= 0)
    {
        DrawMetatile(0xFF, tiles, offset);
        sFieldCameraOffset.copyBGToVRAM = TRUE;
    }
}

static void DrawMetatileAt(const struct MapLayout *mapLayout, u16 offset, int x, int y)
{
    u16 metatileId = MapGridGetMetatileIdAt(x, y);
    const u16 *metatiles;

    if (metatileId > NUM_METATILES_TOTAL)
        metatileId = 0;
    if (metatileId < NUM_METATILES_IN_PRIMARY)
    {
        metatiles = mapLayout->primaryTileset->metatiles;
    }
    else
    {
        metatiles = GetActiveLocationData()->secondaryTileset->metatiles;
        metatileId -= NUM_METATILES_IN_PRIMARY;
    }
    DrawMetatile(MapGridGetMetatileLayerTypeAt(x, y), metatiles + metatileId * NUM_TILES_PER_METATILE, offset);
}

static void DrawMetatile(s32 metatileLayerType, const u16 *tiles, u16 offset)
{
    if (metatileLayerType == 0xFF)
    {
        // A door metatile shall be drawn, we use covered behavior
        // Draw metatile's bottom layer to the bottom background layer.
        gOverworldTilemapBuffer_Bg3[offset] = tiles[0];
        gOverworldTilemapBuffer_Bg3[offset + 1] = tiles[1];
        gOverworldTilemapBuffer_Bg3[offset + 0x20] = tiles[2];
        gOverworldTilemapBuffer_Bg3[offset + 0x21] = tiles[3];

        // Draw transparent tiles to the top background layer.
        gOverworldTilemapBuffer_Bg2[offset] = 0;
        gOverworldTilemapBuffer_Bg2[offset + 1] = 0;
        gOverworldTilemapBuffer_Bg2[offset + 0x20] = 0;
        gOverworldTilemapBuffer_Bg2[offset + 0x21] = 0;

        // Draw metatile's top layer to the middle background layer.
        gOverworldTilemapBuffer_Bg1[offset] = tiles[4];
        gOverworldTilemapBuffer_Bg1[offset + 1] = tiles[5];
        gOverworldTilemapBuffer_Bg1[offset + 0x20] = tiles[6];
        gOverworldTilemapBuffer_Bg1[offset + 0x21] = tiles[7];

    }
    else
    {
        // Draw metatile's bottom layer to the bottom background layer.
        gOverworldTilemapBuffer_Bg3[offset] = tiles[0];
        gOverworldTilemapBuffer_Bg3[offset + 1] = tiles[1];
        gOverworldTilemapBuffer_Bg3[offset + 0x20] = tiles[2];
        gOverworldTilemapBuffer_Bg3[offset + 0x21] = tiles[3];

        // Draw metatile's middle layer to the middle background layer.
        gOverworldTilemapBuffer_Bg2[offset] = tiles[4];
        gOverworldTilemapBuffer_Bg2[offset + 1] = tiles[5];
        gOverworldTilemapBuffer_Bg2[offset + 0x20] = tiles[6];
        gOverworldTilemapBuffer_Bg2[offset + 0x21] = tiles[7];

        // Draw metatile's top layer to the top background layer, which covers object event sprites.
        gOverworldTilemapBuffer_Bg1[offset] = tiles[8];
        gOverworldTilemapBuffer_Bg1[offset + 1] = tiles[9];
        gOverworldTilemapBuffer_Bg1[offset + 0x20] = tiles[10];
        gOverworldTilemapBuffer_Bg1[offset + 0x21] = tiles[11];


    }

    ScheduleBgCopyTilemapToVram(1);
    ScheduleBgCopyTilemapToVram(2);
    ScheduleBgCopyTilemapToVram(3);
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

// Drives the camera from the D-pad while the debug freecam is active, instead of
// following the player's sprite. Movement direction is only (re)evaluated when the
// camera is sitting exactly on a tile boundary, so an in-progress step always
// finishes and the camera comes to rest grid-aligned (matching normal movement).
// Movement is clamped to the current map's bounds, keeping the camera focus within
// the same range the player can occupy so RecenterCameraOnPlayer can snap back with
// a single CameraMove (no connection-crossing edge cases). Scrolling halts whenever the
// player's field controls are locked, so an open menu (the debug menu, or the start menu
// reachable while detached) takes the D-pad without the camera drifting underneath it.
static void UpdateFreecamMovement(struct CameraObject *fieldCamera)
{
    if (fieldCamera->x == 0 && fieldCamera->y == 0)
    {
        u16 keys = ArePlayerFieldControlsLocked() ? 0 : gMain.heldKeys;
        int width = gMapHeader.mapLayout->width;
        int height = gMapHeader.mapLayout->height;

        sFreecamMoveX = 0;
        sFreecamMoveY = 0;
        if ((keys & DPAD_RIGHT) && gCameraPos.x < width - 1)
            sFreecamMoveX = FREECAM_SPEED;
        else if ((keys & DPAD_LEFT) && gCameraPos.x > 0)
            sFreecamMoveX = -FREECAM_SPEED;
        if ((keys & DPAD_DOWN) && gCameraPos.y < height - 1)
            sFreecamMoveY = FREECAM_SPEED;
        else if ((keys & DPAD_UP) && gCameraPos.y > 0)
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
    // Freecam and an auto-pan are mutually-exclusive ways to drive the camera; starting
    // freecam takes over from a pan in progress.
    if (active)
        sCameraPanActive = FALSE;
}

// The camera is "detached" from the player while freecam is running or while tracking
// an object other than the player (local id 0). The overworld uses this to keep the
// player parked (no walking, no field interactions) while still allowing menus.
bool8 IsCameraDetachedFromPlayer(void)
{
    return sFreecamActive || sCameraPanActive || sCameraTrackedLocalId != TRACK_LOCAL_ID_PLAYER;
}

// Jumps the camera focus to a tile (map coordinates without MAP_OFFSET, clamped to the
// current map) and rebuilds a pristine, tile-aligned view there: the sub-tile scroll
// offsets are zeroed, the scene is reculled/respawned, and every object sprite is snapped
// into the new frame. Resetting to a clean tile-aligned state each time is what keeps the
// camera from drifting when re-centring repeatedly on objects caught between tiles.
static void CenterCameraOnTile(int x, int y)
{
    int width = gMapHeader.mapLayout->width;
    int height = gMapHeader.mapLayout->height;
    bool8 cameraMoved;
    bool8 gfxReloaded;
    u8 i;

    if (x < 0)
        x = 0;
    else if (x > width - 1)
        x = width - 1;
    if (y < 0)
        y = 0;
    else if (y > height - 1)
        y = height - 1;

    // Only rebuild the view when the focus actually moves (or the camera is sitting mid-step). Re-
    // centring on the tile the camera already rests on — e.g. opening the Track box, which re-tracks
    // the player at its current tile — must NOT zero the field-camera offset and DrawWholeMapView:
    // that rewrites the BG buffer from whatever rolled, but correct, alignment it inherited from the
    // map-entry view (saved map view / connection stitch) to a canonical one, and the buffer rewrite
    // and scroll register settle over two frames — the one-off viewport shift seen on the first Track
    // open after warping/connecting in. With no movement the BG is already correct, so leave it be.
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

    // Respawn/cull around the new focus, then snap every live sprite into the new frame so
    // none are left at a stale sub-tile position relative to the camera. UpdateObjectEventsForCameraUpdate
    // culls before it spawns, so a far jump frees the old view's object/sprite slots before
    // trying to fill them with the new context's objects (see its definition).
    UpdateObjectEventsForCameraUpdate(0, 0);
    for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
    {
        if (gObjectEvents[i].active)
            MoveObjectEventToMapCoords(&gObjectEvents[i], gObjectEvents[i].currentCoords.x, gObjectEvents[i].currentCoords.y);
    }
    // The jump may have landed in a different location (e.g. tracking an NPC that stands in a
    // different secondary tileset to the player). Apply that location's tileset synchronously
    // BEFORE redrawing, so the whole-map redraw below uses the destination's graphics instead of
    // the previous focus's — otherwise the new surroundings flash with the old (player's) tileset
    // for the frame before the deferred swap catches up. Safe to do synchronously here because a
    // jump lands tile-aligned, with no sub-tile scroll offset to corrupt.
    gfxReloaded = UpdateMapLocationGfxImmediate(gCameraPos.x + MAP_OFFSET, gCameraPos.y + MAP_OFFSET);
    // Redraw when the focus moved, or when a tileset swap above means the resident graphics changed
    // under the current view. A pure re-centre on the resting tile needs neither.
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

// Tracks an object event by local id (0 = the player), changeable on the fly. Stable
// across culling/respawn: if the target is currently culled we move the camera onto its
// default position (its map template, or gSaveBlock1's saved tile for the player), which
// recreates the scene there and respawns it; the object event currently bearing that
// local id is then followed, so a respawn into a different gObjectEvents slot is
// transparent.
//
// When repointing to the player to leave freecam, call this while freecam is still
// flagged active so the location update swaps the tileset silently, without the
// player-facing music/popup (see TryUpdateMapLocation).
void SetCameraTrackedLocalId(u8 localId)
{
    u8 objectEventId = ResolveTrackedObjectEvent(localId);
    s16 targetX, targetY;
    u8 i;

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
    else if (!GetObjectEventTemplateCoordsByLocalId(localId, &targetX, &targetY))
    {
        return; // No such object event in this map.
    }

    // Drop the previous anchor before the rebuild, so snapping sprites into the new frame
    // doesn't resync the camera onto the object we're about to stop following.
    for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
        gObjectEvents[i].trackedByCamera = FALSE;

    // Jump to the target, rebuilding a clean tile-aligned scene around it (also respawns it
    // if it was culled).
    CenterCameraOnTile(targetX - MAP_OFFSET, targetY - MAP_OFFSET);
    sCameraTrackedLocalId = localId;

    // Follow it if it is (now) spawned. A flag-removed object never spawns, so the camera
    // simply rests on its default position with nothing to follow.
    objectEventId = ResolveTrackedObjectEvent(localId);
    if (objectEventId < OBJECT_EVENTS_COUNT)
    {
        struct ObjectEvent *tracked = &gObjectEvents[objectEventId];

        // Make the tracked object the camera's anchor, exactly as the engine does for the
        // player (see SetCameraToTrackPlayer): the trackedByCamera object is the one the
        // culling/redraw scrolls around.
        tracked->trackedByCamera = TRUE;

        // The camera follows the object's SPRITE, so the object has to be live for the
        // camera to track its movement. An off-screen target respawns unfrozen (which is
        // why those already worked); an on-screen one was frozen when the menu opened, so
        // its sprite never moved and the camera looked stuck on its start position. Clear
        // any half-finished step so it restarts cleanly from the tile it was snapped to,
        // then unfreeze it.
        tracked->singleMovementActive = FALSE;
        ObjectEventClearHeldMovement(tracked);
        UnfreezeObjectEvent(tracked);

        CameraObjectSetFollowedSpriteId(tracked->spriteId);
        // A deliberate repoint, not the player walking: swap the location graphics without the
        // music/popup (the camera reads as "attached" to the new target this same frame, so the
        // popup gate wouldn't catch it, and a popup over the open debug box corrupts it).
        TryUpdateMapLocationSilent(gCameraPos.x + MAP_OFFSET, gCameraPos.y + MAP_OFFSET);
    }
}

u8 GetCameraTrackedLocalId(void)
{
    return sCameraTrackedLocalId;
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
    u8 i;

    for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
        gObjectEvents[i].trackedByCamera = FALSE;
    sCameraTrackedLocalId = TRACK_LOCAL_ID_PLAYER;
    sCameraPanActive = FALSE;
}

// Re-establishes the active debug camera mode after a return to the field (closing a menu or
// finishing a battle routes through SetCameraToTrackPlayer, which would otherwise snap the camera
// back to the player). The tracking statics and gCameraPos persist across the trip and the tracked
// object's sprite has just been recreated, so tracking simply resumes. Reusing SetCameraTrackedLocalId
// keeps it robust: it re-anchors a spawned target, or jumps to and respawns a culled one, and ends any
// pan that a menu interrupted by locking onto its destination.
void RestoreCameraTracking(void)
{
    u8 i;

    // A field move is about to play (see RequestCameraResetToPlayerOnFieldReturn): its animation is
    // player-centred, so cancel any tracking/freecam and snap the camera onto the player instead of
    // restoring tracking. RecenterCameraOnPlayer moves gCameraPos onto the player (SetCameraToTrackPlayer
    // only re-anchors, it doesn't move the focus, which was sitting on the tracked object).
    if (sResetCameraToPlayerOnFieldReturn)
    {
        ResetCameraTracking();
        RecenterCameraOnPlayer();
        return;
    }

    if (sFreecamActive)
    {
        // Freecam is D-pad driven from gCameraPos and ignores the followed sprite, so it only needs
        // no object pulling the camera; drop the player anchor SetCameraToTrackPlayer just added.
        for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
            gObjectEvents[i].trackedByCamera = FALSE;
        return;
    }
    if (sCameraTrackedLocalId != TRACK_LOCAL_ID_PLAYER)
        SetCameraTrackedLocalId(sCameraTrackedLocalId);
}

// Clears all debug camera state back to following the player. Called on a full map load (warp),
// where any tracked object belonged to the old map.
void ResetCameraTracking(void)
{
    sFreecamActive = FALSE;
    sFreecamMoveX = 0;
    sFreecamMoveY = 0;
    sCameraPanActive = FALSE;
    sCameraPanMoveX = 0;
    sCameraPanMoveY = 0;
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

// Resolves the tile the pan is heading for, in gCameraPos space (no MAP_OFFSET). A spawned
// target gives its live tile, so a moving NPC is chased; a culled one gives its default
// position (its map template, or gSaveBlock1's saved tile for the player) so the camera heads
// there until the scroll brings it into view and spawns it. Returns the spawned object event
// id, or OBJECT_EVENTS_COUNT if the target isn't spawned right now.
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

// Hands the camera off from the pan to ordinary sprite-following of the object it has arrived
// at, mirroring the tail of SetCameraTrackedLocalId: the object becomes the camera anchor and
// is unfrozen with any half-finished step cleared, so the camera follows its sprite from here
// (tracking its continually-updating position). Ends the pan.
static void LockCameraOntoPanTarget(u8 objectEventId)
{
    struct ObjectEvent *tracked = &gObjectEvents[objectEventId];

    tracked->trackedByCamera = TRUE;
    tracked->singleMovementActive = FALSE;
    ObjectEventClearHeldMovement(tracked);
    UnfreezeObjectEvent(tracked);
    CameraObjectSetFollowedSpriteId(tracked->spriteId);

    sCameraPanActive = FALSE;
    sCameraPanMoveX = 0;
    sCameraPanMoveY = 0;
    // Silent: the camera locks on "attached" to the target this frame, so the popup gate
    // wouldn't suppress it, and a popup would be wrong for an automatic lock-on anyway.
    TryUpdateMapLocationSilent(gCameraPos.x + MAP_OFFSET, gCameraPos.y + MAP_OFFSET);
}

// Drives the camera toward the pan target while "Pan to NPC" is active, reusing the freecam
// scroll path: movement is only (re)evaluated on a tile boundary so each step finishes
// grid-aligned. The target tile is re-resolved every boundary, so a moving NPC is chased and a
// culled target (headed for its default tile) is picked up the moment the scroll brings it
// into view and spawns it. On arrival the camera locks onto the (now spawned) object exactly
// like SetCameraTrackedLocalId and the pan ends; if the target stays culled at its default tile
// (e.g. a flag-removed object that never spawns) the pan stays armed and the camera simply
// rests there.
static void UpdateCameraPanMovement(struct CameraObject *fieldCamera)
{
    if (fieldCamera->x == 0 && fieldCamera->y == 0)
    {
        s16 targetX, targetY;
        u8 objectEventId = GetPanTargetTile(sCameraPanLocalId, &targetX, &targetY);

        sCameraPanMoveX = 0;
        sCameraPanMoveY = 0;
        if (gCameraPos.x < targetX)
            sCameraPanMoveX = FREECAM_SPEED;
        else if (gCameraPos.x > targetX)
            sCameraPanMoveX = -FREECAM_SPEED;
        if (gCameraPos.y < targetY)
            sCameraPanMoveY = FREECAM_SPEED;
        else if (gCameraPos.y > targetY)
            sCameraPanMoveY = -FREECAM_SPEED;

        // Focus tile now matches the target tile.
        if (sCameraPanMoveX == 0 && sCameraPanMoveY == 0)
        {
            // A culled target may not be spawned yet. With the camera now at rest no further
            // CameraMove runs the spawn pass, so force one here at the current focus (exactly as
            // CenterCameraOnTile does on a jump). Without this the pan deadlocks on an empty tile
            // and never locks on. The forced pass spawns the target if its tile is in view.
            if (objectEventId >= OBJECT_EVENTS_COUNT)
            {
                UpdateObjectEventsForCameraUpdate(0, 0);
                objectEventId = ResolveTrackedObjectEvent(sCameraPanLocalId);
            }
            if (objectEventId < OBJECT_EVENTS_COUNT)
                LockCameraOntoPanTarget(objectEventId);
        }
    }
    fieldCamera->movementSpeedX = sCameraPanMoveX;
    fieldCamera->movementSpeedY = sCameraPanMoveY;
}

// Smoothly pans the camera to an object event (0 = the player), as an alternative to
// SetCameraTrackedLocalId's instant jump. Detaches the camera from whatever it is currently
// tracking (dropping the old anchor and cancelling any freecam/pan already running), then
// scrolls tile-by-tile toward the target until it arrives, at which point it locks on exactly
// like SetCameraTrackedLocalId. Stable across culling: while the target is culled the camera
// heads for its default position; the scroll spawns it as it enters view, and a spawned
// target's continually-updating position is chased so a moving NPC is followed in. Does
// nothing if the target isn't present in the current map.
void PanCameraToLocalId(u8 localId)
{
    s16 targetX, targetY;
    u8 i;

    // Reject a target the current map doesn't have (neither spawned nor a template), matching
    // SetCameraTrackedLocalId.
    if (localId != TRACK_LOCAL_ID_PLAYER
        && ResolveTrackedObjectEvent(localId) >= OBJECT_EVENTS_COUNT
        && !GetObjectEventTemplateCoordsByLocalId(localId, &targetX, &targetY))
        return;

    // Take over from freecam or a track/pan already running: clear the freecam flag and drop
    // every camera anchor so the upcoming scroll isn't resynced onto the old target.
    sFreecamActive = FALSE;
    sFreecamMoveX = 0;
    sFreecamMoveY = 0;
    for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
        gObjectEvents[i].trackedByCamera = FALSE;

    // Start the pan from a clean, tile-aligned state. The camera is very often mid-step when the
    // pan is triggered: while the pan box is open it is still following the previously-tracked
    // object, and an NPC's sprite (unlike the player's) isn't input-gated to tile boundaries, so
    // it is usually caught between tiles. UpdateCameraPanMovement only (re)evaluates direction on
    // a boundary, so a held sub-tile offset would never return to 0 once the pan speed is zeroed,
    // and the camera would stall after barely moving. Re-centring on the current focus tile zeroes
    // the offset (as a track-jump does) so the first boundary eval runs and the pan proceeds.
    CenterCameraOnTile(gCameraPos.x, gCameraPos.y);

    // Mark the camera as tracking the new id for the whole pan, so the overworld keeps the
    // player parked while it travels (and once it arrives, exactly as if Track NPC was used).
    sCameraTrackedLocalId = localId;
    sCameraPanLocalId = localId;
    sCameraPanMoveX = 0;
    sCameraPanMoveY = 0;
    sCameraPanActive = TRUE;
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

// Tracks the camera's actual elevation level from its current focus tile, mirroring how
// gPlayerElevation is tracked for the player. Only ordinary elevation levels
// (ELEVATION_FIRST_LEVEL and up) update it; the stored value is the level (tile value minus
// ELEVATION_FIRST_LEVEL, i.e. 0-123). Special tiles (transition, collision, water,
// multi-level) leave the last tracked level untouched.
void UpdateCameraElevation(void)
{
    u8 elevation = MapGridGetElevationAt(gCameraPos.x + MAP_OFFSET, gCameraPos.y + MAP_OFFSET);
    if (elevation >= ELEVATION_FIRST_LEVEL)
        gCameraElevation = elevation - ELEVATION_FIRST_LEVEL;
}

// Tracks the biome of the camera's focus tile, mirroring gPlayerBiome. No special cases:
// the camera's biome is just whatever the focus tile's biome value is.
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
        // Y-axis mid-step reversal must move the camera (and redraw the slice) on the Y axis.
        // Upstream wrote deltaX here; the player never hits this path (its camera is input-gated
        // to tile boundaries and so never reverses mid-step), but a tracked free-roaming NPC that
        // turns around between tiles did, redrawing the wrong (E/W) slice and leaving the newly
        // exposed N/S edge stale for a frame -> the brief tile flicker while tracking NPCs.
        if (movementSpeedY > 0)
            deltaY = 1;
        else
            deltaY = -1;
    }

    gFieldCamera.x += movementSpeedX;
    gFieldCamera.x %= 16;
    gFieldCamera.y += movementSpeedY;
    gFieldCamera.y %= 16;

    if (deltaX != 0 || deltaY != 0)
    {
        CameraMove(deltaX, deltaY);
        // Location is driven by the camera, not the player: whenever the camera's focus
        // advances to a new tile, re-evaluate the active map location from that tile. This
        // keeps location transitions correct even when the camera is detached from the
        // player (see SpawnCameraObject). CameraMove has just updated gCameraPos
        // to the camera's new focus tile.
        TryUpdateMapLocation(gCameraPos.x + MAP_OFFSET, gCameraPos.y + MAP_OFFSET);
        UpdateCameraElevation();
        UpdateCameraBiome();
        UpdateObjectEventsForCameraUpdate(deltaX, deltaY);
        RotatingGatePuzzleCameraUpdate(deltaX, deltaY);
        SetBerryTreesSeen();
        AddCameraTileOffset(&sFieldCameraOffset, deltaX * 2, deltaY * 2);
        RedrawMapSlicesForCameraUpdate(&sFieldCameraOffset, deltaX * 2, deltaY * 2);
    }

    AddCameraPixelOffset(&sFieldCameraOffset, movementSpeedX, movementSpeedY);
    gTotalCameraPixelOffsetX -= movementSpeedX;
    gTotalCameraPixelOffsetY -= movementSpeedY;
}

void MoveCameraAndRedrawMap(int deltaX, int deltaY) //unused
{
    CameraMove(deltaX, deltaY);
    UpdateObjectEventsForCameraUpdate(deltaX, deltaY);
    DrawWholeMapView();
    gTotalCameraPixelOffsetX -= deltaX * 16;
    gTotalCameraPixelOffsetY -= deltaY * 16;
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
