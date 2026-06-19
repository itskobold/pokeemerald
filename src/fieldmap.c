#include "global.h"
#include "battle_pyramid.h"
#include "bg.h"
#include "event_object_movement.h"
#include "field_camera.h"
#include "field_weather.h"
#include "fieldmap.h"
#include "fldeff.h"
#include "fldeff_misc.h"
#include "frontier_util.h"
#include "menu.h"
#include "mirage_tower.h"
#include "overworld.h"
#include "palette.h"
#include "pokenav.h"
#include "script.h"
#include "secret_base.h"
#include "trainer_hill.h"
#include "tv.h"
#include "constants/rgb.h"
#include "constants/metatile_behaviors.h"
#include "constants/biome.h"

struct ConnectionFlags
{
    u8 south:1;
    u8 north:1;
    u8 west:1;
    u8 east:1;
    u8 southwest:1;
    u8 southeast:1;
    u8 northwest:1;
    u8 northeast:1;
};

EWRAM_DATA static u16 ALIGNED(4) sBackupMapData[MAX_MAP_DATA_SIZE] = {0};
EWRAM_DATA static u8 ALIGNED(4) sBackupMapAttrData[MAX_MAP_DATA_SIZE] = {0};
EWRAM_DATA struct MapHeader gMapHeader = {0};
// Index (0..MAX_MAP_LOCATIONS-1) of the location property set currently active for
// gMapHeader, selected by the location attribute of the tile the camera is focused on.
EWRAM_DATA static u8 sActiveMapLocation = 0;
EWRAM_DATA struct Camera gCamera = {0};
EWRAM_DATA static struct ConnectionFlags sMapConnectionFlags = {0};
EWRAM_DATA static u32 UNUSED sFiller = 0; // without this, the next file won't align properly

COMMON_DATA struct BackupMapLayout gBackupMapLayout = {0};

static const struct ConnectionFlags sDummyConnectionFlags = {0};

static void InitMapLayoutData(struct MapHeader *mapHeader);
static void InitBackupMapLayoutData(const u16 *map, const u8 *attributes, u16 width, u16 height);
static void FillSouthConnection(struct MapHeader const *mapHeader, struct MapHeader const *connectedMapHeader, s32 offset);
static void FillNorthConnection(struct MapHeader const *mapHeader, struct MapHeader const *connectedMapHeader, s32 offset);
static void FillWestConnection(struct MapHeader const *mapHeader, struct MapHeader const *connectedMapHeader, s32 offset);
static void FillEastConnection(struct MapHeader const *mapHeader, struct MapHeader const *connectedMapHeader, s32 offset);
static void FillSouthWestConnection(struct MapHeader const *mapHeader, struct MapHeader const *connectedMapHeader, s32 offset);
static void FillSouthEastConnection(struct MapHeader const *mapHeader, struct MapHeader const *connectedMapHeader, s32 offset);
static void FillNorthWestConnection(struct MapHeader const *mapHeader, struct MapHeader const *connectedMapHeader, s32 offset);
static void FillNorthEastConnection(struct MapHeader const *mapHeader, struct MapHeader const *connectedMapHeader, s32 offset);
static void InitBackupMapLayoutConnections(struct MapHeader *mapHeader);
static void LoadSavedMapView(void);
static bool8 SkipCopyingMetatileFromSavedMap(u16 *mapBlock, u16 mapWidth, u8 yMode);
static const struct MapConnection *GetIncomingConnection(u8 direction, int x, int y);
static bool8 IsPosInIncomingConnectingMap(u8 direction, int x, int y, const struct MapConnection *connection);
static bool8 IsCoordInIncomingConnectingMap(int coord, int srcMax, int destMax, int offset);

// Border (void) tiles use the focus map's border, not always home's, so the empty space past a
// roamed connection's edge shows that map's border — and stays consistent with its secondary tileset,
// which ActiveLocationHeader() also drives.
#define GetBorderBlockAt(x, y) ({                                                                  \
    u16 block;                                                                                     \
    int i;                                                                                         \
    const u16 *border = ActiveLocationHeader()->mapLayout->border; /* Unused, they read it again below */ \
                                                                                                   \
    i = (x + 1) & 1;                                                                               \
    i += ((y + 1) & 1) * 2;                                                                        \
                                                                                                   \
    block = ActiveLocationHeader()->mapLayout->border[i];                                          \
})

// The backup buffer slides with the camera's anchor map (the map currently under the camera). A
// tile's buffer cell is its passed coordinate (home-frame + MAP_OFFSET, the convention every caller
// already uses) minus the anchor's home-frame offset. Both shifts are 0 while the camera is on the
// home map, so normal play indexes the buffer exactly as before. See StitchCameraView.
static s16 sCameraViewShiftX;
static s16 sCameraViewShiftY;

#define MapGridBufX(x) ((x) - sCameraViewShiftX)
#define MapGridBufY(y) ((y) - sCameraViewShiftY)

#define AreCoordsWithinMapGridBounds(x, y) (MapGridBufX(x) >= 0 && MapGridBufX(x) < gBackupMapLayout.width && MapGridBufY(y) >= 0 && MapGridBufY(y) < gBackupMapLayout.height)

#define GetMapGridBlockAt(x, y) (AreCoordsWithinMapGridBounds(x, y) ? gBackupMapLayout.map[MapGridBufX(x) + gBackupMapLayout.width * MapGridBufY(y)] : GetBorderBlockAt(x, y))

// Out-of-bounds (border) tiles have no attributes; treat them as impassable.
#define GetMapGridAttrAt(x, y) (AreCoordsWithinMapGridBounds(x, y) ? gBackupMapLayout.attributes[MapGridBufX(x) + gBackupMapLayout.width * MapGridBufY(y)] : MAPATTR_COLLISION)

// Whether the buffer currently holds a roamed (non-home) anchor, plus the view it was last stitched
// from, so a re-stitch only runs when the anchor or in-view set actually changes and the return to
// the home map restores the canonical buffer exactly once.
static bool8 sCameraViewRoaming;
static struct ViewMap sLastStitchAnchor;
static struct ViewMap sLastStitchMaps[MAX_MAPS_IN_VIEW];
static u8 sLastStitchCount;
// The map whose weather is currently applied, so weather transitions once each time the camera roams
// onto a different map rather than every frame.
static u8 sCameraWeatherMapGroup;
static u8 sCameraWeatherMapNum;

const struct MapHeader *const GetMapHeaderFromConnection(const struct MapConnection *connection)
{
    return Overworld_GetMapHeaderByGroupAndId(connection->mapGroup, connection->mapNum);
}

u8 GetActiveMapLocation(void)
{
    return sActiveMapLocation;
}

// Returns the current map's position within the world map grid (each coordinate 0-31).
void GetMapGridXY(u8 *outX, u8 *outY)
{
    *outX = gMapHeader.mapGridX;
    *outY = gMapHeader.mapGridY;
}

// The map whose locations[] array the active location index refers to. While the camera roams onto a
// connected map (which never becomes gMapHeader), the focus tile's location index belongs to that
// map, so its properties — secondary tileset above all — must resolve against its header, not home's.
// sActiveLocationHeader NULL means the home map (&gMapHeader); the group/num identify it for the
// camera location-update's change detection.
static const struct MapHeader *sActiveLocationHeader;
static u8 sActiveLocationMapGroup;
static u8 sActiveLocationMapNum;

static const struct MapHeader *ActiveLocationHeader(void)
{
    return sActiveLocationHeader != NULL ? sActiveLocationHeader : &gMapHeader;
}

// Returns the location property set the rest of the game should read for the
// current map. Falls back to slot 0 if the requested slot is undefined (NULL).
const struct MapHeaderLocationData *GetActiveLocationData(void)
{
    const struct MapHeader *header = ActiveLocationHeader();
    const struct MapHeaderLocationData *data = header->locations[sActiveMapLocation];
    if (data == NULL)
        data = header->locations[0];
    return data;
}

void SetActiveMapLocation(u8 location)
{
    // Index-only setter for home-map contexts (map load, scripts): the active location belongs to home.
    sActiveLocationHeader = &gMapHeader;
    sActiveLocationMapGroup = gSaveBlock1Ptr->location.mapGroup;
    sActiveLocationMapNum = gSaveBlock1Ptr->location.mapNum;
    if (location >= MAX_MAP_LOCATIONS || gMapHeader.locations[location] == NULL)
        location = 0;
    sActiveMapLocation = location;
}

// As SetActiveMapLocation, but binds the index to a specific (connected) map — used by the camera's
// location update so that map's location indices resolve against it.
void SetActiveMapLocationForMap(u8 mapGroup, u8 mapNum, u8 location)
{
    const struct MapHeader *header = Overworld_GetMapHeaderByGroupAndId(mapGroup, mapNum);

    if (header == NULL)
        header = &gMapHeader;
    if (location >= MAX_MAP_LOCATIONS || header->locations[location] == NULL)
        location = 0;
    sActiveLocationHeader = header;
    sActiveLocationMapGroup = mapGroup;
    sActiveLocationMapNum = mapNum;
    sActiveMapLocation = location;
}

// The (group, num) of the map the active location index belongs to (home, or a roamed connection).
void GetActiveLocationMap(u8 *mapGroup, u8 *mapNum)
{
    *mapGroup = sActiveLocationMapGroup;
    *mapNum = sActiveLocationMapNum;
}

// The map header the camera's focus tile belongs to: the anchor map while roaming, else the home map.
const struct MapHeader *GetCameraFocusMapHeader(void)
{
    struct ViewMap anchor;
    const struct MapHeader *header;

    GetCameraViewAnchor(&anchor);
    if (anchor.mapGroup == gSaveBlock1Ptr->location.mapGroup && anchor.mapNum == gSaveBlock1Ptr->location.mapNum)
        return &gMapHeader;
    header = Overworld_GetMapHeaderByGroupAndId(anchor.mapGroup, anchor.mapNum);
    return header != NULL ? header : &gMapHeader;
}

// Selects the active location from the camera's focus tile, on map load, so the initial
// tileset/music/map name reflect the spawn location; thereafter TryUpdateMapLocation updates it.
void SetActiveMapLocationFromCamera(void)
{
    SetActiveMapLocation(MapGridGetMetatileLocationAt(gCameraPos.x + MAP_OFFSET,
                                                      gCameraPos.y + MAP_OFFSET));
}

void InitMap(void)
{
    InitMapLayoutData(&gMapHeader);
    SetOccupiedSecretBaseEntranceMetatiles(gMapHeader.events);
    SetActiveMapLocationFromCamera();
    UpdateCameraElevation();
    UpdateCameraBiome();
    RunOnLoadMapScript();
}

void InitMapFromSavedGame(void)
{
    // Continuing a saved game doesn't warp in, so seed the camera's focus tile from the saved
    // player position here (SetPlayerCoordsFromWarp would do this on a warp), else gCameraPos stays
    // zero and the camera loads in the map's corner.
    gCameraPos = gSaveBlock1Ptr->playerPos;
    InitMapLayoutData(&gMapHeader);
    InitSecretBaseAppearance(FALSE);
    SetOccupiedSecretBaseEntranceMetatiles(gMapHeader.events);
    SetActiveMapLocationFromCamera();
    UpdateCameraElevation();
    UpdateCameraBiome();
    LoadSavedMapView();
    RunOnLoadMapScript();
    UpdateTVScreensOnMap(gBackupMapLayout.width, gBackupMapLayout.height);
}

void InitBattlePyramidMap(bool8 setPlayerPosition)
{
    CpuFastFill16(MAPGRID_UNDEFINED, sBackupMapData, sizeof(sBackupMapData));
    CpuFastFill16((MAPATTR_UNDEFINED << 8) | MAPATTR_UNDEFINED, sBackupMapAttrData, sizeof(sBackupMapAttrData));
    gBackupMapLayout.attributes = sBackupMapAttrData;
    GenerateBattlePyramidFloorLayout(sBackupMapData, setPlayerPosition);
}

void InitTrainerHillMap(void)
{
    CpuFastFill16(MAPGRID_UNDEFINED, sBackupMapData, sizeof(sBackupMapData));
    CpuFastFill16((MAPATTR_UNDEFINED << 8) | MAPATTR_UNDEFINED, sBackupMapAttrData, sizeof(sBackupMapAttrData));
    gBackupMapLayout.attributes = sBackupMapAttrData;
    GenerateTrainerHillFloorLayout(sBackupMapData);
}

static void InitMapLayoutData(struct MapHeader *mapHeader)
{
    struct MapLayout const *mapLayout;
    int width;
    int height;
    // A fresh map load (including a player connection transition) re-bases the buffer on the new home
    // map at zero shift; clear any roaming offset left by the camera so this build is canonical.
    sCameraViewShiftX = 0;
    sCameraViewShiftY = 0;
    sCameraViewRoaming = FALSE;
    // The load path applies this map's weather; align the tracker so roaming only re-transitions on a
    // genuine map change (and the player's own transitions don't double-trigger it).
    sCameraWeatherMapGroup = gSaveBlock1Ptr->location.mapGroup;
    sCameraWeatherMapNum = gSaveBlock1Ptr->location.mapNum;
    mapLayout = mapHeader->mapLayout;
    CpuFastFill16(MAPGRID_UNDEFINED, sBackupMapData, sizeof(sBackupMapData));
    CpuFastFill16((MAPATTR_UNDEFINED << 8) | MAPATTR_UNDEFINED, sBackupMapAttrData, sizeof(sBackupMapAttrData));
    gBackupMapLayout.map = sBackupMapData;
    gBackupMapLayout.attributes = sBackupMapAttrData;
    width = mapLayout->width + MAP_OFFSET_W;
    gBackupMapLayout.width = width;
    height = mapLayout->height + MAP_OFFSET_H;
    gBackupMapLayout.height = height;
    if (width * height <= MAX_MAP_DATA_SIZE)
    {
        InitBackupMapLayoutData(mapLayout->map, mapLayout->mapAttributes, mapLayout->width, mapLayout->height);
        InitBackupMapLayoutConnections(mapHeader);
    }
}

// Copy a whole map's tiles + attributes into the backup buffer at buffer cell (destX, destY) (the
// position of the map's local (0,0)), clipped to the buffer. Generalises FillConnection: the stitcher
// lays every in-view map into the anchor-centered buffer this way, including diagonal corners.
static void BlitMapIntoBuffer(const struct MapLayout *src, int destX, int destY)
{
    int srcX = 0, srcY = 0;
    int w = src->width, h = src->height;
    int y;

    if (destX < 0) { srcX = -destX; w += destX; destX = 0; }
    if (destY < 0) { srcY = -destY; h += destY; destY = 0; }
    if (destX + w > gBackupMapLayout.width)
        w = gBackupMapLayout.width - destX;
    if (destY + h > gBackupMapLayout.height)
        h = gBackupMapLayout.height - destY;
    if (w <= 0 || h <= 0)
        return;

    for (y = 0; y < h; y++)
    {
        const u16 *srcRow = &src->map[(srcY + y) * src->width + srcX];
        const u8 *attrSrcRow = &src->mapAttributes[(srcY + y) * src->width + srcX];
        u16 *dstRow = &gBackupMapLayout.map[(destY + y) * gBackupMapLayout.width + destX];
        u8 *attrDstRow = &gBackupMapLayout.attributes[(destY + y) * gBackupMapLayout.width + destX];

        CpuCopy16(srcRow, dstRow, w * 2);
        memcpy(attrDstRow, attrSrcRow, w);
    }
}

// Rebuild the backup buffer centered on the anchor map (placed at MAP_OFFSET, like a normal map),
// filling its border ring from every in-view map at its anchor-relative offset. Sets the buffer
// shift so MapGrid lookups in the home frame land in the right cell.
static bool8 StitchAnchorBuffer(const struct ViewMap *anchor, const struct ViewMap *maps, u8 count)
{
    const struct MapHeader *anchorHeader = Overworld_GetMapHeaderByGroupAndId(anchor->mapGroup, anchor->mapNum);
    int width, height;
    u8 i;

    if (anchorHeader == NULL || anchorHeader->mapLayout == NULL)
        return FALSE;
    width = anchorHeader->mapLayout->width + MAP_OFFSET_W;
    height = anchorHeader->mapLayout->height + MAP_OFFSET_H;
    if (width * height > MAX_MAP_DATA_SIZE)
        return FALSE; // anchor map too large to stitch; leave the current buffer in place

    CpuFastFill16(MAPGRID_UNDEFINED, sBackupMapData, sizeof(sBackupMapData));
    CpuFastFill16((MAPATTR_UNDEFINED << 8) | MAPATTR_UNDEFINED, sBackupMapAttrData, sizeof(sBackupMapAttrData));
    gBackupMapLayout.map = sBackupMapData;
    gBackupMapLayout.attributes = sBackupMapAttrData;
    gBackupMapLayout.width = width;
    gBackupMapLayout.height = height;

    for (i = 0; i < count; i++)
    {
        const struct MapHeader *header = Overworld_GetMapHeaderByGroupAndId(maps[i].mapGroup, maps[i].mapNum);

        if (header == NULL || header->mapLayout == NULL)
            continue;
        // The map's local (0,0) in home-frame coords is maps[i].dx/dy; its buffer cell is that minus
        // the anchor's offset, plus the MAP_OFFSET that frames the anchor map.
        BlitMapIntoBuffer(header->mapLayout,
                          (maps[i].dx - anchor->dx) + MAP_OFFSET,
                          (maps[i].dy - anchor->dy) + MAP_OFFSET);
    }

    sCameraViewShiftX = anchor->dx;
    sCameraViewShiftY = anchor->dy;
    return TRUE;
}

// Whether the in-view set (anchor + member maps with their offsets) differs from what is currently
// stitched, so we only rebuild the buffer when it would actually change.
static bool8 ViewChangedSinceLastStitch(const struct ViewMap *anchor, const struct ViewMap *maps, u8 count)
{
    u8 i;

    if (!sCameraViewRoaming || count != sLastStitchCount
     || anchor->mapGroup != sLastStitchAnchor.mapGroup || anchor->mapNum != sLastStitchAnchor.mapNum
     || anchor->dx != sLastStitchAnchor.dx || anchor->dy != sLastStitchAnchor.dy)
        return TRUE;

    for (i = 0; i < count; i++)
    {
        if (maps[i].mapGroup != sLastStitchMaps[i].mapGroup || maps[i].mapNum != sLastStitchMaps[i].mapNum
         || maps[i].dx != sLastStitchMaps[i].dx || maps[i].dy != sLastStitchMaps[i].dy)
            return TRUE;
    }
    return FALSE;
}

// Keep the backup buffer aligned with the camera as it roams across map connections. While the camera
// is on the home map this is a no-op (the buffer is the canonical one InitMap built); once it crosses
// onto another map the buffer re-centers on that anchor, and on return home the canonical buffer is
// restored. Driven from CameraUpdate, after the in-view set is rebuilt and before the slice redraw.
void StitchCameraView(void)
{
    struct ViewMap anchor;
    const struct ViewMap *maps;
    u8 count, i;
    bool8 onHome;

    GetCameraViewAnchor(&anchor);
    count = GetMapsInView(&maps);
    onHome = (anchor.mapGroup == gSaveBlock1Ptr->location.mapGroup
           && anchor.mapNum == gSaveBlock1Ptr->location.mapNum
           && anchor.dx == 0 && anchor.dy == 0);

    // Weather follows the camera across boundaries, but only the DISPLAYED weather — the saved weather
    // (gSaveBlock1Ptr->weather) must stay the player's home-map weather so a save while roaming records
    // the player's weather, not the camera's. Preview the roamed map's weather, and on return restore
    // the player's saved weather (which scripts may have changed during play, so use DoCurrentWeather).
    if (anchor.mapGroup != sCameraWeatherMapGroup || anchor.mapNum != sCameraWeatherMapNum)
    {
        if (onHome)
        {
            DoCurrentWeather();
            sCameraWeatherMapGroup = anchor.mapGroup;
            sCameraWeatherMapNum = anchor.mapNum;
        }
        else
        {
            const struct MapHeader *anchorHeader = Overworld_GetMapHeaderByGroupAndId(anchor.mapGroup, anchor.mapNum);
            if (anchorHeader != NULL)
            {
                SetNextWeatherFromMapHeader(anchorHeader);
                sCameraWeatherMapGroup = anchor.mapGroup;
                sCameraWeatherMapNum = anchor.mapNum;
            }
        }
    }

    if (onHome)
    {
        // Restore the canonical full-ring buffer once, the first frame back on the home map.
        if (sCameraViewRoaming)
            InitMapLayoutData(&gMapHeader);
        return;
    }

    if (!ViewChangedSinceLastStitch(&anchor, maps, count))
        return;

    if (!StitchAnchorBuffer(&anchor, maps, count))
        return;

    sCameraViewRoaming = TRUE;
    sLastStitchAnchor = anchor;
    sLastStitchCount = count;
    for (i = 0; i < count; i++)
        sLastStitchMaps[i] = maps[i];
}

static void InitBackupMapLayoutData(const u16 *map, const u8 *attributes, u16 width, u16 height)
{
    u16 *dest;
    u8 *attrDest;
    int y;
    dest = gBackupMapLayout.map;
    dest += gBackupMapLayout.width * 7 + MAP_OFFSET;
    attrDest = gBackupMapLayout.attributes;
    attrDest += gBackupMapLayout.width * 7 + MAP_OFFSET;
    for (y = 0; y < height; y++)
    {
        CpuCopy16(map, dest, width * 2);
        memcpy(attrDest, attributes, width);
        dest += width + MAP_OFFSET_W;
        attrDest += width + MAP_OFFSET_W;
        map += width;
        attributes += width;
    }
}

static void InitBackupMapLayoutConnections(struct MapHeader *mapHeader)
{
    int count;
    const struct MapConnection *connection;
    int i;

    if (mapHeader->connections)
    {
        count = mapHeader->connections->count;
        connection = mapHeader->connections->connections;
        sMapConnectionFlags = sDummyConnectionFlags;
        for (i = 0; i < count; i++, connection++)
        {
            struct MapHeader const *cMap = GetMapHeaderFromConnection(connection);
            s32 offset = connection->offset;
            switch (connection->direction)
            {
            case CONNECTION_SOUTH:
                FillSouthConnection(mapHeader, cMap, offset);
                sMapConnectionFlags.south = TRUE;
                break;
            case CONNECTION_NORTH:
                FillNorthConnection(mapHeader, cMap, offset);
                sMapConnectionFlags.north = TRUE;
                break;
            case CONNECTION_WEST:
                FillWestConnection(mapHeader, cMap, offset);
                sMapConnectionFlags.west = TRUE;
                break;
            case CONNECTION_EAST:
                FillEastConnection(mapHeader, cMap, offset);
                sMapConnectionFlags.east = TRUE;
                break;
            case CONNECTION_SOUTHWEST:
                FillSouthWestConnection(mapHeader, cMap, offset);
                sMapConnectionFlags.southwest = TRUE;
                break;
            case CONNECTION_SOUTHEAST:
                FillSouthEastConnection(mapHeader, cMap, offset);
                sMapConnectionFlags.southeast = TRUE;
                break;
            case CONNECTION_NORTHWEST:
                FillNorthWestConnection(mapHeader, cMap, offset);
                sMapConnectionFlags.northwest = TRUE;
                break;
            case CONNECTION_NORTHEAST:
                FillNorthEastConnection(mapHeader, cMap, offset);
                sMapConnectionFlags.northeast = TRUE;
                break;
            }
        }
    }
}

static void FillConnection(int x, int y, struct MapHeader const *connectedMapHeader, int x2, int y2, int width, int height)
{
    int i;
    const u16 *src;
    u16 *dest;
    const u8 *attrSrc;
    u8 *attrDest;
    int mapWidth;

    mapWidth = connectedMapHeader->mapLayout->width;
    src = &connectedMapHeader->mapLayout->map[mapWidth * y2 + x2];
    dest = &gBackupMapLayout.map[gBackupMapLayout.width * y + x];
    attrSrc = &connectedMapHeader->mapLayout->mapAttributes[mapWidth * y2 + x2];
    attrDest = &gBackupMapLayout.attributes[gBackupMapLayout.width * y + x];

    for (i = 0; i < height; i++)
    {
        CpuCopy16(src, dest, width * 2);
        memcpy(attrDest, attrSrc, width);
        dest += gBackupMapLayout.width;
        src += mapWidth;
        attrDest += gBackupMapLayout.width;
        attrSrc += mapWidth;
    }
}

static void FillSouthConnection(struct MapHeader const *mapHeader, struct MapHeader const *connectedMapHeader, s32 offset)
{
    int x, y;
    int x2;
    int width;
    int cWidth;

    if (connectedMapHeader)
    {
        cWidth = connectedMapHeader->mapLayout->width;
        x = offset + MAP_OFFSET;
        y = mapHeader->mapLayout->height + MAP_OFFSET;
        if (x < 0)
        {
            x2 = -x;
            x += cWidth;
            if (x < gBackupMapLayout.width)
                width = x;
            else
                width = gBackupMapLayout.width;
            x = 0;
        }
        else
        {
            x2 = 0;
            if (x + cWidth < gBackupMapLayout.width)
                width = cWidth;
            else
                width = gBackupMapLayout.width - x;
        }

        FillConnection(
            x, y,
            connectedMapHeader,
            x2, /*y2*/ 0,
            width, /*height*/ MAP_OFFSET);
    }
}

static void FillNorthConnection(struct MapHeader const *mapHeader, struct MapHeader const *connectedMapHeader, s32 offset)
{
    int x;
    int x2, y2;
    int width;
    int cWidth, cHeight;

    if (connectedMapHeader)
    {
        cWidth = connectedMapHeader->mapLayout->width;
        cHeight = connectedMapHeader->mapLayout->height;
        x = offset + MAP_OFFSET;
        y2 = cHeight - MAP_OFFSET;
        if (x < 0)
        {
            x2 = -x;
            x += cWidth;
            if (x < gBackupMapLayout.width)
                width = x;
            else
                width = gBackupMapLayout.width;
            x = 0;
        }
        else
        {
            x2 = 0;
            if (x + cWidth < gBackupMapLayout.width)
                width = cWidth;
            else
                width = gBackupMapLayout.width - x;
        }

        FillConnection(
            x, /*y*/ 0,
            connectedMapHeader,
            x2, y2,
            width, /*height*/ MAP_OFFSET);

    }
}

static void FillWestConnection(struct MapHeader const *mapHeader, struct MapHeader const *connectedMapHeader, s32 offset)
{
    int y;
    int x2, y2;
    int height;
    int cWidth, cHeight;
    if (connectedMapHeader)
    {
        cWidth = connectedMapHeader->mapLayout->width;
        cHeight = connectedMapHeader->mapLayout->height;
        y = offset + MAP_OFFSET;
        x2 = cWidth - MAP_OFFSET;
        if (y < 0)
        {
            y2 = -y;
            if (y + cHeight < gBackupMapLayout.height)
                height = y + cHeight;
            else
                height = gBackupMapLayout.height;
            y = 0;
        }
        else
        {
            y2 = 0;
            if (y + cHeight < gBackupMapLayout.height)
                height = cHeight;
            else
                height = gBackupMapLayout.height - y;
        }

        FillConnection(
            /*x*/ 0, y,
            connectedMapHeader,
            x2, y2,
            /*width*/ MAP_OFFSET, height);
    }
}

static void FillEastConnection(struct MapHeader const *mapHeader, struct MapHeader const *connectedMapHeader, s32 offset)
{
    int x, y;
    int y2;
    int height;
    int cHeight;
    if (connectedMapHeader)
    {
        cHeight = connectedMapHeader->mapLayout->height;
        x = mapHeader->mapLayout->width + MAP_OFFSET;
        y = offset + MAP_OFFSET;
        if (y < 0)
        {
            y2 = -y;
            if (y + cHeight < gBackupMapLayout.height)
                height = y + cHeight;
            else
                height = gBackupMapLayout.height;
            y = 0;
        }
        else
        {
            y2 = 0;
            if (y + cHeight < gBackupMapLayout.height)
                height = cHeight;
            else
                height = gBackupMapLayout.height - y;
        }

        FillConnection(
            x, y,
            connectedMapHeader,
            /*x2*/ 0, y2,
            /*width*/ MAP_OFFSET + 1, height);
    }
}

static void FillSouthWestConnection(struct MapHeader const *mapHeader, struct MapHeader const *connectedMapHeader, s32 offset)
{
    int y;
    int cWidth;

    if (connectedMapHeader)
    {
        cWidth = connectedMapHeader->mapLayout->width;
        y = mapHeader->mapLayout->height + MAP_OFFSET;

        FillConnection(
            /*x*/ 0, y,
            connectedMapHeader,
            /*x2*/ cWidth - MAP_OFFSET, /*y2*/ 0,
            /*width*/ MAP_OFFSET, /*height*/ MAP_OFFSET);
    }
}

static void FillSouthEastConnection(struct MapHeader const *mapHeader, struct MapHeader const *connectedMapHeader, s32 offset)
{
    int x, y;

    if (connectedMapHeader)
    {
        x = mapHeader->mapLayout->width + MAP_OFFSET;
        y = mapHeader->mapLayout->height + MAP_OFFSET;

        FillConnection(
            x, y,
            connectedMapHeader,
            /*x2*/ 0, /*y2*/ 0,
            /*width*/ MAP_OFFSET + 1, /*height*/ MAP_OFFSET);
    }
}

static void FillNorthWestConnection(struct MapHeader const *mapHeader, struct MapHeader const *connectedMapHeader, s32 offset)
{
    int cWidth, cHeight;

    if (connectedMapHeader)
    {
        cWidth = connectedMapHeader->mapLayout->width;
        cHeight = connectedMapHeader->mapLayout->height;

        FillConnection(
            /*x*/ 0, /*y*/ 0,
            connectedMapHeader,
            /*x2*/ cWidth - MAP_OFFSET, /*y2*/ cHeight - MAP_OFFSET,
            /*width*/ MAP_OFFSET, /*height*/ MAP_OFFSET);
    }
}

static void FillNorthEastConnection(struct MapHeader const *mapHeader, struct MapHeader const *connectedMapHeader, s32 offset)
{
    int x;
    int cHeight;

    if (connectedMapHeader)
    {
        cHeight = connectedMapHeader->mapLayout->height;
        x = mapHeader->mapLayout->width + MAP_OFFSET;

        FillConnection(
            x, /*y*/ 0,
            connectedMapHeader,
            /*x2*/ 0, /*y2*/ cHeight - MAP_OFFSET,
            /*width*/ MAP_OFFSET + 1, /*height*/ MAP_OFFSET);
    }
}

u8 MapGridGetElevationAt(int x, int y)
{
    return UNPACK_ELEVATION(GetMapGridAttrAt(x, y));
}

u8 MapGridGetCollisionAt(int x, int y)
{
    u8 attr = GetMapGridAttrAt(x, y);

    // Impassable if a script has set the runtime collision override, or the tile's
    // painted elevation is the dedicated collision value.
    if (attr & MAPATTR_COLLISION)
        return TRUE;
    return UNPACK_ELEVATION(attr) == ELEVATION_COLLISION;
}

u8 MapGridGetMetatileLocationAt(int x, int y)
{
    return UNPACK_LOCATION(GetMapGridBlockAt(x, y));
}

// A tile's biome field is a per-group relative id. These tables convert a relative id to its
// absolute biome id (BIOME_*) for the cave and underwater groups. The overworld group's
// relative ids already match the biome ids, so it needs no table.
static const u8 sCaveBiomes[BIOME_CV_COUNT] = {
    [BIOME_CV_NONE]  = BIOME_NONE,
    [BIOME_CV_CAVE]  = BIOME_CAVE,
    [BIOME_CV_DRY]   = BIOME_CAVE_DRY,
    [BIOME_CV_DAMP]  = BIOME_CAVE_DAMP,
    [BIOME_CV_MOSSY] = BIOME_CAVE_MOSSY,
    [BIOME_CV_SANDY] = BIOME_CAVE_SANDY,
};

static const u8 sUnderwaterBiomes[BIOME_UW_COUNT] = {
    [BIOME_UW_NONE]    = BIOME_NONE,
    [BIOME_UW_SHALLOW] = BIOME_UNDERWATER_SHALLOW,
    [BIOME_UW_DEEP]    = BIOME_UNDERWATER_DEEP,
    [BIOME_UW_ABYSS]   = BIOME_UNDERWATER_ABYSS,
};

// Resolves the absolute biome id (BIOME_*) for a tile from its per-group relative biome field
// and the current map's biome group (see struct MapHeader.biomeGroup).
u8 MapGridGetMetatileBiomeAt(int x, int y)
{
    u8 relativeBiome = UNPACK_BIOME(GetMapGridBlockAt(x, y));

    switch (gMapHeader.biomeGroup)
    {
    case BIOME_GROUP_OVERWORLD:
        // Overworld relative ids are identical to the biome ids.
        return relativeBiome;
    case BIOME_GROUP_CAVE:
        return relativeBiome < BIOME_CV_COUNT ? sCaveBiomes[relativeBiome] : BIOME_NONE;
    case BIOME_GROUP_UNDERWATER:
        return relativeBiome < BIOME_UW_COUNT ? sUnderwaterBiomes[relativeBiome] : BIOME_NONE;
    case BIOME_GROUP_NONE:
    default:
        return BIOME_NONE;
    }
}

u32 MapGridGetMetatileIdAt(int x, int y)
{
    u16 block = GetMapGridBlockAt(x, y);

    if (block == MAPGRID_UNDEFINED)
        return UNPACK_METATILE(GetBorderBlockAt(x, y));

    return UNPACK_METATILE(block);
}

u32 MapGridGetMetatileBehaviorAt(int x, int y)
{
    u16 metatile = MapGridGetMetatileIdAt(x, y);
    return UNPACK_BEHAVIOR(GetMetatileAttributesById(metatile));
}

u8 MapGridGetMetatileLayerTypeAt(int x, int y)
{
    u16 metatile = MapGridGetMetatileIdAt(x, y);
    return UNPACK_LAYER_TYPE(GetMetatileAttributesById(metatile));
}

void MapGridSetMetatileIdAt(int x, int y, u16 metatile)
{
    int i;
    if (AreCoordsWithinMapGridBounds(x, y))
    {
        i = x + y * gBackupMapLayout.width;

        // The MAPGRID_IMPASSABLE sentinel in the argument marks the tile impassable;
        // otherwise the tile is made passable. This toggles the runtime collision
        // override only, so the tile's painted elevation is preserved in the attribute
        // byte; location and biome are preserved in the block.
        if (metatile & MAPGRID_IMPASSABLE)
            gBackupMapLayout.attributes[i] |= MAPATTR_COLLISION;
        else
            gBackupMapLayout.attributes[i] &= ~MAPATTR_COLLISION;

        gBackupMapLayout.map[i] = (gBackupMapLayout.map[i] & ~MAPGRID_METATILE_ID_MASK) | (metatile & MAPGRID_METATILE_ID_MASK);
    }
}

void MapGridSetMetatileEntryAt(int x, int y, u16 metatile)
{
    int i;
    if (AreCoordsWithinMapGridBounds(x, y))
    {
        i = x + gBackupMapLayout.width * y;
        gBackupMapLayout.map[i] = metatile;
    }
}

u16 GetMetatileAttributesById(u16 metatile)
{
    const u16 *attributes;
    if (metatile < NUM_METATILES_IN_PRIMARY)
    {
        attributes = gMapHeader.mapLayout->primaryTileset->metatileAttributes;
        return attributes[metatile];
    }
    else if (metatile < NUM_METATILES_TOTAL)
    {
        attributes = GetActiveLocationData()->secondaryTileset->metatileAttributes;
        return attributes[metatile - NUM_METATILES_IN_PRIMARY];
    }
    else
    {
        return MB_INVALID;
    }
}

// Captures the on-screen metatile view (the MAP_OFFSET_W x MAP_OFFSET_H window whose
// top-left map tile is x,y) into gSaveBlock1Ptr->mapView.
static void SaveMapViewAt(int x, int y)
{
    int i, j;
    u16 *mapView = gSaveBlock1Ptr->mapView;
    int width = gBackupMapLayout.width;

    for (i = y; i < y + MAP_OFFSET_H; i++)
    {
        for (j = x; j < x + MAP_OFFSET_W; j++)
            *mapView++ = sBackupMapData[width * i + j];
    }
}

void SaveMapView(void)
{
    // Persisted with the save and reloaded around the player's tile (see LoadSavedMapView), so the
    // capture must be the player's surroundings on the home map. The camera can be detached on another
    // map (debug freecam), leaving the backup buffer stitched around the camera rather than the player;
    // rebuild the canonical home buffer for the capture, then restore the camera's stitched view. During
    // normal play — and while the camera is detached but still on the home map — the buffer already is
    // the home one, so this is a plain capture.
    if (sCameraViewRoaming)
    {
        u8 weatherMapGroup = sCameraWeatherMapGroup;
        u8 weatherMapNum = sCameraWeatherMapNum;

        InitMapLayoutData(&gMapHeader);
        SaveMapViewAt(gSaveBlock1Ptr->playerPos.x, gSaveBlock1Ptr->playerPos.y);
        // Restore the weather tracker so re-stitching doesn't re-transition to the (unchanged) weather.
        sCameraWeatherMapGroup = weatherMapGroup;
        sCameraWeatherMapNum = weatherMapNum;
        StitchCameraView();
    }
    else
    {
        SaveMapViewAt(gSaveBlock1Ptr->playerPos.x, gSaveBlock1Ptr->playerPos.y);
    }
}

static bool32 SavedMapViewIsEmpty(void)
{
    u16 i;
    u32 marker = 0;

#ifndef UBFIX
    // BUG: This loop extends past the bounds of the mapView array. Its size is only 0x100.
    for (i = 0; i < 0x200; i++)
        marker |= gSaveBlock1Ptr->mapView[i];
#else
    // UBFIX: Only iterate over 0x100
    for (i = 0; i < ARRAY_COUNT(gSaveBlock1Ptr->mapView); i++)
        marker |= gSaveBlock1Ptr->mapView[i];
#endif


    if (marker == 0)
        return TRUE;
    else
        return FALSE;
}

static void ClearSavedMapView(void)
{
    CpuFill16(0, gSaveBlock1Ptr->mapView, sizeof(gSaveBlock1Ptr->mapView));
}

static void LoadSavedMapView(void)
{
    u8 yMode;
    int i, j;
    int x, y;
    u16 *mapView;
    int width;
    mapView = gSaveBlock1Ptr->mapView;
    if (!SavedMapViewIsEmpty())
    {
        width = gBackupMapLayout.width;
        // Restore around the player, matching how SaveMapView captured it (the camera may
        // have been detached when the game was saved).
        x = gSaveBlock1Ptr->playerPos.x;
        y = gSaveBlock1Ptr->playerPos.y;
        for (i = y; i < y + MAP_OFFSET_H; i++)
        {
            if (i == y && i != 0)
                yMode = 0;
            else if (i == y + MAP_OFFSET_H - 1 && i != gMapHeader.mapLayout->height - 1)
                yMode = 1;
            else
                yMode = 0xFF;

            for (j = x; j < x + MAP_OFFSET_W; j++)
            {
                if (!SkipCopyingMetatileFromSavedMap(&sBackupMapData[j + width * i], width, yMode))
                    sBackupMapData[j + width * i] = *mapView;
                mapView++;
            }
        }
        for (j = x; j < x + MAP_OFFSET_W; j++)
        {
            if (y != 0)
                FixLongGrassMetatilesWindowTop(j, y - 1);
            if (i < gMapHeader.mapLayout->height - 1)
                FixLongGrassMetatilesWindowBottom(j, y + MAP_OFFSET_H - 1);
        }
        ClearSavedMapView();
    }
}

static void MoveMapViewToBackup(u8 direction)
{
    int width;
    u16 *mapView;
    int x0, y0;
    int x2, y2;
    u16 *src, *dest;
    int srci, desti;
    int r9, r8;
    int x, y;
    int i, j;
    mapView = gSaveBlock1Ptr->mapView;
    width = gBackupMapLayout.width;
    r9 = 0;
    r8 = 0;
    x0 = gCameraPos.x;
    y0 = gCameraPos.y;
    x2 = MAP_OFFSET_W;
    y2 = MAP_OFFSET_H;
    switch (direction)
    {
    case CONNECTION_NORTH:
        y0 += 1;
        y2 = MAP_OFFSET_H - 1;
        break;
    case CONNECTION_SOUTH:
        r8 = 1;
        y2 = MAP_OFFSET_H - 1;
        break;
    case CONNECTION_WEST:
        x0 += 1;
        x2 = MAP_OFFSET_W - 1;
        break;
    case CONNECTION_EAST:
        r9 = 1;
        x2 = MAP_OFFSET_W - 1;
        break;
    }
    for (y = 0; y < y2; y++)
    {
        i = 0;
        j = 0;
        for (x = 0; x < x2; x++)
        {
            desti = width * (y + y0);
            srci = (y + r8) * MAP_OFFSET_W + r9;
            src = &mapView[srci + i];
            dest = &sBackupMapData[x0 + desti + j];
            *dest = *src;
            i++;
            j++;
        }
    }
    ClearSavedMapView();
}

int GetMapBorderIdAt(int x, int y)
{
    if (GetMapGridBlockAt(x, y) == MAPGRID_UNDEFINED)
        return CONNECTION_INVALID;

    if (x >= (gBackupMapLayout.width - (MAP_OFFSET + 1)))
    {
        if (!sMapConnectionFlags.east)
            return CONNECTION_INVALID;

        return CONNECTION_EAST;
    }
    else if (x < MAP_OFFSET)
    {
        if (!sMapConnectionFlags.west)
            return CONNECTION_INVALID;

        return CONNECTION_WEST;
    }
    else if (y >= (gBackupMapLayout.height - MAP_OFFSET))
    {
        if (!sMapConnectionFlags.south)
            return CONNECTION_INVALID;

        return CONNECTION_SOUTH;
    }
    else if (y < MAP_OFFSET)
    {
        if (!sMapConnectionFlags.north)
            return CONNECTION_INVALID;

        return CONNECTION_NORTH;
    }
    else
    {
        return CONNECTION_NONE;
    }
}

int GetPostCameraMoveMapBorderId(int x, int y)
{
    return GetMapBorderIdAt(gCameraPos.x + MAP_OFFSET + x, gCameraPos.y + MAP_OFFSET + y);
}

bool32 CanCameraMoveInDirection(int direction)
{
    int x, y;
    x = gCameraPos.x + MAP_OFFSET + gDirectionToVectors[direction].x;
    y = gCameraPos.y + MAP_OFFSET + gDirectionToVectors[direction].y;

    if (GetMapBorderIdAt(x, y) == CONNECTION_INVALID)
        return FALSE;

    return TRUE;
}

static void SetPositionFromConnection(const struct MapConnection *connection, int direction, int x, int y)
{
    struct MapHeader const *mapHeader;
    mapHeader = GetMapHeaderFromConnection(connection);
    switch (direction)
    {
    case CONNECTION_EAST:
        gCameraPos.x = -x;
        gCameraPos.y -= connection->offset;
        break;
    case CONNECTION_WEST:
        gCameraPos.x = mapHeader->mapLayout->width;
        gCameraPos.y -= connection->offset;
        break;
    case CONNECTION_SOUTH:
        gCameraPos.x -= connection->offset;
        gCameraPos.y = -y;
        break;
    case CONNECTION_NORTH:
        gCameraPos.x -= connection->offset;
        gCameraPos.y = mapHeader->mapLayout->height;
        break;
    }
}

// Whether the home-frame tile (x, y) has real map data in the current view buffer (i.e. some in-view
// map covers it), as opposed to empty space past the edge of the connected world. The freecam uses
// this to stop roaming at the boundary of the stitched plane instead of drifting into the void.
bool8 IsCameraTileDefined(s16 x, s16 y)
{
    return GetMapGridBlockAt(x + MAP_OFFSET, y + MAP_OFFSET) != MAPGRID_UNDEFINED;
}

bool8 CameraMove(int x, int y)
{
    int direction;
    const struct MapConnection *connection;
    int old_x, old_y;
    gCamera.active = FALSE;
    // The freecam roams in the home frame without ever making a connected map "current": it just
    // advances the camera tile and lets StitchCameraView slide the render buffer across the seam. The
    // full connection transition (with its music/script/warp side effects) is reserved for the player.
    if (IsFreecamActive())
    {
        gCameraPos.x += x;
        gCameraPos.y += y;
        return gCamera.active;
    }
    direction = GetPostCameraMoveMapBorderId(x, y);
    if (direction == CONNECTION_NONE || direction == CONNECTION_INVALID)
    {
        gCameraPos.x += x;
        gCameraPos.y += y;
    }
    else
    {
        // Connection transition: this captured strip is re-stitched into the destination map
        // by MoveMapViewToBackup using the camera's tile, so it must be camera-anchored (the
        // camera is what crosses the border). This is transient and cleared before any save.
        SaveMapViewAt(gCameraPos.x, gCameraPos.y);
        ClearMirageTowerPulseBlendEffect();
        old_x = gCameraPos.x;
        old_y = gCameraPos.y;
        connection = GetIncomingConnection(direction, gCameraPos.x, gCameraPos.y);
        SetPositionFromConnection(connection, direction, x, y);
        LoadMapFromCameraTransition(connection->mapGroup, connection->mapNum);
        gCamera.active = TRUE;
        gCamera.x = old_x - gCameraPos.x;
        gCamera.y = old_y - gCameraPos.y;
        gCameraPos.x += x;
        gCameraPos.y += y;
        MoveMapViewToBackup(direction);
    }
    return gCamera.active;
}

static const struct MapConnection *GetIncomingConnection(u8 direction, int x, int y)
{
    int count;
    int i;
    const struct MapConnection *connection;
    const struct MapConnections *connections = gMapHeader.connections;

#ifdef UBFIX // UB: Multiple possible null dereferences
    if (connections == NULL || connections->connections == NULL)
        return NULL;
#endif
    count = connections->count;
    connection = connections->connections;
    for (i = 0; i < count; i++, connection++)
    {
        if (connection->direction == direction && IsPosInIncomingConnectingMap(direction, x, y, connection) == TRUE)
            return connection;
    }
    return NULL;
}

static bool8 IsPosInIncomingConnectingMap(u8 direction, int x, int y, const struct MapConnection *connection)
{
    struct MapHeader const *mapHeader;
    mapHeader = GetMapHeaderFromConnection(connection);
    switch (direction)
    {
    case CONNECTION_SOUTH:
    case CONNECTION_NORTH:
        return IsCoordInIncomingConnectingMap(x, gMapHeader.mapLayout->width, mapHeader->mapLayout->width, connection->offset);
    case CONNECTION_WEST:
    case CONNECTION_EAST:
        return IsCoordInIncomingConnectingMap(y, gMapHeader.mapLayout->height, mapHeader->mapLayout->height, connection->offset);
    }
    return FALSE;
}

static bool8 IsCoordInIncomingConnectingMap(int coord, int srcMax, int destMax, int offset)
{
    int offset2;
    offset2 = offset;

    if (offset2 < 0)
        offset2 = 0;

    if (destMax + offset < srcMax)
        srcMax = destMax + offset;

    if (offset2 <= coord && coord <= srcMax)
        return TRUE;

    return FALSE;
}

static int IsCoordInConnectingMap(int coord, int max)
{
    if (coord >= 0 && coord < max)
        return TRUE;

    return FALSE;
}

static int IsPosInConnectingMap(const struct MapConnection *connection, int x, int y)
{
    struct MapHeader const *mapHeader;
    mapHeader = GetMapHeaderFromConnection(connection);
    switch (connection->direction)
    {
    case CONNECTION_SOUTH:
    case CONNECTION_NORTH:
        return IsCoordInConnectingMap(x - connection->offset, mapHeader->mapLayout->width);
    case CONNECTION_WEST:
    case CONNECTION_EAST:
        return IsCoordInConnectingMap(y - connection->offset, mapHeader->mapLayout->height);
    }
    return FALSE;
}

const struct MapConnection *GetMapConnectionAtPos(s16 x, s16 y)
{
    int count;
    const struct MapConnection *connection;
    int i;
    u8 direction;
    if (!gMapHeader.connections)
    {
        return NULL;
    }
    else
    {
        count = gMapHeader.connections->count;
        connection = gMapHeader.connections->connections;
        for (i = 0; i < count; i++, connection++)
        {
            direction = connection->direction;
            if ((direction == CONNECTION_DIVE || direction == CONNECTION_EMERGE)
             || (direction == CONNECTION_NORTH && y > MAP_OFFSET - 1)
             || (direction == CONNECTION_SOUTH && y < gMapHeader.mapLayout->height + MAP_OFFSET)
             || (direction == CONNECTION_WEST && x > MAP_OFFSET - 1)
             || (direction == CONNECTION_EAST && x < gMapHeader.mapLayout->width + MAP_OFFSET))
            {
                continue;
            }
            if (IsPosInConnectingMap(connection, x - MAP_OFFSET, y - MAP_OFFSET) == TRUE)
            {
                return connection;
            }
        }
    }
    return NULL;
}

void SetCameraFocusCoords(u16 x, u16 y)
{
    gCameraPos.x = x - MAP_OFFSET;
    gCameraPos.y = y - MAP_OFFSET;
}

void GetCameraFocusCoords(u16 *x, u16 *y)
{
    *x = gCameraPos.x + MAP_OFFSET;
    *y = gCameraPos.y + MAP_OFFSET;
}

static void UNUSED SetCameraCoords(u16 x, u16 y)
{
    gCameraPos.x = x;
    gCameraPos.y = y;
}

void GetCameraCoords(u16 *x, u16 *y)
{
    *x = gCameraPos.x;
    *y = gCameraPos.y;
}

void MapGridSetMetatileImpassabilityAt(int x, int y, bool32 impassable)
{
    if (AreCoordsWithinMapGridBounds(x, y))
    {
        if (impassable)
            gBackupMapLayout.attributes[x + gBackupMapLayout.width * y] |= MAPATTR_COLLISION;
        else
            gBackupMapLayout.attributes[x + gBackupMapLayout.width * y] &= ~MAPATTR_COLLISION;
    }
}

void MapGridSetMetatileElevationAt(int x, int y, u8 elevation)
{
    if (AreCoordsWithinMapGridBounds(x, y))
    {
        u8 *attr = &gBackupMapLayout.attributes[x + gBackupMapLayout.width * y];
        *attr = (*attr & ~MAPATTR_ELEVATION_MASK) | PACK_ELEVATION(elevation);
    }
}

void MapGridSetMetatileLocationAt(int x, int y, u8 location)
{
    if (AreCoordsWithinMapGridBounds(x, y))
    {
        u16 *block = &gBackupMapLayout.map[x + gBackupMapLayout.width * y];
        *block = (*block & ~MAPGRID_LOCATION_MASK) | PACK_LOCATION(location);
    }
}

static bool8 SkipCopyingMetatileFromSavedMap(u16 *mapBlock, u16 mapWidth, u8 yMode)
{
    if (yMode == 0xFF)
        return FALSE;

    if (yMode == 0)
        mapBlock -= mapWidth;
    else
        mapBlock += mapWidth;

    if (IsLargeBreakableDecoration(UNPACK_METATILE(*mapBlock), yMode) == TRUE)
        return TRUE;
    return FALSE;
}

static void CopyTilesetToVram(struct Tileset const *tileset, u16 numTiles, u16 offset)
{
    if (tileset)
    {
        if (!tileset->isCompressed)
            LoadBgTiles(2, tileset->tiles, numTiles * 32, offset);
        else
            DecompressAndCopyTileDataToVram(2, tileset->tiles, numTiles * 32, offset, 0);
    }
}

static void CopyTilesetToVramUsingHeap(struct Tileset const *tileset, u16 numTiles, u16 offset)
{
    if (tileset)
    {
        if (!tileset->isCompressed)
            LoadBgTiles(2, tileset->tiles, numTiles * 32, offset);
        else
            DecompressAndLoadBgGfxUsingHeap(2, tileset->tiles, numTiles * 32, offset, 0);
    }
}

// Below two are dummied functions from FRLG, used to tint the overworld palettes for the Quest Log
static void ApplyGlobalTintToPaletteEntries(u16 offset, u16 size)
{

}

static void UNUSED ApplyGlobalTintToPaletteSlot(u8 slot, u8 count)
{

}

static void LoadTilesetPalette(struct Tileset const *tileset, u16 destOffset, u16 size)
{
    u16 black = RGB_BLACK;

    if (tileset)
    {
        if (tileset->isSecondary == FALSE)
        {
            LoadPalette(&black, destOffset, PLTT_SIZEOF(1));
            LoadPalette(tileset->palettes[0] + 1, destOffset + 1, size - PLTT_SIZEOF(1));
            ApplyGlobalTintToPaletteEntries(destOffset + 1, (size - PLTT_SIZEOF(1)) >> 1);
        }
        else if (tileset->isSecondary == TRUE)
        {
            LoadPalette(tileset->palettes[NUM_PALS_IN_PRIMARY], destOffset, size);
            ApplyGlobalTintToPaletteEntries(destOffset, size >> 1);
        }
        else
        {
            LoadCompressedPalette((const u32 *)tileset->palettes, destOffset, size);
            ApplyGlobalTintToPaletteEntries(destOffset, size >> 1);
        }
    }
}

void CopyPrimaryTilesetToVram(struct MapLayout const *mapLayout)
{
    CopyTilesetToVram(mapLayout->primaryTileset, NUM_TILES_IN_PRIMARY, 0);
}

void CopySecondaryTilesetToVram(struct MapLayout const *mapLayout)
{
    CopyTilesetToVram(GetActiveLocationData()->secondaryTileset, NUM_TILES_TOTAL - NUM_TILES_IN_PRIMARY, NUM_TILES_IN_PRIMARY);
}

void CopySecondaryTilesetToVramUsingHeap(struct MapLayout const *mapLayout)
{
    CopyTilesetToVramUsingHeap(GetActiveLocationData()->secondaryTileset, NUM_TILES_TOTAL - NUM_TILES_IN_PRIMARY, NUM_TILES_IN_PRIMARY);
}

static void LoadPrimaryTilesetPalette(struct MapLayout const *mapLayout)
{
    LoadTilesetPalette(mapLayout->primaryTileset, BG_PLTT_ID(0), NUM_PALS_IN_PRIMARY * PLTT_SIZE_4BPP);
}

void LoadSecondaryTilesetPalette(struct MapLayout const *mapLayout)
{
    LoadTilesetPalette(GetActiveLocationData()->secondaryTileset, BG_PLTT_ID(NUM_PALS_IN_PRIMARY), (NUM_PALS_TOTAL - NUM_PALS_IN_PRIMARY) * PLTT_SIZE_4BPP);
}

void CopyMapTilesetsToVram(struct MapLayout const *mapLayout)
{
    if (mapLayout)
    {
        CopyTilesetToVramUsingHeap(mapLayout->primaryTileset, NUM_TILES_IN_PRIMARY, 0);
        CopyTilesetToVramUsingHeap(GetActiveLocationData()->secondaryTileset, NUM_TILES_TOTAL - NUM_TILES_IN_PRIMARY, NUM_TILES_IN_PRIMARY);
    }
}

void LoadMapTilesetPalettes(struct MapLayout const *mapLayout)
{
    if (mapLayout)
    {
        LoadPrimaryTilesetPalette(mapLayout);
        LoadSecondaryTilesetPalette(mapLayout);
    }
}
