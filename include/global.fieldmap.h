#ifndef GUARD_GLOBAL_FIELDMAP_H
#define GUARD_GLOBAL_FIELDMAP_H

// Masks/shifts for blocks in the map grid
// A map grid block is a packed 16 bit value, stored in each
// data/layouts/*/map.bin file:
//   bits 0-9   metatile id (max 1024)
//   bits 10-11 location
//   bits 12-15 biome (currently unused)
#define MAPGRID_METATILE_ID_MASK  0x03FF // Bits 0-9
#define MAPGRID_METATILE_ID_SHIFT 0
#define MAPGRID_LOCATION_MASK     0x0C00 // Bits 10-11
#define MAPGRID_LOCATION_SHIFT    10
#define MAPGRID_BIOME_MASK        0xF000 // Bits 12-15
#define MAPGRID_BIOME_SHIFT       12

// Per-tile attributes are stored separately, one u16 per tile, in each
// data/layouts/*/attributes.bin file (loaded parallel to map.bin):
//   bits 0-7   elevation level (0-255, every value an ordinary level)
//   bit 8      cliff collision (impassable only to objects climbing behind a cliff)
//   bit 9      collision (impassable)
//   bits 10-13 bgMaterial (currently unused)
// Movement semantics that used to ride on special elevation values live on metatile
// behaviors: stairs (MB_FORWARD_STAIRS / MB_BACKWARD_STAIRS / MB_SIDEWAYS_STAIRS_*) gate level changes, the
// surf behaviors gate water, and multi-level is handled by the behind-cliff system.
// Scripts may toggle the collision bit at runtime (MapGridSetMetatileImpassabilityAt)
// without disturbing the elevation level.
#define MAPATTR_ELEVATION_MASK   0x00FF // Bits 0-7 (0-255)
#define MAPATTR_ELEVATION_SHIFT  0
#define MAPATTR_CLIFF_COLLISION  0x0100 // Bit 8, collision for objects behind a cliff
#define MAPATTR_COLLISION        0x0200 // Bit 9, collision (impassable)
#define MAPATTR_BGMATERIAL_MASK  0x3C00 // Bits 10-13 (unused for now)
#define MAPATTR_BGMATERIAL_SHIFT 10

// The location attribute is 2 bits, so a map can define up to 4 distinct
// per-location property sets (see struct MapHeaderLocationData / MapHeader).
#define MAX_MAP_LOCATIONS 4

enum
{
    ELEVATION_DEFAULT = 5,     // Default ground level (levels 0-63 are all valid)
    ELEVATION_INVALID = 0xFFFF
};

// Wildcard for elevation-matching occupancy queries (GetObjectEventIdByPosition):
// matches an object at any elevation. Never a real level (those are 0-63).
#define ELEVATION_MATCH_ANY 0xFF

// Map events (warps, coord/bg events, object templates) store their level in the
// low bits of their elevation byte. Bit 7 is the "any elevation" flag: when set,
// the event interacts with the player regardless of elevation (for object events
// it also forces always-on-top rendering). Levels are 0-63 so bit 7 is free.
#define EVENT_ELEVATION_ANY  0x80
#define EVENT_ELEVATION_MASK 0x7F

// PACK_METATILE/PACK_LOCATION/PACK_BIOME operate on a map grid block (map.bin u16);
// PACK_ELEVATION operates on an attribute byte (attributes.bin u8).
#define PACK_METATILE(metatileId) PACK(metatileId, MAPGRID_METATILE_ID_SHIFT, MAPGRID_METATILE_ID_MASK)
#define PACK_LOCATION(location)   PACK(location, MAPGRID_LOCATION_SHIFT, MAPGRID_LOCATION_MASK)
#define PACK_BIOME(biome)         PACK(biome, MAPGRID_BIOME_SHIFT, MAPGRID_BIOME_MASK)
#define PACK_ELEVATION(elevation) PACK(elevation, MAPATTR_ELEVATION_SHIFT, MAPATTR_ELEVATION_MASK)
#define PACK_BGMATERIAL(material)  PACK(material, MAPATTR_BGMATERIAL_SHIFT, MAPATTR_BGMATERIAL_MASK)
#define UNPACK_METATILE(data)  UNPACK(data, MAPGRID_METATILE_ID_SHIFT, MAPGRID_METATILE_ID_MASK)
#define UNPACK_LOCATION(data)  UNPACK(data, MAPGRID_LOCATION_SHIFT, MAPGRID_LOCATION_MASK)
#define UNPACK_BIOME(data)     UNPACK(data, MAPGRID_BIOME_SHIFT, MAPGRID_BIOME_MASK)
#define UNPACK_ELEVATION(attr) UNPACK(attr, MAPATTR_ELEVATION_SHIFT, MAPATTR_ELEVATION_MASK)
#define UNPACK_BGMATERIAL(attr) UNPACK(attr, MAPATTR_BGMATERIAL_SHIFT, MAPATTR_BGMATERIAL_MASK)

// An undefined map grid block has every bit set (no real block, with metatile id
// <= 0x3FF and biome 0, matches it).
#define MAPGRID_UNDEFINED   0xFFFF

// Fill value for attribute tiles not yet loaded from a map (treated as impassable
// via the runtime collision override).
#define MAPATTR_UNDEFINED   MAPATTR_COLLISION

// Sentinel flag, only valid as an argument to MapGridSetMetatileIdAt. It is not
// stored in the grid; the setter strips it and marks the tile's collision impassable.
// Uses bit 15 so it never collides with a real (<= 0x3FF) metatile id argument.
#define MAPGRID_IMPASSABLE  0x8000

// Masks/shifts for metatile attributes
// Metatile attributes consist of an 8 bit behavior value, a "use bg material" flag,
// a 2 bit reflection type, a "reflection punch" flag, and a 4 bit layer type value.
// This is the data stored in each data/tilesets/*/*/metatile_attributes.bin file
#define METATILE_ATTR_BEHAVIOR_MASK 0x00FF // Bits 0-7
#define METATILE_ATTR_BGMATERIAL    0x0100 // Bit 8, gate the bgMaterial render path
#define METATILE_ATTR_REFLECTION_MASK  0x0600 // Bits 9-10, reflection type (see MetatileReflectionType)
#define METATILE_ATTR_REFLECTION_SHIFT 9
#define METATILE_ATTR_REFLECTION_PUNCH 0x0800 // Bit 11, punch a hole in the bg plane so the reflection shows through
#define METATILE_ATTR_LAYER_MASK    0xF000 // Bits 12-15
#define METATILE_ATTR_BEHAVIOR_SHIFT 0
#define METATILE_ATTR_LAYER_SHIFT   12

#define PACK_BEHAVIOR(behavior) PACK(behavior, METATILE_ATTR_BEHAVIOR_SHIFT, METATILE_ATTR_BEHAVIOR_MASK)
#define PACK_LAYER_TYPE(layerType) PACK(layerType, METATILE_ATTR_LAYER_SHIFT, METATILE_ATTR_LAYER_MASK)
#define UNPACK_BEHAVIOR(data) UNPACK(data, METATILE_ATTR_BEHAVIOR_SHIFT, METATILE_ATTR_BEHAVIOR_MASK)
#define UNPACK_LAYER_TYPE(data) UNPACK(data, METATILE_ATTR_LAYER_SHIFT, METATILE_ATTR_LAYER_MASK)
#define UNPACK_USES_BGMATERIAL(data) (((data) & METATILE_ATTR_BGMATERIAL) != 0)
#define UNPACK_REFLECTION(data) UNPACK(data, METATILE_ATTR_REFLECTION_SHIFT, METATILE_ATTR_REFLECTION_MASK)
#define UNPACK_REFLECTION_PUNCH(data) (((data) & METATILE_ATTR_REFLECTION_PUNCH) != 0)

// Per-metatile reflection type. Drives which objects cast a reflection over the tile and how the
// tile's background layer composites (reflective tiles go to BG3, behind the reflection sprites).
// Values are kept in sync with REFL_TYPE_* (see event_object_movement.h) so the flag maps straight
// through: STILL -> REFL_TYPE_ICE, WAVY -> REFL_TYPE_WATER.
enum MetatileReflectionType
{
    METATILE_REFLECTION_NONE,  // No reflection
    METATILE_REFLECTION_STILL, // Still reflection (formerly MB_ICE)
    METATILE_REFLECTION_WAVY,  // Wavy reflection (formerly MB_PUDDLE / water)
};

// Per-metatile foreground/background compositing flags. One u16 per metatile, stored in each
// data/tilesets/*/*/metatile_compositing.bin (loaded parallel to metatile_attributes). Three 3-bit
// fields, one bit per metatile layer (0=ground, 1=middle, 2=top):
//   bits 0-2  in front of cliff: layer 0/1/2 -> foreground (else background)
//   bits 3-5  behind cliff:      layer 0/1/2 -> foreground (else background)
//   bits 6-8  reflection:        layer 0/1/2 is a reflection layer (bitmask; multiple may be set)
// A reflection layer renders to the BG3 reflective plane and (with the attribute punch flag) punches
// a transparent hole through the BG2 background composite wherever it is opaque.
#define METATILE_COMPOSITE_FRONT_SHIFT  0
#define METATILE_COMPOSITE_BEHIND_SHIFT 3
#define METATILE_COMPOSITE_REFLECTION_SHIFT 6
#define METATILE_COMPOSITE_LAYER_MASK   0x7
#define METATILE_COMPOSITE_FRONT(flags)  (((flags) >> METATILE_COMPOSITE_FRONT_SHIFT) & METATILE_COMPOSITE_LAYER_MASK)
#define METATILE_COMPOSITE_BEHIND(flags) (((flags) >> METATILE_COMPOSITE_BEHIND_SHIFT) & METATILE_COMPOSITE_LAYER_MASK)
// Reflection layer bitmask (bit per layer, 0 = no reflection layer).
#define METATILE_COMPOSITE_REFLECTION(flags) (((flags) >> METATILE_COMPOSITE_REFLECTION_SHIFT) & METATILE_COMPOSITE_LAYER_MASK)

enum {
    METATILE_LAYER_TYPE_NORMAL,  // Metatile uses middle and top bg layers
    METATILE_LAYER_TYPE_COVERED, // Metatile uses bottom and middle bg layers
    METATILE_LAYER_TYPE_SPLIT,   // Metatile uses bottom and top bg layers
};

#define METATILE_ID(tileset, name) (METATILE_##tileset##_##name)

// Rows of metatiles do not actually have a strict width.
// This constant is used for calculations for finding the next row of metatiles
// for constructing large tiles, such as the Battle Pike's curtain tile.
#define METATILE_ROW_WIDTH 8

typedef void (*TilesetCB)(void);

struct Tileset
{
    /*0x00*/ bool8 isCompressed;
    /*0x01*/ bool8 isSecondary;
    /*0x04*/ const u32 *tiles;
    /*0x08*/ const u16 *palettes; // flat: primary banks 0-5, secondary banks 6-12 (16 colours each)
    /*0x0C*/ const u16 *metatiles;
    /*0x10*/ const u16 *metatileAttributes;
    /*0x14*/ const u16 *metatileCompositing; // per-metatile fg/bg layer flags, see METATILE_COMPOSITE_*
    /*0x18*/ TilesetCB callback;
    // Optional 16-byte bank->base-offset table for the 8bpp compositor. The compositor adds
    // base[palField] to each non-zero source pixel (see field_compositor.c). NULL = default
    // base[i] = i*16 (vanilla 16-colour banks). Lets a tile relocate its colours anywhere in
    // the 256-colour overworld palette.
    /*0x1C*/ const u8 *paletteOffsets;
};

struct MapLayout
{
    /*0x00*/ s32 width;
    /*0x04*/ s32 height;
    /*0x08*/ const u16 *border;
    /*0x0C*/ const u16 *borderAttributes;
    /*0x10*/ const u16 *map;
    /*0x14*/ const u16 *mapAttributes;
    /*0x18*/ const struct Tileset *primaryTileset;
};

struct BackupMapLayout
{
    s32 width;
    s32 height;
    u16 *map;
    u16 *attributes;
};

struct ObjectEventTemplate
{
    /*0x00*/ u8 localId;
    /*0x01*/ u8 graphicsId;
    /*0x02*/ u8 kind; // Always OBJ_KIND_NORMAL in Emerald.
    /*0x03*/ //u8 padding1;
    /*0x04*/ s16 x;
    /*0x06*/ s16 y;
    /*0x08*/ u8 elevation;
    /*0x09*/ u8 movementType;
    /*0x0A*/ u16 movementRangeX:4;
             u16 movementRangeY:4;
             //u16 padding2:8;
    /*0x0C*/ u16 trainerType;
    /*0x0E*/ u16 trainerRange_berryTreeId;
    /*0x10*/ const u8 *script;
    /*0x14*/ u16 flagId;
    /*0x16*/ //u8 padding3[2];
};

struct WarpEvent
{
    s16 x, y;
    u8 elevation;
    u8 warpId;
    u8 mapNum;
    u8 mapGroup;
};

struct CoordEvent
{
    s16 x, y;
    u8 elevation;
    u16 trigger;
    u16 index;
    const u8 *script;
};

struct BgEvent
{
    u16 x, y;
    u8 elevation;
    u8 kind; // The "kind" field determines how to access bgUnion union below.
    union {
        const u8 *script;
        struct {
            u16 item;
            u16 hiddenItemId;
        } hiddenItem;
        u32 secretBaseId;
    } bgUnion;
};

struct MapEvents
{
    u8 objectEventCount;
    u8 warpCount;
    u8 coordEventCount;
    u8 bgEventCount;
    const struct ObjectEventTemplate *objectEvents;
    const struct WarpEvent *warps;
    const struct CoordEvent *coordEvents;
    const struct BgEvent *bgEvents;
};

struct MapConnection
{
    u8 direction;
    s32 offset;
    u8 mapGroup;
    u8 mapNum;
};

struct MapConnections
{
    s32 count;
    const struct MapConnection *connections;
};

// A map that overlaps the camera window, with the offset (dx, dy) that places its local (0,0) tile
// into the home (active) frame. Built by the view-frame resolver in event_object_movement.c and
// consumed by the tile-window stitcher in fieldmap.c.
#define MAX_MAPS_IN_VIEW 8 // home + 4 cardinals + the corners straddled at once
struct ViewMap
{
    u8 mapGroup;
    u8 mapNum;
    s16 dx;
    s16 dy;
};

// Properties that apply per in-map location. A map's per-tile location attribute
// (see MAPGRID_LOCATION_MASK) is intended to select which set of these properties
// is active. Each set is stored as its own const instance in ROM and referenced
// by pointer from the map header (see MapHeader.locations).
struct MapHeaderLocationData
{
    /* 0x00 */ const struct Tileset *secondaryTileset;
    /* 0x04 */ u16 music;
    /* 0x06 */ mapsec_u8_t regionMapSectionId;
    /* 0x07 */ u8 mapType;
    /* 0x08 */ u8 battleType;
    /* 0x09 */ bool8 showMapName;
};

struct MapHeader
{
    /* 0x00 */ const struct MapLayout *mapLayout;
    /* 0x04 */ const struct MapEvents *events;
    /* 0x08 */ const u8 *mapScripts;
    /* 0x0C */ const struct MapConnections *connections;
    // One pointer per location slot. The first numLocations entries point to
    // const MapHeaderLocationData in ROM; unused trailing slots are NULL.
    /* 0x10 */ const struct MapHeaderLocationData *locations[MAX_MAP_LOCATIONS];
    /* 0x20 */ u16 mapLayoutId;
    /* 0x22 */ u8 cave;
    /* 0x23 */ u8 weather;
    /* 0x24 */ u8 numLocations:2;
               u8 biomeGroup:3; // BIOME_GROUP_* (see constants/biome.h)
               u8 filler_20:3;
               // fields correspond to the arguments in the map_header_flags macro
    /* 0x25 */ bool8 allowCycling:1;
               bool8 allowEscaping:1; // Escape Rope and Dig
               bool8 allowRunning:1;
               bool8 filler_25:5; // pad the flags byte so the grid below starts cleanly at 0x26
    // Position of this map within the world map grid, which spans the region map's
    // playable area (MAP_WIDTH x MAP_HEIGHT = 22 x 18). X coord 0-21, Y coord 0-17,
    // with (0,0) at the bottom-left (region_map.c flips Y when placing the icon).
    // mapjson packs this as a .2byte at 0x26: mapGridX in bits 0-4, mapGridY in bits 5-9.
    /* 0x26 */ u16 mapGridX:5;
               u16 mapGridY:5;
               // the remaining 6 bits are unused
};

// State of a sprite relative to a cliff face it has climbed behind. Drives cliff collision and which
// tiles promote their middle layer into the foreground to occlude the sprite (UpdateCliffFacePromotion);
// the sprite itself always draws at the one flat object priority. FRONT is 0, so any non-FRONT state
// means behind the cliff.
enum CliffLayer
{
    CLIFF_LAYER_FRONT,    // in front of the cliff: rendered normally, no terrain promotion
    CLIFF_LAYER_BEHIND,   // behind the cliff face: covered tiles promote their middle layer over it
    CLIFF_LAYER_OBSCURED, // fully buried: every tile the sprite spans is higher, so it is hidden/silhouetted
};

struct ObjectEvent
{
    /*0x00*/ u32 active:1;
             u32 singleMovementActive:1;
             u32 triggerGroundEffectsOnMove:1;
             u32 triggerGroundEffectsOnStop:1;
             u32 disableCoveringGroundEffects:1;
             u32 landingJump:1;
             u32 heldMovementActive:1;
             u32 heldMovementFinished:1;
    /*0x01*/ u32 frozen:1;
             u32 facingDirectionLocked:1;
             u32 disableAnim:1;
             u32 enableAnim:1;
             u32 inanimate:1;
             u32 invisible:1;
             u32 offScreen:1;
             u32 trackedByCamera:1;
    /*0x02*/ u32 isPlayer:1;
             u32 hasReflection:1;
             u32 inShortGrass:1;
             u32 inShallowFlowingWater:1;
             u32 inSandPile:1;
             u32 inHotSprings:1;
             u32 hasShadow:1;
             u32 spriteAnimPausedBackup:1;
    /*0x03*/ u32 spriteAffineAnimPausedBackup:1;
             u32 disableJumpLandingGroundEffect:1;
             u32 fixedPriority:1;
             u32 hideReflection:1;
             u32 surfacingFromCliff:1; // surface condition met at step-commit; defer leaving the cliff until arrival
             u32 cliffLayer:2;         // CLIFF_LAYER_*: render plane relative to a cliff face
             u32 drawAtHighestElevation:1; // "any elevation" object: always drawn on top, interacts at any elevation
    /*0x04*/ u8 spriteId;
    /*0x05*/ u8 graphicsId;
    /*0x06*/ u8 movementType;
    /*0x07*/ u8 trainerType;
    /*0x08*/ u8 localId;
    /*0x09*/ u8 mapNum;
    /*0x0A*/ u8 mapGroup;
    // The elevation level the object belongs to: normally the elevation of the tile it stands
    // on, but frozen at the level it climbed up from while behind a cliff (see
    // UpdateObjectEventBehindCliff). Full 8 bits to match the map grid's 0-255 elevations.
    /*0x0B*/ u8 baseElevation;
    /*0x0C*/ struct Coords16 initialCoords;
    /*0x10*/ struct Coords16 currentCoords;
    /*0x14*/ struct Coords16 previousCoords;
    /*0x18*/ u16 facingDirection:4; // current direction?
             u16 movementDirection:4;
             struct __attribute__((packed))
             {
                u8 rangeX:4;
                u8 rangeY:4;
             } range;
    /*0x1A*/ u8 fieldEffectSpriteId;
    /*0x1B*/ u8 warpArrowSpriteId;
    /*0x1C*/ u8 movementActionId;
    /*0x1D*/ u8 trainerRange_berryTreeId;
    /*0x1E*/ u8 currentMetatileBehavior;
    /*0x1F*/ u8 previousMetatileBehavior;
    /*0x20*/ u8 previousMovementDirection:4;
             u8 directionOverwrite:4;
    /*0x21*/ u8 directionSequenceIndex;
    /*0x22*/ u8 playerCopyableMovement; // COPY_MOVE_*
    /*0x23*/ u8 cliffMetatileBehavior; // behavior of the tile left when stepping behind a cliff; held until surfacing
    /*size = 0x24*/
};

// Persisted record of an object event that has wandered off its home map onto an adjacent
// (laterally connected) map. Keyed by the object's home identity (localId + home map); records the
// map it now stands on and its tile there (map-local, without MAP_OFFSET) so it respawns at the
// displaced position instead of its template position and is not double-spawned by its home map.
// An entry with localId == 0 is empty. See event_object_movement.c wander-store functions.
struct ObjectEventWander
{
    /*0x0*/ u8 localId;       // home localId (0 = empty slot)
    /*0x1*/ u8 homeMapNum;
    /*0x2*/ u8 homeMapGroup;
    /*0x3*/ u8 curMapNum;     // map the object currently stands on
    /*0x4*/ u8 curMapGroup;
    /*0x5*/ u8 movementType;  // permanent movement-type override set by script (0xFF = none)
    /*0x6*/ s16 x;            // current tile on curMap (map-local, no MAP_OFFSET)
    /*0x8*/ s16 y;
    /*0xA*/ s16 permX;        // permanent home-map spawn tile set by script (-1 = none)
    /*0xC*/ s16 permY;
}; // size = 0xE

// "No movement-type override" sentinel for struct ObjectEventWander.movementType (no real
// movement type uses 0xFF).
#define OBJ_EVENT_WANDER_NO_MOVEMENT_TYPE 0xFF
// "No permanent-position override" sentinel for struct ObjectEventWander.permX (no real map tile
// is negative).
#define OBJ_EVENT_WANDER_NO_PERM_POS ((s16)-1)

struct ObjectEventGraphicsInfo
{
    /*0x00*/ u16 tileTag;
    /*0x02*/ u16 paletteTag;
    /*0x04*/ u16 reflectionPaletteTag;
    /*0x06*/ u16 size;
    /*0x08*/ s16 width;
    /*0x0A*/ s16 height;
    /*0x0C*/ u8 paletteSlot:4;
             u8 shadowSize:2;
             u8 inanimate:1;
             u8 disableReflectionPaletteLoad:1;
    /*0x0D*/ u8 tracks;
    /*0x10*/ const struct OamData *oam;
    /*0x14*/ const struct SubspriteTable *subspriteTables;
    /*0x18*/ const union AnimCmd *const *anims;
    /*0x1C*/ const struct SpriteFrameImage *images;
    /*0x20*/ const union AffineAnimCmd *const *affineAnims;
};

enum {
    PLAYER_AVATAR_STATE_NORMAL,
    PLAYER_AVATAR_STATE_MACH_BIKE,
    PLAYER_AVATAR_STATE_ACRO_BIKE,
    PLAYER_AVATAR_STATE_SURFING,
    PLAYER_AVATAR_STATE_UNDERWATER,
    PLAYER_AVATAR_STATE_FIELD_MOVE,
    PLAYER_AVATAR_STATE_FISHING,
    PLAYER_AVATAR_STATE_WATERING,
};

#define PLAYER_AVATAR_FLAG_ON_FOOT      (1 << 0)
#define PLAYER_AVATAR_FLAG_MACH_BIKE    (1 << 1)
#define PLAYER_AVATAR_FLAG_ACRO_BIKE    (1 << 2)
#define PLAYER_AVATAR_FLAG_SURFING      (1 << 3)
#define PLAYER_AVATAR_FLAG_UNDERWATER   (1 << 4)
#define PLAYER_AVATAR_FLAG_CONTROLLABLE (1 << 5)
#define PLAYER_AVATAR_FLAG_FORCED_MOVE  (1 << 6)
#define PLAYER_AVATAR_FLAG_DASH         (1 << 7)

enum
{
    ACRO_BIKE_NORMAL,
    ACRO_BIKE_TURNING,
    ACRO_BIKE_WHEELIE_STANDING,
    ACRO_BIKE_BUNNY_HOP,
    ACRO_BIKE_WHEELIE_MOVING,
    ACRO_BIKE_STATE5,
    ACRO_BIKE_STATE6,
};

enum
{
    COLLISION_NONE,
    COLLISION_OUTSIDE_RANGE,
    COLLISION_IMPASSABLE,
    COLLISION_ELEVATION_MISMATCH,
    COLLISION_OBJECT_EVENT,
    COLLISION_STOP_SURFING,
    COLLISION_LEDGE_JUMP,
    COLLISION_PUSHED_BOULDER,
    COLLISION_ROTATING_GATE,
    COLLISION_WHEELIE_HOP,
    COLLISION_ISOLATED_VERTICAL_RAIL,
    COLLISION_ISOLATED_HORIZONTAL_RAIL,
    COLLISION_VERTICAL_RAIL,
    COLLISION_HORIZONTAL_RAIL,
    //sideways_stairs
    COLLISION_SIDEWAYS_STAIRS_TO_RIGHT,
    COLLISION_SIDEWAYS_STAIRS_TO_LEFT
};

// player running states
enum
{
    NOT_MOVING,
    TURN_DIRECTION, // not the same as turning! turns your avatar without moving. also known as a turn frame in some circles
    MOVING,
};

// player tile transition states
enum
{
    T_NOT_MOVING,
    T_TILE_TRANSITION,
    T_TILE_CENTER, // player is on a frame in which they are centered on a tile during which the player either stops or keeps their momentum and keeps going, changing direction if necessary.
};

struct PlayerAvatar
{
    /*0x00*/ u8 flags;
    /*0x01*/ u8 transitionFlags; // used to be named bike, but its definitely not that. seems to be some transition flags
    /*0x02*/ u8 runningState; // this is a static running state. 00 is not moving, 01 is turn direction, 02 is moving.
    /*0x03*/ u8 tileTransitionState; // this is a transition running state: 00 is not moving, 01 is transition between tiles, 02 means you are on the frame in which you have centered on a tile but are about to keep moving, even if changing directions. 2 is also used for a ledge hop, since you are transitioning.
    /*0x04*/ u8 spriteId;
    /*0x05*/ u8 objectEventId;
    /*0x06*/ bool8 preventStep;
    /*0x07*/ u8 gender;
    /*0x08*/ u8 acroBikeState; // 00 is normal, 01 is turning, 02 is standing wheelie, 03 is hopping wheelie
    /*0x09*/ u8 newDirBackup; // during bike movement, the new direction as opposed to player's direction is backed up here.
    /*0x0A*/ u8 bikeFrameCounter; // on the mach bike, when this value is 1, the bike is moving but not accelerating yet for 1 tile. on the acro bike, this acts as a timer for acro bike.
    /*0x0B*/ u8 bikeSpeed;
    // acro bike only
    /*0x0C*/ u32 directionHistory; // up/down/left/right history is stored in each nybble, but using the field directions and not the io inputs.
    /*0x10*/ u32 abStartSelectHistory; // same as above but for A + B + start + select only
    // these two are timer history arrays which [0] is the active timer for acro bike. every element is backed up to the next element upon update.
    /*0x14*/ u8 dirTimerHistory[8];
    /*0x1C*/ u8 abStartSelectTimerHistory[8];
};

struct Camera
{
    bool8 active:1;
    s32 x;
    s32 y;
};

extern struct ObjectEvent gObjectEvents[OBJECT_EVENTS_COUNT];
extern u8 gSelectedObjectEvent;
extern struct MapHeader gMapHeader;
extern struct PlayerAvatar gPlayerAvatar;

// Accessor for the current map's active location property set (selected by the
// location attribute of the tile the player stands on). Defined in fieldmap.c.
const struct MapHeaderLocationData *GetActiveLocationData(void);
u8 GetActiveMapLocation(void);
void GetMapGridXY(u8 *outX, u8 *outY);
extern struct Camera gCamera;

#endif // GUARD_GLOBAL_FIELDMAP_H
