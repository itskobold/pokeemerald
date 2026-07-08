#include "global.h"
#include "graphics.h"
#include "palette.h"
#include "util.h"
#include "battle_transition.h"
#include "task.h"
#include "battle_transition.h"
#include "fieldmap.h"
#include "field_compositor.h"
#include "field_camera.h"
#include "script.h"
#include "constants/field_weather.h"

static EWRAM_DATA struct {
    const u16 *src;
    u16 *dest;
    u16 size;
} sTilesetDMA3TransferBuffer[20] = {0};

static u8 sTilesetDMA3TransferBufferSize;
static u16 sPrimaryTilesetAnimCounter;
static u16 sPrimaryTilesetAnimCounterMax;
static u16 sSecondaryTilesetAnimCounter;
static u16 sSecondaryTilesetAnimCounterMax;
static void (*sPrimaryTilesetAnimCallback)(u16);
static void (*sSecondaryTilesetAnimCallback)(u16);

static void _InitPrimaryTilesetAnimation(void);
static void _InitSecondaryTilesetAnimation(void);
static void TilesetAnim_General(u16);
static void TilesetAnim_Terrain(u16);
static void TilesetAnim_Building(u16);
static void TilesetAnim_Rustboro(u16);
static void TilesetAnim_Dewford(u16);
static void TilesetAnim_Slateport(u16);
static void TilesetAnim_Mauville(u16);
static void TilesetAnim_Lavaridge(u16);
static void TilesetAnim_EverGrande(u16);
static void TilesetAnim_Pacifidlog(u16);
static void TilesetAnim_Sootopolis(u16);
static void TilesetAnim_BattleFrontierOutsideWest(u16);
static void TilesetAnim_BattleFrontierOutsideEast(u16);
static void TilesetAnim_Underwater(u16);
static void TilesetAnim_SootopolisGym(u16);
static void TilesetAnim_Cave(u16);
static void TilesetAnim_EliteFour(u16);
static void TilesetAnim_MauvilleGym(u16);
static void TilesetAnim_BikeShop(u16);
static void TilesetAnim_BattlePyramid(u16);
static void TilesetAnim_BattleDome(u16);
static s16 PhaseFn_Terrain_Water(s16, s16);
static s16 PhaseFn_Zero(s16, s16);
static void QueueAnimTiles_General_Flower(u16);
static void QueueAnimTiles_General_Water(u16);
static void QueueAnimTiles_General_SandWaterEdge(u16);
static void QueueAnimTiles_General_Waterfall(u16);
static void QueueAnimTiles_General_LandWaterEdge(u16);
static void QueueAnimTiles_Building_TVTurnedOn(u16);
static void QueueAnimTiles_Rustboro_WindyWater(u16, u8);
static void QueueAnimTiles_Rustboro_Fountain(u16);
static void QueueAnimTiles_Dewford_Flag(u16);
static void QueueAnimTiles_Slateport_Balloons(u16);
static void QueueAnimTiles_Mauville_Flowers(u16, u8);
static void QueueAnimTiles_BikeShop_BlinkingLights(u16);
static void QueueAnimTiles_BattlePyramid_Torch(u16);
static void QueueAnimTiles_BattlePyramid_StatueShadow(u16);
static void BlendAnimPalette_BattleDome_FloorLights(u16);
static void BlendAnimPalette_BattleDome_FloorLightsNoBlend(u16);
static void QueueAnimTiles_Lavaridge_Steam(u8);
static void QueueAnimTiles_Lavaridge_Lava(u16);
static void QueueAnimTiles_EverGrande_Flowers(u16, u8);
static void QueueAnimTiles_Pacifidlog_LogBridges(u8);
static void QueueAnimTiles_Pacifidlog_WaterCurrents(u8);
static void QueueAnimTiles_Sootopolis_StormyWater(u16);
static void QueueAnimTiles_Underwater_Seaweed(u8);
static void QueueAnimTiles_Cave_Lava(u16);
static void QueueAnimTiles_BattleFrontierOutsideWest_Flag(u16);
static void QueueAnimTiles_BattleFrontierOutsideEast_Flag(u16);
static void QueueAnimTiles_MauvilleGym_ElectricGates(u16);
static void QueueAnimTiles_SootopolisGym_Waterfalls(u16);
static void QueueAnimTiles_EliteFour_GroundLights(u16);
static void QueueAnimTiles_EliteFour_WallLights(u16);

const u16 gTilesetAnims_Terrain_Water_Frame0[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Frame1[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Frame2[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water/2.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Frame3[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water/3.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Frame4[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water/4.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Frame5[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water/5.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Frame6[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water/6.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Frame7[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water/7.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Frame8[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water/8.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Frame9[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water/9.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Frame10[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water/10.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Frame11[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water/11.png", ".8bpp", "-palette_mod 16");

const u16 *const gTilesetAnims_Terrain_Water[] = {
    gTilesetAnims_Terrain_Water_Frame0,
    gTilesetAnims_Terrain_Water_Frame1,
    gTilesetAnims_Terrain_Water_Frame2,
    gTilesetAnims_Terrain_Water_Frame3,
    gTilesetAnims_Terrain_Water_Frame4,
    gTilesetAnims_Terrain_Water_Frame5,
    gTilesetAnims_Terrain_Water_Frame6,
    gTilesetAnims_Terrain_Water_Frame7,
    gTilesetAnims_Terrain_Water_Frame8,
    gTilesetAnims_Terrain_Water_Frame9,
    gTilesetAnims_Terrain_Water_Frame10,
    gTilesetAnims_Terrain_Water_Frame11
};

const u16 gTilesetAnims_Terrain_Water_Deep_Frame0[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_deep/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Deep_Frame1[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_deep/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Deep_Frame2[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_deep/2.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Deep_Frame3[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_deep/3.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Deep_Frame4[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_deep/4.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Deep_Frame5[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_deep/5.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Deep_Frame6[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_deep/6.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Deep_Frame7[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_deep/7.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Deep_Frame8[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_deep/8.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Deep_Frame9[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_deep/9.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Deep_Frame10[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_deep/10.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Deep_Frame11[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_deep/11.png", ".8bpp", "-palette_mod 16");

const u16 *const gTilesetAnims_Terrain_Water_Deep[] = {
    gTilesetAnims_Terrain_Water_Deep_Frame0,
    gTilesetAnims_Terrain_Water_Deep_Frame1,
    gTilesetAnims_Terrain_Water_Deep_Frame2,
    gTilesetAnims_Terrain_Water_Deep_Frame3,
    gTilesetAnims_Terrain_Water_Deep_Frame4,
    gTilesetAnims_Terrain_Water_Deep_Frame5,
    gTilesetAnims_Terrain_Water_Deep_Frame6,
    gTilesetAnims_Terrain_Water_Deep_Frame7,
    gTilesetAnims_Terrain_Water_Deep_Frame8,
    gTilesetAnims_Terrain_Water_Deep_Frame9,
    gTilesetAnims_Terrain_Water_Deep_Frame10,
    gTilesetAnims_Terrain_Water_Deep_Frame11
};

const u16 gTilesetAnims_Terrain_Water_Deep_Edge_Frame0[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_deep_edge/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Deep_Edge_Frame1[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_deep_edge/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Deep_Edge_Frame2[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_deep_edge/2.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Deep_Edge_Frame3[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_deep_edge/3.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Deep_Edge_Frame4[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_deep_edge/4.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Deep_Edge_Frame5[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_deep_edge/5.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Deep_Edge_Frame6[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_deep_edge/6.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Deep_Edge_Frame7[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_deep_edge/7.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Deep_Edge_Frame8[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_deep_edge/8.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Deep_Edge_Frame9[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_deep_edge/9.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Deep_Edge_Frame10[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_deep_edge/10.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Deep_Edge_Frame11[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_deep_edge/11.png", ".8bpp", "-palette_mod 16");

const u16 *const gTilesetAnims_Terrain_Water_Deep_Edge[] = {
    gTilesetAnims_Terrain_Water_Deep_Edge_Frame0,
    gTilesetAnims_Terrain_Water_Deep_Edge_Frame1,
    gTilesetAnims_Terrain_Water_Deep_Edge_Frame2,
    gTilesetAnims_Terrain_Water_Deep_Edge_Frame3,
    gTilesetAnims_Terrain_Water_Deep_Edge_Frame4,
    gTilesetAnims_Terrain_Water_Deep_Edge_Frame5,
    gTilesetAnims_Terrain_Water_Deep_Edge_Frame6,
    gTilesetAnims_Terrain_Water_Deep_Edge_Frame7,
    gTilesetAnims_Terrain_Water_Deep_Edge_Frame8,
    gTilesetAnims_Terrain_Water_Deep_Edge_Frame9,
    gTilesetAnims_Terrain_Water_Deep_Edge_Frame10,
    gTilesetAnims_Terrain_Water_Deep_Edge_Frame11
};

const u16 gTilesetAnims_Terrain_Water_Shallow_Frame0[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_shallow/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Shallow_Frame1[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_shallow/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Shallow_Frame2[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_shallow/2.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Shallow_Frame3[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_shallow/3.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Shallow_Frame4[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_shallow/4.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Shallow_Frame5[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_shallow/5.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Shallow_Frame6[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_shallow/6.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Shallow_Frame7[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_shallow/7.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Shallow_Frame8[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_shallow/8.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Shallow_Frame9[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_shallow/9.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Shallow_Frame10[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_shallow/10.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Shallow_Frame11[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_shallow/11.png", ".8bpp", "-palette_mod 16");

const u16 *const gTilesetAnims_Terrain_Water_Shallow[] = {
    gTilesetAnims_Terrain_Water_Shallow_Frame0,
    gTilesetAnims_Terrain_Water_Shallow_Frame1,
    gTilesetAnims_Terrain_Water_Shallow_Frame2,
    gTilesetAnims_Terrain_Water_Shallow_Frame3,
    gTilesetAnims_Terrain_Water_Shallow_Frame4,
    gTilesetAnims_Terrain_Water_Shallow_Frame5,
    gTilesetAnims_Terrain_Water_Shallow_Frame6,
    gTilesetAnims_Terrain_Water_Shallow_Frame7,
    gTilesetAnims_Terrain_Water_Shallow_Frame8,
    gTilesetAnims_Terrain_Water_Shallow_Frame9,
    gTilesetAnims_Terrain_Water_Shallow_Frame10,
    gTilesetAnims_Terrain_Water_Shallow_Frame11
};

const u16 gTilesetAnims_Terrain_Water_Shallow_Edge_Frame0[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_shallow_edge/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Shallow_Edge_Frame1[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_shallow_edge/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Shallow_Edge_Frame2[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_shallow_edge/2.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Shallow_Edge_Frame3[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_shallow_edge/3.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Shallow_Edge_Frame4[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_shallow_edge/4.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Shallow_Edge_Frame5[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_shallow_edge/5.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Shallow_Edge_Frame6[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_shallow_edge/6.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Shallow_Edge_Frame7[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_shallow_edge/7.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Shallow_Edge_Frame8[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_shallow_edge/8.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Shallow_Edge_Frame9[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_shallow_edge/9.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Shallow_Edge_Frame10[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_shallow_edge/10.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Shallow_Edge_Frame11[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_shallow_edge/11.png", ".8bpp", "-palette_mod 16");

const u16 *const gTilesetAnims_Terrain_Water_Shallow_Edge[] = {
    gTilesetAnims_Terrain_Water_Shallow_Edge_Frame0,
    gTilesetAnims_Terrain_Water_Shallow_Edge_Frame1,
    gTilesetAnims_Terrain_Water_Shallow_Edge_Frame2,
    gTilesetAnims_Terrain_Water_Shallow_Edge_Frame3,
    gTilesetAnims_Terrain_Water_Shallow_Edge_Frame4,
    gTilesetAnims_Terrain_Water_Shallow_Edge_Frame5,
    gTilesetAnims_Terrain_Water_Shallow_Edge_Frame6,
    gTilesetAnims_Terrain_Water_Shallow_Edge_Frame7,
    gTilesetAnims_Terrain_Water_Shallow_Edge_Frame8,
    gTilesetAnims_Terrain_Water_Shallow_Edge_Frame9,
    gTilesetAnims_Terrain_Water_Shallow_Edge_Frame10,
    gTilesetAnims_Terrain_Water_Shallow_Edge_Frame11
};

const u16 gTilesetAnims_Terrain_Water_Land_Border_Frame0[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_land_border/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Land_Border_Frame1[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_land_border/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Land_Border_Frame2[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_land_border/2.png", ".8bpp", "-palette_mod 16");

// Ping-pong: 0 -> 1 -> 2 -> 1 -> (0) ...
const u16 *const gTilesetAnims_Terrain_Water_Land_Border[] = {
    gTilesetAnims_Terrain_Water_Land_Border_Frame0,
    gTilesetAnims_Terrain_Water_Land_Border_Frame1,
    gTilesetAnims_Terrain_Water_Land_Border_Frame2,
    gTilesetAnims_Terrain_Water_Land_Border_Frame1
};

// Water currents: 12-frame cycle, 2x2 tiles each. Phased (not override): the currents share subtiles with
// the phased waves, so their slots get banked - a banked slot copies its pre-built frame and ignores
// override writes, which would freeze a QueueAnimTiles approach. As a phased group the frames bake into
// the banks. Position-dependent phase (PhaseFn_Terrain_Water) so they ripple across the map with the wind
// like the water surfaces.
const u16 gTilesetAnims_Terrain_Water_Currents_Frame0[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_currents/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Currents_Frame1[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_currents/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Currents_Frame2[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_currents/2.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Currents_Frame3[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_currents/3.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Currents_Frame4[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_currents/4.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Currents_Frame5[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_currents/5.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Currents_Frame6[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_currents/6.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Currents_Frame7[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_currents/7.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Currents_Frame8[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_currents/8.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Currents_Frame9[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_currents/9.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Currents_Frame10[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_currents/10.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Currents_Frame11[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_currents/11.png", ".8bpp", "-palette_mod 16");

const u16 *const gTilesetAnims_Terrain_Water_Currents[] = {
    gTilesetAnims_Terrain_Water_Currents_Frame0,
    gTilesetAnims_Terrain_Water_Currents_Frame1,
    gTilesetAnims_Terrain_Water_Currents_Frame2,
    gTilesetAnims_Terrain_Water_Currents_Frame3,
    gTilesetAnims_Terrain_Water_Currents_Frame4,
    gTilesetAnims_Terrain_Water_Currents_Frame5,
    gTilesetAnims_Terrain_Water_Currents_Frame6,
    gTilesetAnims_Terrain_Water_Currents_Frame7,
    gTilesetAnims_Terrain_Water_Currents_Frame8,
    gTilesetAnims_Terrain_Water_Currents_Frame9,
    gTilesetAnims_Terrain_Water_Currents_Frame10,
    gTilesetAnims_Terrain_Water_Currents_Frame11
};

// Water currents (side): same format/handling as water_currents (see above), a separate 12-frame 2x2 set.
const u16 gTilesetAnims_Terrain_Water_Currents_Side_Frame0[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_currents_side/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Currents_Side_Frame1[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_currents_side/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Currents_Side_Frame2[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_currents_side/2.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Currents_Side_Frame3[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_currents_side/3.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Currents_Side_Frame4[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_currents_side/4.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Currents_Side_Frame5[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_currents_side/5.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Currents_Side_Frame6[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_currents_side/6.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Currents_Side_Frame7[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_currents_side/7.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Currents_Side_Frame8[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_currents_side/8.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Currents_Side_Frame9[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_currents_side/9.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Currents_Side_Frame10[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_currents_side/10.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Currents_Side_Frame11[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_currents_side/11.png", ".8bpp", "-palette_mod 16");

const u16 *const gTilesetAnims_Terrain_Water_Currents_Side[] = {
    gTilesetAnims_Terrain_Water_Currents_Side_Frame0,
    gTilesetAnims_Terrain_Water_Currents_Side_Frame1,
    gTilesetAnims_Terrain_Water_Currents_Side_Frame2,
    gTilesetAnims_Terrain_Water_Currents_Side_Frame3,
    gTilesetAnims_Terrain_Water_Currents_Side_Frame4,
    gTilesetAnims_Terrain_Water_Currents_Side_Frame5,
    gTilesetAnims_Terrain_Water_Currents_Side_Frame6,
    gTilesetAnims_Terrain_Water_Currents_Side_Frame7,
    gTilesetAnims_Terrain_Water_Currents_Side_Frame8,
    gTilesetAnims_Terrain_Water_Currents_Side_Frame9,
    gTilesetAnims_Terrain_Water_Currents_Side_Frame10,
    gTilesetAnims_Terrain_Water_Currents_Side_Frame11
};

// Two source wave sets: the cardinal crest flows N->S (down), the diagonal crest flows NE->SW (down-left).
// Every wind heading reuses one of these, mirrored/rotated to point the right way (see BuildWaveDirFrames).
const u16 gTilesetAnims_Terrain_Water_Waves_Frame0[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_waves/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Waves_Frame1[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_waves/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Waves_Frame2[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_waves/2.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Waves_Frame3[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_waves/3.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Waves_Frame4[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_waves/4.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Waves_Frame5[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_waves/5.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Waves_Frame6[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_waves/6.png", ".8bpp", "-palette_mod 16");

const u16 gTilesetAnims_Terrain_Water_Waves_Diag_Frame0[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_waves_diag/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Waves_Diag_Frame1[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_waves_diag/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Waves_Diag_Frame2[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_waves_diag/2.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Waves_Diag_Frame3[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_waves_diag/3.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Waves_Diag_Frame4[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_waves_diag/4.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Waves_Diag_Frame5[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_waves_diag/5.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Terrain_Water_Waves_Diag_Frame6[] = INCGFX_U16("data/tilesets/primary/terrain/anim/water_waves_diag/6.png", ".8bpp", "-palette_mod 16");

static const u16 *const sWaveBaseCardinal[6] = {
    gTilesetAnims_Terrain_Water_Waves_Frame0, gTilesetAnims_Terrain_Water_Waves_Frame1,
    gTilesetAnims_Terrain_Water_Waves_Frame3, gTilesetAnims_Terrain_Water_Waves_Frame4,
    gTilesetAnims_Terrain_Water_Waves_Frame5, gTilesetAnims_Terrain_Water_Waves_Frame6,
};

static const u16 *const sWaveBaseCardinalRough[6] = {
    gTilesetAnims_Terrain_Water_Waves_Frame0, gTilesetAnims_Terrain_Water_Waves_Frame1,
    gTilesetAnims_Terrain_Water_Waves_Frame2, gTilesetAnims_Terrain_Water_Waves_Frame4,
    gTilesetAnims_Terrain_Water_Waves_Frame5, gTilesetAnims_Terrain_Water_Waves_Frame6,
};

static const u16 *const sWaveBaseDiag[6] = {
    gTilesetAnims_Terrain_Water_Waves_Diag_Frame0, gTilesetAnims_Terrain_Water_Waves_Diag_Frame1,
    gTilesetAnims_Terrain_Water_Waves_Diag_Frame3, gTilesetAnims_Terrain_Water_Waves_Diag_Frame4,
    gTilesetAnims_Terrain_Water_Waves_Diag_Frame5, gTilesetAnims_Terrain_Water_Waves_Diag_Frame6,
};

static const u16 *const sWaveBaseDiagRough[6] = {
    gTilesetAnims_Terrain_Water_Waves_Diag_Frame0, gTilesetAnims_Terrain_Water_Waves_Diag_Frame1,
    gTilesetAnims_Terrain_Water_Waves_Diag_Frame2, gTilesetAnims_Terrain_Water_Waves_Diag_Frame4,
    gTilesetAnims_Terrain_Water_Waves_Diag_Frame5, gTilesetAnims_Terrain_Water_Waves_Diag_Frame6,
};

const u16 gTilesetAnims_General_Flower_Frame1[] = INCGFX_U16("data/tilesets/primary/general/anim/flower/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_General_Flower_Frame0[] = INCGFX_U16("data/tilesets/primary/general/anim/flower/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_General_Flower_Frame2[] = INCGFX_U16("data/tilesets/primary/general/anim/flower/2.png", ".8bpp", "-palette_mod 16");
const u16 tileset_anims_space_0[16] = {};

const u16 *const gTilesetAnims_General_Flower[] = {
    gTilesetAnims_General_Flower_Frame0,
    gTilesetAnims_General_Flower_Frame1,
    gTilesetAnims_General_Flower_Frame0,
    gTilesetAnims_General_Flower_Frame2
};

const u16 gTilesetAnims_General_Water_Frame0[] = INCGFX_U16("data/tilesets/primary/general/anim/water/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_General_Water_Frame1[] = INCGFX_U16("data/tilesets/primary/general/anim/water/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_General_Water_Frame2[] = INCGFX_U16("data/tilesets/primary/general/anim/water/2.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_General_Water_Frame3[] = INCGFX_U16("data/tilesets/primary/general/anim/water/3.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_General_Water_Frame4[] = INCGFX_U16("data/tilesets/primary/general/anim/water/4.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_General_Water_Frame5[] = INCGFX_U16("data/tilesets/primary/general/anim/water/5.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_General_Water_Frame6[] = INCGFX_U16("data/tilesets/primary/general/anim/water/6.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_General_Water_Frame7[] = INCGFX_U16("data/tilesets/primary/general/anim/water/7.png", ".8bpp", "-palette_mod 16");

const u16 *const gTilesetAnims_General_Water[] = {
    gTilesetAnims_General_Water_Frame0,
    gTilesetAnims_General_Water_Frame1,
    gTilesetAnims_General_Water_Frame2,
    gTilesetAnims_General_Water_Frame3,
    gTilesetAnims_General_Water_Frame4,
    gTilesetAnims_General_Water_Frame5,
    gTilesetAnims_General_Water_Frame6,
    gTilesetAnims_General_Water_Frame7
};

const u16 gTilesetAnims_General_SandWaterEdge_Frame0[] = INCGFX_U16("data/tilesets/primary/general/anim/sand_water_edge/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_General_SandWaterEdge_Frame1[] = INCGFX_U16("data/tilesets/primary/general/anim/sand_water_edge/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_General_SandWaterEdge_Frame2[] = INCGFX_U16("data/tilesets/primary/general/anim/sand_water_edge/2.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_General_SandWaterEdge_Frame3[] = INCGFX_U16("data/tilesets/primary/general/anim/sand_water_edge/3.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_General_SandWaterEdge_Frame4[] = INCGFX_U16("data/tilesets/primary/general/anim/sand_water_edge/4.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_General_SandWaterEdge_Frame5[] = INCGFX_U16("data/tilesets/primary/general/anim/sand_water_edge/5.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_General_SandWaterEdge_Frame6[] = INCGFX_U16("data/tilesets/primary/general/anim/sand_water_edge/6.png", ".8bpp", "-palette_mod 16");

const u16 *const gTilesetAnims_General_SandWaterEdge[] = {
    gTilesetAnims_General_SandWaterEdge_Frame0,
    gTilesetAnims_General_SandWaterEdge_Frame1,
    gTilesetAnims_General_SandWaterEdge_Frame2,
    gTilesetAnims_General_SandWaterEdge_Frame3,
    gTilesetAnims_General_SandWaterEdge_Frame4,
    gTilesetAnims_General_SandWaterEdge_Frame5,
    gTilesetAnims_General_SandWaterEdge_Frame6,
    gTilesetAnims_General_SandWaterEdge_Frame0
};

const u16 gTilesetAnims_General_Waterfall_Frame0[] = INCGFX_U16("data/tilesets/primary/general/anim/waterfall/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_General_Waterfall_Frame1[] = INCGFX_U16("data/tilesets/primary/general/anim/waterfall/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_General_Waterfall_Frame2[] = INCGFX_U16("data/tilesets/primary/general/anim/waterfall/2.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_General_Waterfall_Frame3[] = INCGFX_U16("data/tilesets/primary/general/anim/waterfall/3.png", ".8bpp", "-palette_mod 16");

const u16 *const gTilesetAnims_General_Waterfall[] = {
    gTilesetAnims_General_Waterfall_Frame0,
    gTilesetAnims_General_Waterfall_Frame1,
    gTilesetAnims_General_Waterfall_Frame2,
    gTilesetAnims_General_Waterfall_Frame3
};

const u16 gTilesetAnims_General_LandWaterEdge_Frame0[] = INCGFX_U16("data/tilesets/primary/general/anim/land_water_edge/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_General_LandWaterEdge_Frame1[] = INCGFX_U16("data/tilesets/primary/general/anim/land_water_edge/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_General_LandWaterEdge_Frame2[] = INCGFX_U16("data/tilesets/primary/general/anim/land_water_edge/2.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_General_LandWaterEdge_Frame3[] = INCGFX_U16("data/tilesets/primary/general/anim/land_water_edge/3.png", ".8bpp", "-palette_mod 16");

const u16 *const gTilesetAnims_General_LandWaterEdge[] = {
    gTilesetAnims_General_LandWaterEdge_Frame0,
    gTilesetAnims_General_LandWaterEdge_Frame1,
    gTilesetAnims_General_LandWaterEdge_Frame2,
    gTilesetAnims_General_LandWaterEdge_Frame3
};

const u16 gTilesetAnims_Lavaridge_Steam_Frame0[] = INCGFX_U16("data/tilesets/secondary/lavaridge/anim/steam/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Lavaridge_Steam_Frame1[] = INCGFX_U16("data/tilesets/secondary/lavaridge/anim/steam/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Lavaridge_Steam_Frame2[] = INCGFX_U16("data/tilesets/secondary/lavaridge/anim/steam/2.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Lavaridge_Steam_Frame3[] = INCGFX_U16("data/tilesets/secondary/lavaridge/anim/steam/3.png", ".8bpp", "-palette_mod 16");

const u16 *const gTilesetAnims_Lavaridge_Steam[] = {
    gTilesetAnims_Lavaridge_Steam_Frame0,
    gTilesetAnims_Lavaridge_Steam_Frame1,
    gTilesetAnims_Lavaridge_Steam_Frame2,
    gTilesetAnims_Lavaridge_Steam_Frame3
};

const u16 gTilesetAnims_Pacifidlog_LogBridges_Frame0[] = INCGFX_U16("data/tilesets/secondary/pacifidlog/anim/log_bridges/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Pacifidlog_LogBridges_Frame1[] = INCGFX_U16("data/tilesets/secondary/pacifidlog/anim/log_bridges/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Pacifidlog_LogBridges_Frame2[] = INCGFX_U16("data/tilesets/secondary/pacifidlog/anim/log_bridges/2.png", ".8bpp", "-palette_mod 16");

const u16 *const gTilesetAnims_Pacifidlog_LogBridges[] = {
    gTilesetAnims_Pacifidlog_LogBridges_Frame0,
    gTilesetAnims_Pacifidlog_LogBridges_Frame1,
    gTilesetAnims_Pacifidlog_LogBridges_Frame2,
    gTilesetAnims_Pacifidlog_LogBridges_Frame1
};

const u16 gTilesetAnims_Underwater_Seaweed_Frame0[] = INCGFX_U16("data/tilesets/secondary/underwater/anim/seaweed/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Underwater_Seaweed_Frame1[] = INCGFX_U16("data/tilesets/secondary/underwater/anim/seaweed/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Underwater_Seaweed_Frame2[] = INCGFX_U16("data/tilesets/secondary/underwater/anim/seaweed/2.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Underwater_Seaweed_Frame3[] = INCGFX_U16("data/tilesets/secondary/underwater/anim/seaweed/3.png", ".8bpp", "-palette_mod 16");

const u16 *const gTilesetAnims_Underwater_Seaweed[] = {
    gTilesetAnims_Underwater_Seaweed_Frame0,
    gTilesetAnims_Underwater_Seaweed_Frame1,
    gTilesetAnims_Underwater_Seaweed_Frame2,
    gTilesetAnims_Underwater_Seaweed_Frame3
};

const u16 gTilesetAnims_Pacifidlog_WaterCurrents_Frame0[] = INCGFX_U16("data/tilesets/secondary/pacifidlog/anim/water_currents/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Pacifidlog_WaterCurrents_Frame1[] = INCGFX_U16("data/tilesets/secondary/pacifidlog/anim/water_currents/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Pacifidlog_WaterCurrents_Frame2[] = INCGFX_U16("data/tilesets/secondary/pacifidlog/anim/water_currents/2.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Pacifidlog_WaterCurrents_Frame3[] = INCGFX_U16("data/tilesets/secondary/pacifidlog/anim/water_currents/3.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Pacifidlog_WaterCurrents_Frame4[] = INCGFX_U16("data/tilesets/secondary/pacifidlog/anim/water_currents/4.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Pacifidlog_WaterCurrents_Frame5[] = INCGFX_U16("data/tilesets/secondary/pacifidlog/anim/water_currents/5.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Pacifidlog_WaterCurrents_Frame6[] = INCGFX_U16("data/tilesets/secondary/pacifidlog/anim/water_currents/6.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Pacifidlog_WaterCurrents_Frame7[] = INCGFX_U16("data/tilesets/secondary/pacifidlog/anim/water_currents/7.png", ".8bpp", "-palette_mod 16");

const u16 *const gTilesetAnims_Pacifidlog_WaterCurrents[] = {
    gTilesetAnims_Pacifidlog_WaterCurrents_Frame0,
    gTilesetAnims_Pacifidlog_WaterCurrents_Frame1,
    gTilesetAnims_Pacifidlog_WaterCurrents_Frame2,
    gTilesetAnims_Pacifidlog_WaterCurrents_Frame3,
    gTilesetAnims_Pacifidlog_WaterCurrents_Frame4,
    gTilesetAnims_Pacifidlog_WaterCurrents_Frame5,
    gTilesetAnims_Pacifidlog_WaterCurrents_Frame6,
    gTilesetAnims_Pacifidlog_WaterCurrents_Frame7
};

const u16 gTilesetAnims_Mauville_Flower1_Frame0[] = INCGFX_U16("data/tilesets/secondary/mauville/anim/flower_1/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Mauville_Flower1_Frame1[] = INCGFX_U16("data/tilesets/secondary/mauville/anim/flower_1/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Mauville_Flower1_Frame2[] = INCGFX_U16("data/tilesets/secondary/mauville/anim/flower_1/2.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Mauville_Flower1_Frame3[] = INCGFX_U16("data/tilesets/secondary/mauville/anim/flower_1/3.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Mauville_Flower1_Frame4[] = INCGFX_U16("data/tilesets/secondary/mauville/anim/flower_1/4.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Mauville_Flower2_Frame0[] = INCGFX_U16("data/tilesets/secondary/mauville/anim/flower_2/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Mauville_Flower2_Frame1[] = INCGFX_U16("data/tilesets/secondary/mauville/anim/flower_2/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Mauville_Flower2_Frame2[] = INCGFX_U16("data/tilesets/secondary/mauville/anim/flower_2/2.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Mauville_Flower2_Frame3[] = INCGFX_U16("data/tilesets/secondary/mauville/anim/flower_2/3.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Mauville_Flower2_Frame4[] = INCGFX_U16("data/tilesets/secondary/mauville/anim/flower_2/4.png", ".8bpp", "-palette_mod 16");
const u16 tileset_anims_space_1[16] = {};

u16 *const gTilesetAnims_Mauville_Flower1_VDests[] = {
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 96)),
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 100)),
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 104)),
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 108)),
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 112)),
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 116)),
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 120)),
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 124))
};

u16 *const gTilesetAnims_Mauville_Flower2_VDests[] = {
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 128)),
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 132)),
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 136)),
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 140)),
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 144)),
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 148)),
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 152)),
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 156))
};

const u16 *const gTilesetAnims_Mauville_Flower1[] = {
    gTilesetAnims_Mauville_Flower1_Frame0,
    gTilesetAnims_Mauville_Flower1_Frame0,
    gTilesetAnims_Mauville_Flower1_Frame1,
    gTilesetAnims_Mauville_Flower1_Frame2,
    gTilesetAnims_Mauville_Flower1_Frame3,
    gTilesetAnims_Mauville_Flower1_Frame3,
    gTilesetAnims_Mauville_Flower1_Frame3,
    gTilesetAnims_Mauville_Flower1_Frame3,
    gTilesetAnims_Mauville_Flower1_Frame3,
    gTilesetAnims_Mauville_Flower1_Frame3,
    gTilesetAnims_Mauville_Flower1_Frame2,
    gTilesetAnims_Mauville_Flower1_Frame1
};

const u16 *const gTilesetAnims_Mauville_Flower2[] = {
    gTilesetAnims_Mauville_Flower2_Frame0,
    gTilesetAnims_Mauville_Flower2_Frame0,
    gTilesetAnims_Mauville_Flower2_Frame1,
    gTilesetAnims_Mauville_Flower2_Frame2,
    gTilesetAnims_Mauville_Flower2_Frame3,
    gTilesetAnims_Mauville_Flower2_Frame3,
    gTilesetAnims_Mauville_Flower2_Frame3,
    gTilesetAnims_Mauville_Flower2_Frame3,
    gTilesetAnims_Mauville_Flower2_Frame3,
    gTilesetAnims_Mauville_Flower2_Frame3,
    gTilesetAnims_Mauville_Flower2_Frame2,
    gTilesetAnims_Mauville_Flower2_Frame1
};

const u16 *const gTilesetAnims_Mauville_Flower1_B[] = {
    gTilesetAnims_Mauville_Flower1_Frame0,
    gTilesetAnims_Mauville_Flower1_Frame0,
    gTilesetAnims_Mauville_Flower1_Frame4,
    gTilesetAnims_Mauville_Flower1_Frame4
};

const u16 *const gTilesetAnims_Mauville_Flower2_B[] = {
    gTilesetAnims_Mauville_Flower2_Frame0,
    gTilesetAnims_Mauville_Flower2_Frame0,
    gTilesetAnims_Mauville_Flower2_Frame4,
    gTilesetAnims_Mauville_Flower2_Frame4
};

const u16 gTilesetAnims_Rustboro_WindyWater_Frame0[] = INCGFX_U16("data/tilesets/secondary/rustboro/anim/windy_water/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Rustboro_WindyWater_Frame1[] = INCGFX_U16("data/tilesets/secondary/rustboro/anim/windy_water/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Rustboro_WindyWater_Frame2[] = INCGFX_U16("data/tilesets/secondary/rustboro/anim/windy_water/2.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Rustboro_WindyWater_Frame3[] = INCGFX_U16("data/tilesets/secondary/rustboro/anim/windy_water/3.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Rustboro_WindyWater_Frame4[] = INCGFX_U16("data/tilesets/secondary/rustboro/anim/windy_water/4.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Rustboro_WindyWater_Frame5[] = INCGFX_U16("data/tilesets/secondary/rustboro/anim/windy_water/5.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Rustboro_WindyWater_Frame6[] = INCGFX_U16("data/tilesets/secondary/rustboro/anim/windy_water/6.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Rustboro_WindyWater_Frame7[] = INCGFX_U16("data/tilesets/secondary/rustboro/anim/windy_water/7.png", ".8bpp", "-palette_mod 16");

u16 *const gTilesetAnims_Rustboro_WindyWater_VDests[] = {
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 128)),
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 132)),
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 136)),
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 140)),
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 144)),
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 148)),
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 152)),
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 156))
};

const u16 *const gTilesetAnims_Rustboro_WindyWater[] = {
    gTilesetAnims_Rustboro_WindyWater_Frame0,
    gTilesetAnims_Rustboro_WindyWater_Frame1,
    gTilesetAnims_Rustboro_WindyWater_Frame2,
    gTilesetAnims_Rustboro_WindyWater_Frame3,
    gTilesetAnims_Rustboro_WindyWater_Frame4,
    gTilesetAnims_Rustboro_WindyWater_Frame5,
    gTilesetAnims_Rustboro_WindyWater_Frame6,
    gTilesetAnims_Rustboro_WindyWater_Frame7
};

const u16 gTilesetAnims_Rustboro_Fountain_Frame0[] = INCGFX_U16("data/tilesets/secondary/rustboro/anim/fountain/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Rustboro_Fountain_Frame1[] = INCGFX_U16("data/tilesets/secondary/rustboro/anim/fountain/1.png", ".8bpp", "-palette_mod 16");
const u16 tileset_anims_space_2[16] = {};

const u16 *const gTilesetAnims_Rustboro_Fountain[] = {
    gTilesetAnims_Rustboro_Fountain_Frame0,
    gTilesetAnims_Rustboro_Fountain_Frame1
};

const u16 gTilesetAnims_Lavaridge_Cave_Lava_Frame0[] = INCGFX_U16("data/tilesets/secondary/cave/anim/lava/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Lavaridge_Cave_Lava_Frame1[] = INCGFX_U16("data/tilesets/secondary/cave/anim/lava/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Lavaridge_Cave_Lava_Frame2[] = INCGFX_U16("data/tilesets/secondary/cave/anim/lava/2.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Lavaridge_Cave_Lava_Frame3[] = INCGFX_U16("data/tilesets/secondary/cave/anim/lava/3.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Lavaridge_Cave_Lava_Frame4[] = INCGFX_U16("data/tilesets/secondary/cave/anim/lava/4.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Lavaridge_Cave_Lava_Frame5[] = INCGFX_U16("data/tilesets/secondary/cave/anim/lava/5.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Lavaridge_Cave_Lava_Frame6[] = INCGFX_U16("data/tilesets/secondary/cave/anim/lava/6.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Lavaridge_Cave_Lava_Frame7[] = INCGFX_U16("data/tilesets/secondary/cave/anim/lava/7.png", ".8bpp", "-palette_mod 16");
const u16 tileset_anims_space_3[16] = {};

const u16 *const gTilesetAnims_Lavaridge_Cave_Lava[] = {
    gTilesetAnims_Lavaridge_Cave_Lava_Frame0,
    gTilesetAnims_Lavaridge_Cave_Lava_Frame1,
    gTilesetAnims_Lavaridge_Cave_Lava_Frame2,
    gTilesetAnims_Lavaridge_Cave_Lava_Frame3
};

const u16 gTilesetAnims_EverGrande_Flowers_Frame0[] = INCGFX_U16("data/tilesets/secondary/ever_grande/anim/flowers/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_EverGrande_Flowers_Frame1[] = INCGFX_U16("data/tilesets/secondary/ever_grande/anim/flowers/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_EverGrande_Flowers_Frame2[] = INCGFX_U16("data/tilesets/secondary/ever_grande/anim/flowers/2.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_EverGrande_Flowers_Frame3[] = INCGFX_U16("data/tilesets/secondary/ever_grande/anim/flowers/3.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_EverGrande_Flowers_Frame4[] = INCGFX_U16("data/tilesets/secondary/ever_grande/anim/flowers/4.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_EverGrande_Flowers_Frame5[] = INCGFX_U16("data/tilesets/secondary/ever_grande/anim/flowers/5.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_EverGrande_Flowers_Frame6[] = INCGFX_U16("data/tilesets/secondary/ever_grande/anim/flowers/6.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_EverGrande_Flowers_Frame7[] = INCGFX_U16("data/tilesets/secondary/ever_grande/anim/flowers/7.png", ".8bpp", "-palette_mod 16");
const u16 tileset_anims_space_4[16] = {};

u16 *const gTilesetAnims_EverGrande_VDests[] = {
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 224)),
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 228)),
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 232)),
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 236)),
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 240)),
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 244)),
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 248)),
    (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 252))
};

const u16 *const gTilesetAnims_EverGrande_Flowers[] = {
    gTilesetAnims_EverGrande_Flowers_Frame0,
    gTilesetAnims_EverGrande_Flowers_Frame1,
    gTilesetAnims_EverGrande_Flowers_Frame2,
    gTilesetAnims_EverGrande_Flowers_Frame3,
    gTilesetAnims_EverGrande_Flowers_Frame4,
    gTilesetAnims_EverGrande_Flowers_Frame5,
    gTilesetAnims_EverGrande_Flowers_Frame6,
    gTilesetAnims_EverGrande_Flowers_Frame7
};

const u16 gTilesetAnims_Dewford_Flag_Frame0[] = INCGFX_U16("data/tilesets/secondary/dewford/anim/flag/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Dewford_Flag_Frame1[] = INCGFX_U16("data/tilesets/secondary/dewford/anim/flag/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Dewford_Flag_Frame2[] = INCGFX_U16("data/tilesets/secondary/dewford/anim/flag/2.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Dewford_Flag_Frame3[] = INCGFX_U16("data/tilesets/secondary/dewford/anim/flag/3.png", ".8bpp", "-palette_mod 16");

const u16 *const gTilesetAnims_Dewford_Flag[] = {
    gTilesetAnims_Dewford_Flag_Frame0,
    gTilesetAnims_Dewford_Flag_Frame1,
    gTilesetAnims_Dewford_Flag_Frame2,
    gTilesetAnims_Dewford_Flag_Frame3
};

const u16 gTilesetAnims_BattleFrontierOutsideWest_Flag_Frame0[] = INCGFX_U16("data/tilesets/secondary/battle_frontier_outside_west/anim/flag/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_BattleFrontierOutsideWest_Flag_Frame1[] = INCGFX_U16("data/tilesets/secondary/battle_frontier_outside_west/anim/flag/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_BattleFrontierOutsideWest_Flag_Frame2[] = INCGFX_U16("data/tilesets/secondary/battle_frontier_outside_west/anim/flag/2.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_BattleFrontierOutsideWest_Flag_Frame3[] = INCGFX_U16("data/tilesets/secondary/battle_frontier_outside_west/anim/flag/3.png", ".8bpp", "-palette_mod 16");

const u16 *const gTilesetAnims_BattleFrontierOutsideWest_Flag[] = {
    gTilesetAnims_BattleFrontierOutsideWest_Flag_Frame0,
    gTilesetAnims_BattleFrontierOutsideWest_Flag_Frame1,
    gTilesetAnims_BattleFrontierOutsideWest_Flag_Frame2,
    gTilesetAnims_BattleFrontierOutsideWest_Flag_Frame3
};

const u16 gTilesetAnims_BattleFrontierOutsideEast_Flag_Frame0[] = INCGFX_U16("data/tilesets/secondary/battle_frontier_outside_east/anim/flag/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_BattleFrontierOutsideEast_Flag_Frame1[] = INCGFX_U16("data/tilesets/secondary/battle_frontier_outside_east/anim/flag/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_BattleFrontierOutsideEast_Flag_Frame2[] = INCGFX_U16("data/tilesets/secondary/battle_frontier_outside_east/anim/flag/2.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_BattleFrontierOutsideEast_Flag_Frame3[] = INCGFX_U16("data/tilesets/secondary/battle_frontier_outside_east/anim/flag/3.png", ".8bpp", "-palette_mod 16");

const u16 *const gTilesetAnims_BattleFrontierOutsideEast_Flag[] = {
    gTilesetAnims_BattleFrontierOutsideEast_Flag_Frame0,
    gTilesetAnims_BattleFrontierOutsideEast_Flag_Frame1,
    gTilesetAnims_BattleFrontierOutsideEast_Flag_Frame2,
    gTilesetAnims_BattleFrontierOutsideEast_Flag_Frame3
};

const u16 gTilesetAnims_Slateport_Balloons_Frame0[] = INCGFX_U16("data/tilesets/secondary/slateport/anim/balloons/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Slateport_Balloons_Frame1[] = INCGFX_U16("data/tilesets/secondary/slateport/anim/balloons/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Slateport_Balloons_Frame2[] = INCGFX_U16("data/tilesets/secondary/slateport/anim/balloons/2.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Slateport_Balloons_Frame3[] = INCGFX_U16("data/tilesets/secondary/slateport/anim/balloons/3.png", ".8bpp", "-palette_mod 16");

const u16 *const gTilesetAnims_Slateport_Balloons[] = {
    gTilesetAnims_Slateport_Balloons_Frame0,
    gTilesetAnims_Slateport_Balloons_Frame1,
    gTilesetAnims_Slateport_Balloons_Frame2,
    gTilesetAnims_Slateport_Balloons_Frame3
};

const u16 gTilesetAnims_Building_TvTurnedOn_Frame0[] = INCGFX_U16("data/tilesets/primary/building/anim/tv_turned_on/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Building_TvTurnedOn_Frame1[] = INCGFX_U16("data/tilesets/primary/building/anim/tv_turned_on/1.png", ".8bpp", "-palette_mod 16");

const u16 *const gTilesetAnims_Building_TvTurnedOn[] = {
    gTilesetAnims_Building_TvTurnedOn_Frame0,
    gTilesetAnims_Building_TvTurnedOn_Frame1
};

const u16 gTilesetAnims_SootopolisGym_SideWaterfall_Frame0[] = INCGFX_U16("data/tilesets/secondary/sootopolis_gym/anim/side_waterfall/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_SootopolisGym_SideWaterfall_Frame1[] = INCGFX_U16("data/tilesets/secondary/sootopolis_gym/anim/side_waterfall/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_SootopolisGym_SideWaterfall_Frame2[] = INCGFX_U16("data/tilesets/secondary/sootopolis_gym/anim/side_waterfall/2.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_SootopolisGym_FrontWaterfall_Frame0[] = INCGFX_U16("data/tilesets/secondary/sootopolis_gym/anim/front_waterfall/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_SootopolisGym_FrontWaterfall_Frame1[] = INCGFX_U16("data/tilesets/secondary/sootopolis_gym/anim/front_waterfall/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_SootopolisGym_FrontWaterfall_Frame2[] = INCGFX_U16("data/tilesets/secondary/sootopolis_gym/anim/front_waterfall/2.png", ".8bpp", "-palette_mod 16");

const u16 *const gTilesetAnims_SootopolisGym_SideWaterfall[] = {
    gTilesetAnims_SootopolisGym_SideWaterfall_Frame0,
    gTilesetAnims_SootopolisGym_SideWaterfall_Frame1,
    gTilesetAnims_SootopolisGym_SideWaterfall_Frame2
};

const u16 *const gTilesetAnims_SootopolisGym_FrontWaterfall[] = {
    gTilesetAnims_SootopolisGym_FrontWaterfall_Frame0,
    gTilesetAnims_SootopolisGym_FrontWaterfall_Frame1,
    gTilesetAnims_SootopolisGym_FrontWaterfall_Frame2
};

const u16 gTilesetAnims_EliteFour_FloorLight_Frame0[] = INCGFX_U16("data/tilesets/secondary/elite_four/anim/floor_light/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_EliteFour_FloorLight_Frame1[] = INCGFX_U16("data/tilesets/secondary/elite_four/anim/floor_light/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_EliteFour_WallLights_Frame0[] = INCGFX_U16("data/tilesets/secondary/elite_four/anim/wall_lights/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_EliteFour_WallLights_Frame1[] = INCGFX_U16("data/tilesets/secondary/elite_four/anim/wall_lights/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_EliteFour_WallLights_Frame2[] = INCGFX_U16("data/tilesets/secondary/elite_four/anim/wall_lights/2.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_EliteFour_WallLights_Frame3[] = INCGFX_U16("data/tilesets/secondary/elite_four/anim/wall_lights/3.png", ".8bpp", "-palette_mod 16");
const u16 tileset_anims_space_5[16] = {};

const u16 *const gTilesetAnims_EliteFour_WallLights[] = {
    gTilesetAnims_EliteFour_WallLights_Frame0,
    gTilesetAnims_EliteFour_WallLights_Frame1,
    gTilesetAnims_EliteFour_WallLights_Frame2,
    gTilesetAnims_EliteFour_WallLights_Frame3
};

const u16 *const gTilesetAnims_EliteFour_FloorLight[] = {
    gTilesetAnims_EliteFour_FloorLight_Frame0,
    gTilesetAnims_EliteFour_FloorLight_Frame1
};

const u16 gTilesetAnims_MauvilleGym_ElectricGates_Frame0[] = INCGFX_U16("data/tilesets/secondary/mauville_gym/anim/electric_gates/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_MauvilleGym_ElectricGates_Frame1[] = INCGFX_U16("data/tilesets/secondary/mauville_gym/anim/electric_gates/1.png", ".8bpp", "-palette_mod 16");
const u16 tileset_anims_space_6[16] = {};

const u16 *const gTilesetAnims_MauvilleGym_ElectricGates[] = {
    gTilesetAnims_MauvilleGym_ElectricGates_Frame0,
    gTilesetAnims_MauvilleGym_ElectricGates_Frame1
};

const u16 gTilesetAnims_BikeShop_BlinkingLights_Frame0[] = INCGFX_U16("data/tilesets/secondary/bike_shop/anim/blinking_lights/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_BikeShop_BlinkingLights_Frame1[] = INCGFX_U16("data/tilesets/secondary/bike_shop/anim/blinking_lights/1.png", ".8bpp", "-palette_mod 16");
const u16 tileset_anims_space_7[16] = {};

const u16 *const gTilesetAnims_BikeShop_BlinkingLights[] = {
    gTilesetAnims_BikeShop_BlinkingLights_Frame0,
    gTilesetAnims_BikeShop_BlinkingLights_Frame1
};

const u16 gTilesetAnims_Sootopolis_StormyWater_Frame0[] = INCBIN_U16("data/tilesets/secondary/sootopolis/anim/stormy_water/0_kyogre.8bpp", "data/tilesets/secondary/sootopolis/anim/stormy_water/0_groudon.8bpp");
const u16 gTilesetAnims_Sootopolis_StormyWater_Frame1[] = INCBIN_U16("data/tilesets/secondary/sootopolis/anim/stormy_water/1_kyogre.8bpp", "data/tilesets/secondary/sootopolis/anim/stormy_water/1_groudon.8bpp");
const u16 gTilesetAnims_Sootopolis_StormyWater_Frame2[] = INCBIN_U16("data/tilesets/secondary/sootopolis/anim/stormy_water/2_kyogre.8bpp", "data/tilesets/secondary/sootopolis/anim/stormy_water/2_groudon.8bpp");
const u16 gTilesetAnims_Sootopolis_StormyWater_Frame3[] = INCBIN_U16("data/tilesets/secondary/sootopolis/anim/stormy_water/3_kyogre.8bpp", "data/tilesets/secondary/sootopolis/anim/stormy_water/3_groudon.8bpp");
const u16 gTilesetAnims_Sootopolis_StormyWater_Frame4[] = INCBIN_U16("data/tilesets/secondary/sootopolis/anim/stormy_water/4_kyogre.8bpp", "data/tilesets/secondary/sootopolis/anim/stormy_water/4_groudon.8bpp");
const u16 gTilesetAnims_Sootopolis_StormyWater_Frame5[] = INCBIN_U16("data/tilesets/secondary/sootopolis/anim/stormy_water/5_kyogre.8bpp", "data/tilesets/secondary/sootopolis/anim/stormy_water/5_groudon.8bpp");
const u16 gTilesetAnims_Sootopolis_StormyWater_Frame6[] = INCBIN_U16("data/tilesets/secondary/sootopolis/anim/stormy_water/6_kyogre.8bpp", "data/tilesets/secondary/sootopolis/anim/stormy_water/6_groudon.8bpp");
const u16 gTilesetAnims_Sootopolis_StormyWater_Frame7[] = INCBIN_U16("data/tilesets/secondary/sootopolis/anim/stormy_water/7_kyogre.8bpp", "data/tilesets/secondary/sootopolis/anim/stormy_water/7_groudon.8bpp");
const u16 tileset_anims_space_8[16] = {};

const u16 gTilesetAnims_Unused1_Frame0[] = INCGFX_U16("data/tilesets/secondary/unused_1/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Unused1_Frame1[] = INCGFX_U16("data/tilesets/secondary/unused_1/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Unused1_Frame2[] = INCGFX_U16("data/tilesets/secondary/unused_1/2.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_Unused1_Frame3[] = INCGFX_U16("data/tilesets/secondary/unused_1/3.png", ".8bpp", "-palette_mod 16");

const u16 *const gTilesetAnims_Sootopolis_StormyWater[] = {
    gTilesetAnims_Sootopolis_StormyWater_Frame0,
    gTilesetAnims_Sootopolis_StormyWater_Frame1,
    gTilesetAnims_Sootopolis_StormyWater_Frame2,
    gTilesetAnims_Sootopolis_StormyWater_Frame3,
    gTilesetAnims_Sootopolis_StormyWater_Frame4,
    gTilesetAnims_Sootopolis_StormyWater_Frame5,
    gTilesetAnims_Sootopolis_StormyWater_Frame6,
    gTilesetAnims_Sootopolis_StormyWater_Frame7
};

const u16 gTilesetAnims_BattlePyramid_Torch_Frame0[] = INCGFX_U16("data/tilesets/secondary/battle_pyramid/anim/torch/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_BattlePyramid_Torch_Frame1[] = INCGFX_U16("data/tilesets/secondary/battle_pyramid/anim/torch/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_BattlePyramid_Torch_Frame2[] = INCGFX_U16("data/tilesets/secondary/battle_pyramid/anim/torch/2.png", ".8bpp", "-palette_mod 16");
const u16 tileset_anims_space_9[16] = {};

const u16 gTilesetAnims_BattlePyramid_StatueShadow_Frame0[] = INCGFX_U16("data/tilesets/secondary/battle_pyramid/anim/statue_shadow/0.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_BattlePyramid_StatueShadow_Frame1[] = INCGFX_U16("data/tilesets/secondary/battle_pyramid/anim/statue_shadow/1.png", ".8bpp", "-palette_mod 16");
const u16 gTilesetAnims_BattlePyramid_StatueShadow_Frame2[] = INCGFX_U16("data/tilesets/secondary/battle_pyramid/anim/statue_shadow/2.png", ".8bpp", "-palette_mod 16");
const u16 tileset_anims_space_10[7808] = {};

const u16 gTilesetAnims_Unused2_Frame0[] = INCGFX_U16("data/tilesets/secondary/unused_2/0.png", ".8bpp", "-palette_mod 16");
const u16 tileset_anims_space_11[224] = {};

const u16 gTilesetAnims_Unused2_Frame1[] = INCGFX_U16("data/tilesets/secondary/unused_2/1.png", ".8bpp", "-palette_mod 16");

const u16 *const gTilesetAnims_BattlePyramid_Torch[] = {
    gTilesetAnims_BattlePyramid_Torch_Frame0,
    gTilesetAnims_BattlePyramid_Torch_Frame1,
    gTilesetAnims_BattlePyramid_Torch_Frame2
};

const u16 *const gTilesetAnims_BattlePyramid_StatueShadow[] = {
    gTilesetAnims_BattlePyramid_StatueShadow_Frame0,
    gTilesetAnims_BattlePyramid_StatueShadow_Frame1,
    gTilesetAnims_BattlePyramid_StatueShadow_Frame2
};

static const u16 *const sTilesetAnims_BattleDomeFloorLightPals[] = {
    gTilesetAnims_BattleDomePals0_0,
    gTilesetAnims_BattleDomePals0_1,
    gTilesetAnims_BattleDomePals0_2,
    gTilesetAnims_BattleDomePals0_3,
};

// Only entries below the size counter are ever read, so resetting the counter is enough; zeroing the
// whole buffer every frame (as vanilla did) was wasted work.
static void ResetTilesetAnimBuffer(void)
{
    sTilesetDMA3TransferBufferSize = 0;
}

static void AppendTilesetAnimToBuffer(const u16 *src, u16 *dest, u16 size)
{
    if (sTilesetDMA3TransferBufferSize < 20)
    {
        sTilesetDMA3TransferBuffer[sTilesetDMA3TransferBufferSize].src = src;
        sTilesetDMA3TransferBuffer[sTilesetDMA3TransferBufferSize].dest = dest;
        sTilesetDMA3TransferBuffer[sTilesetDMA3TransferBufferSize].size = size;
        sTilesetDMA3TransferBufferSize ++;
    }
}

void TransferTilesetAnimsBuffer(void)
{
    int i;

    for (i = 0; i < sTilesetDMA3TransferBufferSize; i ++)
        DmaCopy16(3, sTilesetDMA3TransferBuffer[i].src, sTilesetDMA3TransferBuffer[i].dest, sTilesetDMA3TransferBuffer[i].size);

    sTilesetDMA3TransferBufferSize = 0;
}

void InitTilesetAnimations(void)
{
    ResetTilesetAnimBuffer();
    _InitPrimaryTilesetAnimation();
    _InitSecondaryTilesetAnimation();
}

void InitSecondaryTilesetAnimation(void)
{
    _InitSecondaryTilesetAnimation();
}

void UpdateTilesetAnimations(void)
{
    ResetTilesetAnimBuffer();
    if (++sPrimaryTilesetAnimCounter >= sPrimaryTilesetAnimCounterMax)
        sPrimaryTilesetAnimCounter = 0;
    if (++sSecondaryTilesetAnimCounter >= sSecondaryTilesetAnimCounterMax)
        sSecondaryTilesetAnimCounter = 0;

    if (sPrimaryTilesetAnimCallback)
        sPrimaryTilesetAnimCallback(sPrimaryTilesetAnimCounter);
    if (sSecondaryTilesetAnimCallback)
        sSecondaryTilesetAnimCallback(sSecondaryTilesetAnimCounter);

    // The overworld map is drawn by the software compositor, which keeps tileset graphics in its
    // own EWRAM cache instead of VRAM. Feed each queued animation frame into that cache and
    // recomposite the affected on-screen tiles here in the main loop, then drop the queue so the
    // VBlank DMA path (TransferTilesetAnimsBuffer) stays idle. The `dest` VRAM address each
    // QueueAnimTiles_* produced encodes the animated tiles' absolute tile id.
    if (IsFieldCompositorActive())
    {
        u32 i;

        for (i = 0; i < sTilesetDMA3TransferBufferSize; i++)
        {
            u32 firstTileId = ((u32)sTilesetDMA3TransferBuffer[i].dest - BG_VRAM) / TILE_SIZE_4BPP;
            FieldCompositorUpdateSourceTiles(firstTileId, sTilesetDMA3TransferBuffer[i].src,
                                             sTilesetDMA3TransferBuffer[i].size / TILE_SIZE_4BPP);
        }
        sTilesetDMA3TransferBufferSize = 0;
        // Recomposite a few of the slots the updates above marked dirty; the rest follow over the
        // next frames so a multi-animation frame doesn't recomposite everything at once.
        FieldCompositorTickAnim();
    }
}

static void _InitPrimaryTilesetAnimation(void)
{
    sPrimaryTilesetAnimCounter = 0;
    sPrimaryTilesetAnimCounterMax = 0;
    sPrimaryTilesetAnimCallback = NULL;
    // Phased groups live in primary-tileset tile-id space; drop the old primary's before the new one's
    // init re-registers its own (a non-phased primary then just starts with none).
    FieldCompositorClearPhasedGroups();
    if (gMapHeader.mapLayout->primaryTileset && gMapHeader.mapLayout->primaryTileset->callback)
        gMapHeader.mapLayout->primaryTileset->callback();
}

static void _InitSecondaryTilesetAnimation(void)
{
    sSecondaryTilesetAnimCounter = 0;
    sSecondaryTilesetAnimCounterMax = 0;
    sSecondaryTilesetAnimCallback = NULL;
    if (GetActiveLocationData()->secondaryTileset && GetActiveLocationData()->secondaryTileset->callback)
        GetActiveLocationData()->secondaryTileset->callback();
}

void InitTilesetAnim_General(void)
{
    sPrimaryTilesetAnimCounter = 0;
    sPrimaryTilesetAnimCounterMax = 256;
    sPrimaryTilesetAnimCallback = TilesetAnim_General;
}

// Phased-group indices for the terrain water surfaces (see FieldCompositorRegisterPhasedGroup).
enum {
    PHASED_GROUP_TERRAIN_WATER,
    PHASED_GROUP_TERRAIN_WATER_DEEP,
    PHASED_GROUP_TERRAIN_WATER_DEEP_EDGE,
    PHASED_GROUP_TERRAIN_WATER_SHALLOW,
    PHASED_GROUP_TERRAIN_WATER_SHALLOW_EDGE,
    PHASED_GROUP_TERRAIN_WATER_LAND_BORDER,
    PHASED_GROUP_TERRAIN_WATER_CURRENTS,
    PHASED_GROUP_TERRAIN_WATER_CURRENTS_SIDE,
    PHASED_GROUP_TERRAIN_WATER_WAVES,
};

// The terrain water-wave overlay scales its animation with wind strength: below WAVES_WIND_CALM it holds
// a single frame (dead calm), then steps up through the calm, mid and rough loops as it strengthens. A
// change only takes effect at a loop boundary (see TilesetAnim_Terrain) so the surface finishes its
// current cycle before switching instead of snapping mid-wave.
enum {
    WAVES_SET_LOCKED,
    WAVES_SET_CALM,
    WAVES_SET_MID,
    WAVES_SET_ROUGH,
};

// Wind-speed cutoffs: 0-5 locked, 6-11 calm, 12-18 mid, 19+ rough.
#define WAVES_WIND_CALM  6
#define WAVES_WIND_MID   12
#define WAVES_WIND_ROUGH 19

// Every wave set is WAVES_FRAME_COUNT frames, so a cell's phase (band = PhaseFn_Terrain_Water %
// WAVES_FRAME_COUNT) doesn't change when the frame set swaps - that's what lets a wind-strength swap avoid a
// re-phasing full redraw. See FieldCompositorReloadPhasedGroup. (A wind-heading change does alter the phase
// and redraws; see below.)
#define WAVES_FRAME_COUNT 6

// Each set is a selection of the 6 direction frames (built by BuildWaveDirFrames), in play order. Locked
// (dead calm) just repeats frame 0, so it animates uniformly like the rest but holds a single image.
static const u8 sWaveSetLocked[] = {0, 0, 0, 0, 0, 0};
static const u8 sWaveSetCalm[]   = {0, 0, 0, 3, 4, 5};
static const u8 sWaveSetMid[]    = {0, 0, 2, 3, 4, 5};
static const u8 sWaveSetRough[]  = {0, 1, 2, 3, 4, 5};

static const u8 *const sTerrainWavesSets[] = {
    [WAVES_SET_LOCKED] = sWaveSetLocked,
    [WAVES_SET_CALM]   = sWaveSetCalm,
    [WAVES_SET_MID]    = sWaveSetMid,
    [WAVES_SET_ROUGH]  = sWaveSetRough,
};

static u8 sTerrainWavesSet;

static u8 TerrainWavesSetForWind(void)
{
    u8 speed = gSaveBlock1Ptr->weatherState.windSpeed;
    if (speed < WAVES_WIND_CALM)
        return WAVES_SET_LOCKED;
    if (speed < WAVES_WIND_MID)
        return WAVES_SET_CALM;
    if (speed < WAVES_WIND_ROUGH)
        return WAVES_SET_MID;
    return WAVES_SET_ROUGH;
}

// Terrain water frames advance every WavesFrameTicks() frames (the shared water step; see TilesetAnim_Terrain).
// The top wind level (WIND_SPEED_MAX) animates at WAVES_FRAME_TICKS; each point of wind below adds one tick, so
// the surface animates progressively slower as the wind drops.
#define WAVES_FRAME_TICKS 16
#define WAVES_STEP_WRAP   48   // steps before wrap (768/16); keeps the 12- and 6-frame water loops closing clean

static u16 sWavesPhasedStep;   // persistent shared water step (0..WAVES_STEP_WRAP-1)
static u8 sWavesStepAccum;     // ticks accumulated toward the next step
static u8 sWavesLoopPhase;     // step % WAVES_FRAME_COUNT, tracked instead of divided per frame
                               // (exact because WAVES_STEP_WRAP is a multiple of WAVES_FRAME_COUNT)

static u16 WavesFrameTicks(void)
{
    u8 speed = gSaveBlock1Ptr->weatherState.windSpeed;
    return (speed >= WIND_SPEED_MAX) ? WAVES_FRAME_TICKS : WAVES_FRAME_TICKS + (WIND_SPEED_MAX - speed);
}

// ---- Wind-direction-driven wave travel -------------------------------------------------------------
// The waves flow toward the wind heading, quantised to 8 directions. Each direction reuses one of the two
// base wave sets (cardinal / diagonal), mirrored or rotated so the crest points the right way, plus a phase
// gradient (PhaseFn_Terrain_Water) so the traveling wave crosses the map in the same direction. Direction is
// handled like wind strength: it only switches at a wave loop boundary, and only when the quantised heading
// actually changes.
enum {
    WAVE_BASE_CARDINAL,
    WAVE_BASE_DIAG,
};

// Frame transform applied to a 16x16 base frame to orient it. The cardinal base flows S, the diagonal base
// flows SW; the rest are those mirrored/rotated (see the numerically verified mapping in sWaveDirs).
enum {
    XFORM_ID,
    XFORM_FLIPX,   // mirror across y axis (left<->right)
    XFORM_FLIPY,   // mirror across x axis (up<->down)
    XFORM_FLIPXY,  // 180 deg
    XFORM_ROT_CW,  // 90 deg clockwise
    XFORM_ROT_CCW, // 90 deg counter-clockwise
};

enum {
    WAVE_DIR_N, WAVE_DIR_NE, WAVE_DIR_E,  WAVE_DIR_SE,
    WAVE_DIR_S, WAVE_DIR_SW, WAVE_DIR_W,  WAVE_DIR_NW,
};

static const struct {
    u8 base;
    u8 xform;
    s8 travelX; // screen travel vector (x right, y down); the phase gradient is -travel (see PhaseFn).
    s8 travelY;
} sWaveDirs[8] = {
    [WAVE_DIR_N]  = { WAVE_BASE_CARDINAL, XFORM_FLIPY,    0, -1 },
    [WAVE_DIR_NE] = { WAVE_BASE_DIAG,     XFORM_FLIPXY,   1, -1 },
    [WAVE_DIR_E]  = { WAVE_BASE_CARDINAL, XFORM_ROT_CCW,  1,  0 },
    [WAVE_DIR_SE] = { WAVE_BASE_DIAG,     XFORM_FLIPX,    1,  1 },
    [WAVE_DIR_S]  = { WAVE_BASE_CARDINAL, XFORM_ID,       0,  1 },
    [WAVE_DIR_SW] = { WAVE_BASE_DIAG,     XFORM_ID,      -1,  1 }, // the original NE->SW flow
    [WAVE_DIR_W]  = { WAVE_BASE_CARDINAL, XFORM_ROT_CW,  -1,  0 },
    [WAVE_DIR_NW] = { WAVE_BASE_DIAG,     XFORM_FLIPY,   -1, -1 },
};

static u8 sWavesTravelDir;                        // current WAVE_DIR_*
static s8 sWavePhaseX, sWavePhaseY;               // phase gradient = -travel, read by PhaseFn_Terrain_Water
static EWRAM_DATA ALIGNED(4) u8 sWaveDirBuf[6][TILE_SIZE_8BPP * 4] = {0}; // 6 frames of 4 8bpp tiles, oriented for the current dir
static const u16 *sWaveDirFrames[6];              // -> each sWaveDirBuf frame
static const u16 *sActiveWaveFrames[6];           // current set's selection into sWaveDirFrames

// Heading-flip staging (see TilesetAnim_Terrain): while a flip is in progress the surface is frozen. First the
// new crest banks reflatten a bounded slice per frame (staging); then the map re-phases and the surface is
// recomposed in VBlank, bounded, over a frame or two (landing). Frozen throughout so nothing tears/spikes.
static bool8 sWavesFlipPending;
static bool8 sWavesFlipLanding;                    // staging done; VBlank surface refresh in progress
static u8 sWavesFlipDir;                            // the heading being flipped to
#define WAVES_FLIP_REBUILD_BUDGET 32               // bank frames reflattened per frozen frame (no step refresh, so headroom)

// Byte offset of pixel (px,py) in a 16x16 metatile stored as 4 8bpp tiles in gbagfx order (TL,TR,BL,BR).
static inline u32 WavePixelOffset(u32 px, u32 py)
{
    return ((py >> 3) * 2 + (px >> 3)) * TILE_SIZE_8BPP + (py & 7) * 8 + (px & 7);
}

// Each transform as an affine basis: source pixel (sx,sy) lands at dest (originX,originY) plus sx steps
// of (stepXX,stepXY) and sy steps of (stepYX,stepYY). Lets BuildWaveDirFrames walk the pixels with two
// adds instead of re-deriving the mapping per pixel.
static const struct {
    s8 originX, originY;
    s8 stepXX, stepXY; // dest delta per source +x
    s8 stepYX, stepYY; // dest delta per source +y
} sWaveXforms[] = {
    [XFORM_ID]      = { 0,  0,   1,  0,   0,  1 },
    [XFORM_FLIPX]   = {15,  0,  -1,  0,   0,  1 },
    [XFORM_FLIPY]   = { 0, 15,   1,  0,   0, -1 },
    [XFORM_FLIPXY]  = {15, 15,  -1,  0,   0, -1 },
    [XFORM_ROT_CW]  = {15,  0,   0,  1,  -1,  0 }, // (sx,sy) -> (15-sy, sx)
    [XFORM_ROT_CCW] = { 0, 15,   0, -1,   1,  0 }, // (sx,sy) -> (sy, 15-sx)
};

// Rebuild the 6 per-direction wave frames from the base set, oriented for `dir`. Cheap (6 * 256 byte moves).
// Does NOT touch the phase gradient - that switches only at the flip (SetWavePhaseForDir), so a flip in
// progress keeps the frozen surface on its old phase until the new crest banks are ready.
static void BuildWaveDirFrames(u8 dir)
{
    bool32 rough = TerrainWavesSetForWind() == WAVES_SET_ROUGH;
    const u16 *const *base = (sWaveDirs[dir].base == WAVE_BASE_DIAG)
        ? (rough ? sWaveBaseDiagRough : sWaveBaseDiag)
        : (rough ? sWaveBaseCardinalRough : sWaveBaseCardinal);
    u8 xform = sWaveDirs[dir].xform;
    u32 f, sx, sy;

    for (f = 0; f < 6; f++)
    {
        const u8 *src = (const u8 *)base[f];
        u8 *dst = sWaveDirBuf[f];
        s32 rowDx = sWaveXforms[xform].originX;
        s32 rowDy = sWaveXforms[xform].originY;

        for (sy = 0; sy < 16; sy++)
        {
            // Source pixels of one 16-wide row: sequential within each 8-wide tile half.
            const u8 *srcL = src + WavePixelOffset(0, sy);
            const u8 *srcR = src + WavePixelOffset(8, sy);
            s32 dx = rowDx, dy = rowDy;

            for (sx = 0; sx < 16; sx++)
            {
                dst[WavePixelOffset(dx, dy)] = (sx < 8) ? srcL[sx] : srcR[sx - 8];
                dx += sWaveXforms[xform].stepXX;
                dy += sWaveXforms[xform].stepXY;
            }
            rowDx += sWaveXforms[xform].stepYX;
            rowDy += sWaveXforms[xform].stepYY;
        }
        sWaveDirFrames[f] = (const u16 *)dst;
    }
}

// Point the shared water phase gradient at `dir` (all four water groups travel this way). -travel so the SW
// default reduces to the original x - y.
static void SetWavePhaseForDir(u8 dir)
{
    sWavePhaseX = -sWaveDirs[dir].travelX;
    sWavePhaseY = -sWaveDirs[dir].travelY;
}

// Quantise the wind heading (BAM angle, 0=N clockwise) to one of the 8 travel directions.
static u8 WaveDirForWind(void)
{
    return ((gSaveBlock1Ptr->weatherState.windDirection + 16) >> 5) & 7;
}

// Tile range the waves phased group occupies (0x184, 4 tiles); the direction swap rebuilds its banks by range.
#define WAVES_FIRST_TILE 0x184
#define WAVES_TILE_COUNT 4

// How RegisterTerrainWavesGroup (re)registers the waves group:
enum {
    WAVES_REG_COLD, // fresh map load: cold register
    WAVES_REG_LAZY, // strength / heading swap: re-point frames + mark banks for rebuild (rebuildMask), no redraw
};

// (Re)register the waves phased group for `set`, pulling the set's frames from the current direction buffer.
// Every set is WAVES_FRAME_COUNT frames, so band count == frame count and the per-cell phase stays put across
// a strength swap.
static void RegisterTerrainWavesGroup(u8 set, u8 mode)
{
    const u8 *idx = sTerrainWavesSets[set];
    u8 i;

    for (i = 0; i < WAVES_FRAME_COUNT; i++)
        sActiveWaveFrames[i] = sWaveDirFrames[idx[i]];

    if (mode == WAVES_REG_LAZY)
        FieldCompositorReloadPhasedGroup(PHASED_GROUP_TERRAIN_WATER_WAVES, WAVES_FIRST_TILE, WAVES_TILE_COUNT, sActiveWaveFrames, WAVES_FRAME_COUNT, WAVES_FRAME_COUNT, PhaseFn_Terrain_Water);
    else // WAVES_REG_COLD: cold register
        FieldCompositorRegisterPhasedGroup(PHASED_GROUP_TERRAIN_WATER_WAVES, WAVES_FIRST_TILE, WAVES_TILE_COUNT, sActiveWaveFrames, WAVES_FRAME_COUNT, WAVES_FRAME_COUNT, PhaseFn_Terrain_Water);
}

void InitTilesetAnim_Terrain(void)
{
    sPrimaryTilesetAnimCounter = 0;
    // 768 = 48 steps of 16 frames: divisible by 12 (water/deep/deep-edge) and by 6 (the waves loop length)
    // so each water group wraps cleanly, and a multiple of 256 so inheriting secondaries don't regress.
    sPrimaryTilesetAnimCounterMax = 768;
    sPrimaryTilesetAnimCallback = TilesetAnim_Terrain;

    // All four water groups travel toward the wind heading: orient the wave frames and set the shared phase
    // gradient before registering, so PhaseFn_Terrain_Water reads the right direction from the first draw.
    sWavesFlipPending = FALSE;
    sWavesFlipLanding = FALSE;
    sWavesPhasedStep = 0;
    sWavesStepAccum = 0;
    sWavesLoopPhase = 0;
    sWavesTravelDir = WaveDirForWind();
    BuildWaveDirFrames(sWavesTravelDir);
    SetWavePhaseForDir(sWavesTravelDir);

    // Water animates per position: the displayed frame is picked from each cell's map location
    // (PhaseFn_Terrain_Water) so the surface ripples across the map instead of moving in unison. The
    // compositor reads frames straight from ROM; TilesetAnim_Terrain only advances the step counter.
    FieldCompositorRegisterPhasedGroup(PHASED_GROUP_TERRAIN_WATER, 0x170, 4,
        gTilesetAnims_Terrain_Water, ARRAY_COUNT(gTilesetAnims_Terrain_Water), 12, PhaseFn_Terrain_Water);
    FieldCompositorRegisterPhasedGroup(PHASED_GROUP_TERRAIN_WATER_DEEP, 0x17B, 4,
        gTilesetAnims_Terrain_Water_Deep, ARRAY_COUNT(gTilesetAnims_Terrain_Water_Deep), 12, PhaseFn_Terrain_Water);
    FieldCompositorRegisterPhasedGroup(PHASED_GROUP_TERRAIN_WATER_DEEP_EDGE, 0x180, 4,
        gTilesetAnims_Terrain_Water_Deep_Edge, ARRAY_COUNT(gTilesetAnims_Terrain_Water_Deep_Edge), 12, PhaseFn_Terrain_Water);
    FieldCompositorRegisterPhasedGroup(PHASED_GROUP_TERRAIN_WATER_SHALLOW, 0x188, 4,
        gTilesetAnims_Terrain_Water_Shallow, ARRAY_COUNT(gTilesetAnims_Terrain_Water_Shallow), 12, PhaseFn_Terrain_Water);
    FieldCompositorRegisterPhasedGroup(PHASED_GROUP_TERRAIN_WATER_SHALLOW_EDGE, 0x18C, 4,
        gTilesetAnims_Terrain_Water_Shallow_Edge, ARRAY_COUNT(gTilesetAnims_Terrain_Water_Shallow_Edge), 12, PhaseFn_Terrain_Water);
    // Border must be phased too, not an override anim: its tiles share subtiles with the phased water
    // (metatiles blend water + border), so those slots get banked - and a banked slot copies its
    // pre-built frame, ignoring override writes, which froze the old QueueAnimTiles approach. As a phased
    // group the ping-pong (4-frame {0,1,2,1}) bakes into the banks. Uniform phase (PhaseFn_Zero, 1 band)
    // so the whole border animates in unison; it shares the water step, so it advances at the water rate.
    FieldCompositorRegisterPhasedGroup(PHASED_GROUP_TERRAIN_WATER_LAND_BORDER, 0x190, 10,
        gTilesetAnims_Terrain_Water_Land_Border, ARRAY_COUNT(gTilesetAnims_Terrain_Water_Land_Border), 1, PhaseFn_Zero);
    // Currents: phased for the same reason as the border (they blend with the phased waves, so their slots
    // bank). Position-dependent phase (PhaseFn_Terrain_Water, bandCount = frameCount) so they ripple across
    // the map with the wind like the water surfaces, rather than cycling in unison.
    FieldCompositorRegisterPhasedGroup(PHASED_GROUP_TERRAIN_WATER_CURRENTS, 0x19A, 4,
        gTilesetAnims_Terrain_Water_Currents, ARRAY_COUNT(gTilesetAnims_Terrain_Water_Currents), 12, PhaseFn_Terrain_Water);
    FieldCompositorRegisterPhasedGroup(PHASED_GROUP_TERRAIN_WATER_CURRENTS_SIDE, 0x19E, 4,
        gTilesetAnims_Terrain_Water_Currents_Side, ARRAY_COUNT(gTilesetAnims_Terrain_Water_Currents_Side), 12, PhaseFn_Terrain_Water);
    // Waves start at whatever set the current wind calls for, then swap at loop boundaries as it changes.
    sTerrainWavesSet = TerrainWavesSetForWind();
    RegisterTerrainWavesGroup(sTerrainWavesSet, WAVES_REG_COLD);
}

void InitTilesetAnim_Building(void)
{
    sPrimaryTilesetAnimCounter = 0;
    sPrimaryTilesetAnimCounterMax = 256;
    sPrimaryTilesetAnimCallback = TilesetAnim_Building;
}

static void TilesetAnim_General(u16 timer)
{
    if (timer % 16 == 0)
        QueueAnimTiles_General_Flower(timer / 16);
    if (timer % 16 == 1)
        QueueAnimTiles_General_Water(timer / 16);
    if (timer % 16 == 2)
        QueueAnimTiles_General_SandWaterEdge(timer / 16);
    if (timer % 16 == 3)
        QueueAnimTiles_General_Waterfall(timer / 16);
    if (timer % 16 == 4)
        QueueAnimTiles_General_LandWaterEdge(timer / 16);
}

// Drive an in-progress heading flip; TRUE while the surface is frozen (the caller must not advance the
// phased step, so the display holds its last frame throughout).
//
// A heading flip changes both the crest orientation (bank content) and the phase gradient across the
// WHOLE surface. Doing that in one frame either tears (live-flatten storm to VRAM) or spikes (rebuild
// every wave bank at once), so instead it's staged: while frozen, the new crest banks reflatten a
// bounded slice per frame into heap - no VRAM writes, no spike. When they're all rebuilt, the flip
// lands atomically: switch the phase gradient, re-acquire the surface for it (DrawWholeMapView;
// tilemap lands in VBlank), and copy the rebuilt banks into VRAM as cheap 64-byte writes via the
// VBlank refresh. The freeze lasts only a handful of frames.
static bool32 UpdateTerrainWavesFlip(void)
{
    if (!sWavesFlipPending)
        return FALSE;

    if (!sWavesFlipLanding)
    {
        // Staging: reflatten the new crest banks into heap (safe even while a textbox is up - no VRAM),
        // then land once banks are ready AND field controls are free. HOLD the land under an open
        // message box: its DrawWholeMapView redraws the whole map and would corrupt the BG0 text plane
        // (e.g. the surf prompt, opened by pressing A on water mid-freeze). Once controls unlock, land:
        // switch the phase gradient, re-phase the map (DrawWholeMapView; tilemap flushes in VBlank), and
        // kick off the VBlank surface refresh - the recompose runs in VBlank so a full screen of
        // large-delta writes lands during blanking instead of tearing mid-scanout.
        if (FieldCompositorProcessBankRebuild(WAVES_FIRST_TILE, WAVES_FIRST_TILE + WAVES_TILE_COUNT - 1, WAVES_FLIP_REBUILD_BUDGET)
         && !ArePlayerFieldControlsLocked())
        {
            SetWavePhaseForDir(sWavesFlipDir);
            DrawWholeMapView();
            FieldCompositorBeginPhasedFlipRefresh();
            sWavesFlipLanding = TRUE;
        }
    }
    else if (!FieldCompositorPhasedFlipRefreshActive())
    {
        // The VBlank refresh has recomposed the whole surface: the new heading is fully on screen. Resume.
        sWavesFlipLanding = FALSE;
        sWavesFlipPending = FALSE;
    }
    return TRUE;
}

// React to the current wind at a wave loop boundary, so the surface finishes its cycle before snapping.
// A strength swap keeps every cell's phase (fixed band count) and lazily retires the old frames - no
// redraw, safe any time. A heading change starts the staged flip (see UpdateTerrainWavesFlip); it's held
// off while field controls are locked (an open message box - e.g. the surf prompt - or a running script)
// so the redraw can't disturb a textbox. The wind holds steady while locked (see UpdateWindDirection),
// so no heading change is even pending until controls free up. Returns TRUE when a flip was staged (the
// freeze begins this frame).
static bool32 UpdateTerrainWavesWind(void)
{
    u8 desiredSet = TerrainWavesSetForWind();
    u8 desiredDir = WaveDirForWind();
    bool32 dirChanged = (desiredDir != sWavesTravelDir) && !ArePlayerFieldControlsLocked();

    if (desiredSet != sTerrainWavesSet && !dirChanged)
    {
        // Crossing the rough boundary swaps the base frame art, so re-orient the dir buffer;
        // the lazy reload re-flattens the banks from it.
        if ((desiredSet == WAVES_SET_ROUGH) != (sTerrainWavesSet == WAVES_SET_ROUGH))
            BuildWaveDirFrames(sWavesTravelDir);
        sTerrainWavesSet = desiredSet;
        RegisterTerrainWavesGroup(desiredSet, WAVES_REG_LAZY);
    }
    if (dirChanged)
    {
        // Stage the flip: orient the new crest frames and (re)register the group (also applies any
        // strength change) with rebuildMask set, so ProcessBankRebuild drains the reflattens over the
        // next few frozen frames. The phase gradient stays on the old heading until the flip lands.
        sWavesTravelDir = desiredDir;
        sWavesFlipDir = desiredDir;
        sTerrainWavesSet = desiredSet;
        BuildWaveDirFrames(desiredDir);
        RegisterTerrainWavesGroup(desiredSet, WAVES_REG_LAZY);
        sWavesFlipPending = TRUE;
        return TRUE;
    }
    return FALSE;
}

static void TilesetAnim_Terrain(u16 timer)
{
    (void)timer;
    // Advance the shared water step from a persistent accumulator rather than timer/16: the per-frame
    // cadence (WavesFrameTicks) grows as the wind drops, and a changing divisor would snap the phase.
    // Wraps at WAVES_STEP_WRAP like the old timer/16 so the water loops still close cleanly.
    if (++sWavesStepAccum >= WavesFrameTicks())
    {
        sWavesStepAccum = 0;
        if (++sWavesPhasedStep >= WAVES_STEP_WRAP)
            sWavesPhasedStep = 0;
        if (++sWavesLoopPhase >= WAVES_FRAME_COUNT)
            sWavesLoopPhase = 0;
    }

    if (IsFieldCompositorActive())
    {
        if (UpdateTerrainWavesFlip())
            return; // frozen throughout the flip (staging + landing): hold the phased step until done
        if (sWavesLoopPhase == 0 && UpdateTerrainWavesWind())
            return; // a flip was just staged: begin the freeze this frame
    }

    // Water's animation frame advances every 16 timer ticks. All four water groups share one step and
    // flip together; each on-screen slot's new frame is a cheap copy from its pre-composited bank (see
    // FieldCompositorTickPhased), so the whole surface can update on one frame without a recompose spike.
    FieldCompositorTickPhased(sWavesPhasedStep);
}

// Position -> raw signed phase for the water surfaces. A traveling wave toward the current wind heading:
// the phase gradient (-travel vector, set by BuildWaveDirFrames) advances one band per tile along the
// flow. The compositor floor-mods this by the group's band count, so only that many distinct bands ever
// composite (keeps large water areas off the slot pool). The result is SIGNED - a negative phase (the y>x
// half of the map) must survive to the floor mod, so returning it as u8 here would seam. Default SW = x - y.
static s16 PhaseFn_Terrain_Water(s16 x, s16 y)
{
    return sWavePhaseX * x + sWavePhaseY * y;
}

// Uniform phase: every cell shows the same frame in unison (no traveling wave). Used by the water/land
// border, which ping-pongs as one rather than rippling across the map.
static s16 PhaseFn_Zero(s16 x, s16 y)
{
    (void)x; (void)y;
    return 0;
}

static void TilesetAnim_Building(u16 timer)
{
    if (timer % 8 == 0)
        QueueAnimTiles_Building_TVTurnedOn(timer / 8);
}

static void QueueAnimTiles_General_Flower(u16 timer)
{
    u16 i = timer % ARRAY_COUNT(gTilesetAnims_General_Flower);
    AppendTilesetAnimToBuffer(gTilesetAnims_General_Flower[i], (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(508)), 4 * TILE_SIZE_4BPP);
}

static void QueueAnimTiles_General_Water(u16 timer)
{
    u8 i = timer % ARRAY_COUNT(gTilesetAnims_General_Water);
    AppendTilesetAnimToBuffer(gTilesetAnims_General_Water[i], (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(432)), 30 * TILE_SIZE_4BPP);
}

static void QueueAnimTiles_General_SandWaterEdge(u16 timer)
{
    u16 i = timer % ARRAY_COUNT(gTilesetAnims_General_SandWaterEdge);
    AppendTilesetAnimToBuffer(gTilesetAnims_General_SandWaterEdge[i], (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(464)), 10 * TILE_SIZE_4BPP);
}

static void QueueAnimTiles_General_Waterfall(u16 timer)
{
    u16 i = timer % ARRAY_COUNT(gTilesetAnims_General_Waterfall);
    AppendTilesetAnimToBuffer(gTilesetAnims_General_Waterfall[i], (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(496)), 6 * TILE_SIZE_4BPP);
}

void InitTilesetAnim_Petalburg(void)
{
    sSecondaryTilesetAnimCounter = 0;
    sSecondaryTilesetAnimCounterMax = sPrimaryTilesetAnimCounterMax;
    sSecondaryTilesetAnimCallback = NULL;
}

void InitTilesetAnim_Rustboro(void)
{
    sSecondaryTilesetAnimCounter = 0;
    sSecondaryTilesetAnimCounterMax = sPrimaryTilesetAnimCounterMax;
    sSecondaryTilesetAnimCallback = TilesetAnim_Rustboro;
}

void InitTilesetAnim_Dewford(void)
{
    sSecondaryTilesetAnimCounter = 0;
    sSecondaryTilesetAnimCounterMax = sPrimaryTilesetAnimCounterMax;
    sSecondaryTilesetAnimCallback = TilesetAnim_Dewford;
}

void InitTilesetAnim_Slateport(void)
{
    sSecondaryTilesetAnimCounter = 0;
    sSecondaryTilesetAnimCounterMax = sPrimaryTilesetAnimCounterMax;
    sSecondaryTilesetAnimCallback = TilesetAnim_Slateport;
}

void InitTilesetAnim_Mauville(void)
{
    sSecondaryTilesetAnimCounter = sPrimaryTilesetAnimCounter;
    sSecondaryTilesetAnimCounterMax = sPrimaryTilesetAnimCounterMax;
    sSecondaryTilesetAnimCallback = TilesetAnim_Mauville;
}

void InitTilesetAnim_Lavaridge(void)
{
    sSecondaryTilesetAnimCounter = 0;
    sSecondaryTilesetAnimCounterMax = sPrimaryTilesetAnimCounterMax;
    sSecondaryTilesetAnimCallback = TilesetAnim_Lavaridge;
}

void InitTilesetAnim_Fallarbor(void)
{
    sSecondaryTilesetAnimCounter = 0;
    sSecondaryTilesetAnimCounterMax = sPrimaryTilesetAnimCounterMax;
    sSecondaryTilesetAnimCallback = NULL;
}

void InitTilesetAnim_Fortree(void)
{
    sSecondaryTilesetAnimCounter = 0;
    sSecondaryTilesetAnimCounterMax = sPrimaryTilesetAnimCounterMax;
    sSecondaryTilesetAnimCallback = NULL;
}

void InitTilesetAnim_Lilycove(void)
{
    sSecondaryTilesetAnimCounter = 0;
    sSecondaryTilesetAnimCounterMax = sPrimaryTilesetAnimCounterMax;
    sSecondaryTilesetAnimCallback = NULL;
}

void InitTilesetAnim_Mossdeep(void)
{
    sSecondaryTilesetAnimCounter = 0;
    sSecondaryTilesetAnimCounterMax = sPrimaryTilesetAnimCounterMax;
    sSecondaryTilesetAnimCallback = NULL;
}

void InitTilesetAnim_EverGrande(void)
{
    sSecondaryTilesetAnimCounter = 0;
    sSecondaryTilesetAnimCounterMax = sPrimaryTilesetAnimCounterMax;
    sSecondaryTilesetAnimCallback = TilesetAnim_EverGrande;
}

void InitTilesetAnim_Pacifidlog(void)
{
    sSecondaryTilesetAnimCounter = sPrimaryTilesetAnimCounter;
    sSecondaryTilesetAnimCounterMax = sPrimaryTilesetAnimCounterMax;
    sSecondaryTilesetAnimCallback = TilesetAnim_Pacifidlog;
}

void InitTilesetAnim_Sootopolis(void)
{
    sSecondaryTilesetAnimCounter = 0;
    sSecondaryTilesetAnimCounterMax = sPrimaryTilesetAnimCounterMax;
    sSecondaryTilesetAnimCallback = TilesetAnim_Sootopolis;
}

void InitTilesetAnim_BattleFrontierOutsideWest(void)
{
    sSecondaryTilesetAnimCounter = 0;
    sSecondaryTilesetAnimCounterMax = sPrimaryTilesetAnimCounterMax;
    sSecondaryTilesetAnimCallback = TilesetAnim_BattleFrontierOutsideWest;
}

void InitTilesetAnim_BattleFrontierOutsideEast(void)
{
    sSecondaryTilesetAnimCounter = 0;
    sSecondaryTilesetAnimCounterMax = sPrimaryTilesetAnimCounterMax;
    sSecondaryTilesetAnimCallback = TilesetAnim_BattleFrontierOutsideEast;
}

void InitTilesetAnim_Underwater(void)
{
    sSecondaryTilesetAnimCounter = 0;
    sSecondaryTilesetAnimCounterMax = 128;
    sSecondaryTilesetAnimCallback = TilesetAnim_Underwater;
}

void InitTilesetAnim_SootopolisGym(void)
{
    sSecondaryTilesetAnimCounter = 0;
    sSecondaryTilesetAnimCounterMax = 240;
    sSecondaryTilesetAnimCallback = TilesetAnim_SootopolisGym;
}

void InitTilesetAnim_Cave(void)
{
    sSecondaryTilesetAnimCounter = 0;
    sSecondaryTilesetAnimCounterMax = sPrimaryTilesetAnimCounterMax;
    sSecondaryTilesetAnimCallback = TilesetAnim_Cave;
}

void InitTilesetAnim_EliteFour(void)
{
    sSecondaryTilesetAnimCounter = 0;
    sSecondaryTilesetAnimCounterMax = 128;
    sSecondaryTilesetAnimCallback = TilesetAnim_EliteFour;
}

void InitTilesetAnim_MauvilleGym(void)
{
    sSecondaryTilesetAnimCounter = 0;
    sSecondaryTilesetAnimCounterMax = sPrimaryTilesetAnimCounterMax;
    sSecondaryTilesetAnimCallback = TilesetAnim_MauvilleGym;
}

void InitTilesetAnim_BikeShop(void)
{
    sSecondaryTilesetAnimCounter = 0;
    sSecondaryTilesetAnimCounterMax = sPrimaryTilesetAnimCounterMax;
    sSecondaryTilesetAnimCallback = TilesetAnim_BikeShop;
}

void InitTilesetAnim_BattlePyramid(void)
{
    sSecondaryTilesetAnimCounter = 0;
    sSecondaryTilesetAnimCounterMax = sPrimaryTilesetAnimCounterMax;
    sSecondaryTilesetAnimCallback = TilesetAnim_BattlePyramid;
}

void InitTilesetAnim_BattleDome(void)
{
    sSecondaryTilesetAnimCounter = 0;
    sSecondaryTilesetAnimCounterMax = sPrimaryTilesetAnimCounterMax;
    sSecondaryTilesetAnimCallback = TilesetAnim_BattleDome;
}

static void TilesetAnim_Rustboro(u16 timer)
{
    if (timer % 8 == 0)
    {
        QueueAnimTiles_Rustboro_WindyWater(timer / 8, 0);
        QueueAnimTiles_Rustboro_Fountain(timer / 8);
    }
    if (timer % 8 == 1)
        QueueAnimTiles_Rustboro_WindyWater(timer / 8, 1);
    if (timer % 8 == 2)
        QueueAnimTiles_Rustboro_WindyWater(timer / 8, 2);
    if (timer % 8 == 3)
        QueueAnimTiles_Rustboro_WindyWater(timer / 8, 3);
    if (timer % 8 == 4)
        QueueAnimTiles_Rustboro_WindyWater(timer / 8, 4);
    if (timer % 8 == 5)
        QueueAnimTiles_Rustboro_WindyWater(timer / 8, 5);
    if (timer % 8 == 6)
        QueueAnimTiles_Rustboro_WindyWater(timer / 8, 6);
    if (timer % 8 == 7)
        QueueAnimTiles_Rustboro_WindyWater(timer / 8, 7);
}

static void TilesetAnim_Dewford(u16 timer)
{
    if (timer % 8 == 0)
        QueueAnimTiles_Dewford_Flag(timer / 8);
}

static void TilesetAnim_Slateport(u16 timer)
{
    if (timer % 16 == 0)
        QueueAnimTiles_Slateport_Balloons(timer / 16);
}

static void TilesetAnim_Mauville(u16 timer)
{
    if (timer % 8 == 0)
        QueueAnimTiles_Mauville_Flowers(timer / 8, 0);
    if (timer % 8 == 1)
        QueueAnimTiles_Mauville_Flowers(timer / 8, 1);
    if (timer % 8 == 2)
        QueueAnimTiles_Mauville_Flowers(timer / 8, 2);
    if (timer % 8 == 3)
        QueueAnimTiles_Mauville_Flowers(timer / 8, 3);
    if (timer % 8 == 4)
        QueueAnimTiles_Mauville_Flowers(timer / 8, 4);
    if (timer % 8 == 5)
        QueueAnimTiles_Mauville_Flowers(timer / 8, 5);
    if (timer % 8 == 6)
        QueueAnimTiles_Mauville_Flowers(timer / 8, 6);
    if (timer % 8 == 7)
        QueueAnimTiles_Mauville_Flowers(timer / 8, 7);
}

static void TilesetAnim_Lavaridge(u16 timer)
{
    if (timer % 16 == 0)
        QueueAnimTiles_Lavaridge_Steam(timer / 16);
    if (timer % 16 == 1)
        QueueAnimTiles_Lavaridge_Lava(timer / 16);
}

static void TilesetAnim_EverGrande(u16 timer)
{
    if (timer % 8 == 0)
        QueueAnimTiles_EverGrande_Flowers(timer / 8, 0);
    if (timer % 8 == 1)
        QueueAnimTiles_EverGrande_Flowers(timer / 8, 1);
    if (timer % 8 == 2)
        QueueAnimTiles_EverGrande_Flowers(timer / 8, 2);
    if (timer % 8 == 3)
        QueueAnimTiles_EverGrande_Flowers(timer / 8, 3);
    if (timer % 8 == 4)
        QueueAnimTiles_EverGrande_Flowers(timer / 8, 4);
    if (timer % 8 == 5)
        QueueAnimTiles_EverGrande_Flowers(timer / 8, 5);
    if (timer % 8 == 6)
        QueueAnimTiles_EverGrande_Flowers(timer / 8, 6);
    if (timer % 8 == 7)
        QueueAnimTiles_EverGrande_Flowers(timer / 8, 7);
}

static void TilesetAnim_Pacifidlog(u16 timer)
{
    if (timer % 16 == 0)
        QueueAnimTiles_Pacifidlog_LogBridges(timer / 16);
    if (timer % 16 == 1)
        QueueAnimTiles_Pacifidlog_WaterCurrents(timer / 16);
}

static void TilesetAnim_Sootopolis(u16 timer)
{
    if (timer % 16 == 0)
        QueueAnimTiles_Sootopolis_StormyWater(timer / 16);
}

static void TilesetAnim_Underwater(u16 timer)
{
    if (timer % 16 == 0)
        QueueAnimTiles_Underwater_Seaweed(timer / 16);
}

static void TilesetAnim_Cave(u16 timer)
{
    if (timer % 16 == 1)
        QueueAnimTiles_Cave_Lava(timer / 16);
}

static void TilesetAnim_BattleFrontierOutsideWest(u16 timer)
{
    if (timer % 8 == 0)
        QueueAnimTiles_BattleFrontierOutsideWest_Flag(timer / 8);
}

static void TilesetAnim_BattleFrontierOutsideEast(u16 timer)
{
    if (timer % 8 == 0)
        QueueAnimTiles_BattleFrontierOutsideEast_Flag(timer / 8);
}

static void QueueAnimTiles_General_LandWaterEdge(u16 timer)
{
    u16 i = timer % ARRAY_COUNT(gTilesetAnims_General_LandWaterEdge);
    AppendTilesetAnimToBuffer(gTilesetAnims_General_LandWaterEdge[i], (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(480)), 10 * TILE_SIZE_4BPP);
}

static void QueueAnimTiles_Lavaridge_Steam(u8 timer)
{
    u8 i = timer % ARRAY_COUNT(gTilesetAnims_Lavaridge_Steam);
    AppendTilesetAnimToBuffer(gTilesetAnims_Lavaridge_Steam[i], (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 288)), 4 * TILE_SIZE_4BPP);

    i = (timer + 2) % (int)ARRAY_COUNT(gTilesetAnims_Lavaridge_Steam);
    AppendTilesetAnimToBuffer(gTilesetAnims_Lavaridge_Steam[i], (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 292)), 4 * TILE_SIZE_4BPP);
}

static void QueueAnimTiles_Pacifidlog_LogBridges(u8 timer)
{
    u8 i = timer % ARRAY_COUNT(gTilesetAnims_Pacifidlog_LogBridges);
    AppendTilesetAnimToBuffer(gTilesetAnims_Pacifidlog_LogBridges[i], (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 464)), 30 * TILE_SIZE_4BPP);
}

static void QueueAnimTiles_Underwater_Seaweed(u8 timer)
{
    u8 i = timer % ARRAY_COUNT(gTilesetAnims_Underwater_Seaweed);
    AppendTilesetAnimToBuffer(gTilesetAnims_Underwater_Seaweed[i], (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 496)), 4 * TILE_SIZE_4BPP);
}

static void QueueAnimTiles_Pacifidlog_WaterCurrents(u8 timer)
{
    u8 i = timer % ARRAY_COUNT(gTilesetAnims_Pacifidlog_WaterCurrents);
    AppendTilesetAnimToBuffer(gTilesetAnims_Pacifidlog_WaterCurrents[i], (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 496)), 8 * TILE_SIZE_4BPP);
}

static void QueueAnimTiles_Mauville_Flowers(u16 timer_div, u8 timer_mod)
{
    timer_div -= timer_mod;
    if (timer_div < min(ARRAY_COUNT(gTilesetAnims_Mauville_Flower1), ARRAY_COUNT(gTilesetAnims_Mauville_Flower2)))
    {
        timer_div %= min(ARRAY_COUNT(gTilesetAnims_Mauville_Flower1), ARRAY_COUNT(gTilesetAnims_Mauville_Flower2));
        AppendTilesetAnimToBuffer(gTilesetAnims_Mauville_Flower1[timer_div], gTilesetAnims_Mauville_Flower1_VDests[timer_mod], 4 * TILE_SIZE_4BPP);
        AppendTilesetAnimToBuffer(gTilesetAnims_Mauville_Flower2[timer_div], gTilesetAnims_Mauville_Flower2_VDests[timer_mod], 4 * TILE_SIZE_4BPP);
    }
    else
    {
        timer_div %= min(ARRAY_COUNT(gTilesetAnims_Mauville_Flower1_B), ARRAY_COUNT(gTilesetAnims_Mauville_Flower2_B));
        AppendTilesetAnimToBuffer(gTilesetAnims_Mauville_Flower1_B[timer_div], gTilesetAnims_Mauville_Flower1_VDests[timer_mod], 4 * TILE_SIZE_4BPP);
        AppendTilesetAnimToBuffer(gTilesetAnims_Mauville_Flower2_B[timer_div], gTilesetAnims_Mauville_Flower2_VDests[timer_mod], 4 * TILE_SIZE_4BPP);
    }
}

static void QueueAnimTiles_Rustboro_WindyWater(u16 timer_div, u8 timer_mod)
{
    timer_div -= timer_mod;
    timer_div %= ARRAY_COUNT(gTilesetAnims_Rustboro_WindyWater);
    if (gTilesetAnims_Rustboro_WindyWater[timer_div])
        AppendTilesetAnimToBuffer(gTilesetAnims_Rustboro_WindyWater[timer_div], gTilesetAnims_Rustboro_WindyWater_VDests[timer_mod], 4 * TILE_SIZE_4BPP);
}

static void QueueAnimTiles_Rustboro_Fountain(u16 timer)
{
    u16 i = timer % ARRAY_COUNT(gTilesetAnims_Rustboro_Fountain);
    AppendTilesetAnimToBuffer(gTilesetAnims_Rustboro_Fountain[i], (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 448)), 4 * TILE_SIZE_4BPP);
}

static void QueueAnimTiles_Lavaridge_Lava(u16 timer)
{
    u16 i = timer % ARRAY_COUNT(gTilesetAnims_Lavaridge_Cave_Lava);
    AppendTilesetAnimToBuffer(gTilesetAnims_Lavaridge_Cave_Lava[i], (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 160)), 4 * TILE_SIZE_4BPP);
}

static void QueueAnimTiles_EverGrande_Flowers(u16 timer_div, u8 timer_mod)
{
    timer_div -= timer_mod;
    timer_div %= ARRAY_COUNT(gTilesetAnims_EverGrande_Flowers);

    AppendTilesetAnimToBuffer(gTilesetAnims_EverGrande_Flowers[timer_div], gTilesetAnims_EverGrande_VDests[timer_mod], 4 * TILE_SIZE_4BPP);
}

static void QueueAnimTiles_Cave_Lava(u16 timer)
{
    u16 i = timer % ARRAY_COUNT(gTilesetAnims_Lavaridge_Cave_Lava);
    AppendTilesetAnimToBuffer(gTilesetAnims_Lavaridge_Cave_Lava[i], (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 416)), 4 * TILE_SIZE_4BPP);
}

static void QueueAnimTiles_Dewford_Flag(u16 timer)
{
    u16 i = timer % ARRAY_COUNT(gTilesetAnims_Dewford_Flag);
    AppendTilesetAnimToBuffer(gTilesetAnims_Dewford_Flag[i], (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 170)), 6 * TILE_SIZE_4BPP);
}

static void QueueAnimTiles_BattleFrontierOutsideWest_Flag(u16 timer)
{
    u16 i = timer % ARRAY_COUNT(gTilesetAnims_BattleFrontierOutsideWest_Flag);
    AppendTilesetAnimToBuffer(gTilesetAnims_BattleFrontierOutsideWest_Flag[i], (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 218)), 6 * TILE_SIZE_4BPP);
}

static void QueueAnimTiles_BattleFrontierOutsideEast_Flag(u16 timer)
{
    u16 i = timer % ARRAY_COUNT(gTilesetAnims_BattleFrontierOutsideEast_Flag);
    AppendTilesetAnimToBuffer(gTilesetAnims_BattleFrontierOutsideEast_Flag[i], (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 218)), 6 * TILE_SIZE_4BPP);
}

static void QueueAnimTiles_Slateport_Balloons(u16 timer)
{
    u16 i = timer % ARRAY_COUNT(gTilesetAnims_Slateport_Balloons);
    AppendTilesetAnimToBuffer(gTilesetAnims_Slateport_Balloons[i], (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 224)), 4 * TILE_SIZE_4BPP);
}

static void TilesetAnim_MauvilleGym(u16 timer)
{
    if (timer % 2 == 0)
        QueueAnimTiles_MauvilleGym_ElectricGates(timer / 2);
}

static void TilesetAnim_SootopolisGym(u16 timer)
{
    if (timer % 8 == 0)
        QueueAnimTiles_SootopolisGym_Waterfalls(timer / 8);
}

static void TilesetAnim_EliteFour(u16 timer)
{
    if (timer % 64 == 1)
        QueueAnimTiles_EliteFour_GroundLights(timer / 64);
    if (timer % 8 == 1)
        QueueAnimTiles_EliteFour_WallLights(timer / 8);
}

static void TilesetAnim_BikeShop(u16 timer)
{
    if (timer % 4 == 0)
        QueueAnimTiles_BikeShop_BlinkingLights(timer / 4);
}

static void TilesetAnim_BattlePyramid(u16 timer)
{
    if (timer % 8 == 0)
    {
        QueueAnimTiles_BattlePyramid_Torch(timer / 8);
        QueueAnimTiles_BattlePyramid_StatueShadow(timer / 8);
    }
}

static void TilesetAnim_BattleDome(u16 timer)
{
    if (timer % 4 == 0)
        BlendAnimPalette_BattleDome_FloorLights(timer / 4);
}

static void TilesetAnim_BattleDome2(u16 timer)
{
    if (timer % 4 == 0)
        BlendAnimPalette_BattleDome_FloorLightsNoBlend(timer / 4);
}

static void QueueAnimTiles_Building_TVTurnedOn(u16 timer)
{
    u16 i = timer % ARRAY_COUNT(gTilesetAnims_Building_TvTurnedOn);
    AppendTilesetAnimToBuffer(gTilesetAnims_Building_TvTurnedOn[i], (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(496)), 4 * TILE_SIZE_4BPP);
}

static void QueueAnimTiles_SootopolisGym_Waterfalls(u16 timer)
{
    u16 i = timer % min(ARRAY_COUNT(gTilesetAnims_SootopolisGym_SideWaterfall), ARRAY_COUNT(gTilesetAnims_SootopolisGym_FrontWaterfall));
    AppendTilesetAnimToBuffer(gTilesetAnims_SootopolisGym_SideWaterfall[i], (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 496)), 12 * TILE_SIZE_4BPP);
    AppendTilesetAnimToBuffer(gTilesetAnims_SootopolisGym_FrontWaterfall[i], (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 464)), 20 * TILE_SIZE_4BPP);
}

static void QueueAnimTiles_EliteFour_WallLights(u16 timer)
{
    u16 i = timer % ARRAY_COUNT(gTilesetAnims_EliteFour_WallLights);
    AppendTilesetAnimToBuffer(gTilesetAnims_EliteFour_WallLights[i], (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 504)), 1 * TILE_SIZE_4BPP);
}

static void QueueAnimTiles_EliteFour_GroundLights(u16 timer)
{
    u16 i = timer % ARRAY_COUNT(gTilesetAnims_EliteFour_FloorLight);
    AppendTilesetAnimToBuffer(gTilesetAnims_EliteFour_FloorLight[i], (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 480)), 4 * TILE_SIZE_4BPP);
}

static void QueueAnimTiles_MauvilleGym_ElectricGates(u16 timer)
{
    u16 i = timer % ARRAY_COUNT(gTilesetAnims_MauvilleGym_ElectricGates);
    AppendTilesetAnimToBuffer(gTilesetAnims_MauvilleGym_ElectricGates[i], (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 144)), 16 * TILE_SIZE_4BPP);
}

static void QueueAnimTiles_BikeShop_BlinkingLights(u16 timer)
{
    u16 i = timer % ARRAY_COUNT(gTilesetAnims_BikeShop_BlinkingLights);
    AppendTilesetAnimToBuffer(gTilesetAnims_BikeShop_BlinkingLights[i], (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 496)), 9 * TILE_SIZE_4BPP);
}

static void QueueAnimTiles_Sootopolis_StormyWater(u16 timer)
{
    u16 i = timer % ARRAY_COUNT(gTilesetAnims_Sootopolis_StormyWater);
    AppendTilesetAnimToBuffer(gTilesetAnims_Sootopolis_StormyWater[i], (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 240)), 96 * TILE_SIZE_4BPP);
}

static void QueueAnimTiles_BattlePyramid_Torch(u16 timer)
{
    u16 i = timer % ARRAY_COUNT(gTilesetAnims_BattlePyramid_Torch);
    AppendTilesetAnimToBuffer(gTilesetAnims_BattlePyramid_Torch[i], (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 151)), 8 * TILE_SIZE_4BPP);
}

static void QueueAnimTiles_BattlePyramid_StatueShadow(u16 timer)
{
    u16 i = timer % ARRAY_COUNT(gTilesetAnims_BattlePyramid_StatueShadow);
    AppendTilesetAnimToBuffer(gTilesetAnims_BattlePyramid_StatueShadow[i], (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(NUM_TILES_IN_PRIMARY + 135)), 8 * TILE_SIZE_4BPP);
}

static void BlendAnimPalette_BattleDome_FloorLights(u16 timer)
{
    CpuCopy16(sTilesetAnims_BattleDomeFloorLightPals[timer % ARRAY_COUNT(sTilesetAnims_BattleDomeFloorLightPals)], &gPlttBufferUnfaded[BG_PLTT_ID(8)], PLTT_SIZE_4BPP);
    BlendPalette(BG_PLTT_ID(8), 16, gPaletteFade.y, gPaletteFade.blendColor & 0x7FFF);
    if ((u8)FindTaskIdByFunc(Task_BattleTransition_Intro) != TASK_NONE)
    {
        sSecondaryTilesetAnimCallback = TilesetAnim_BattleDome2;
        sSecondaryTilesetAnimCounterMax = 32;
    }
}

static void BlendAnimPalette_BattleDome_FloorLightsNoBlend(u16 timer)
{
    CpuCopy16(sTilesetAnims_BattleDomeFloorLightPals[timer % ARRAY_COUNT(sTilesetAnims_BattleDomeFloorLightPals)], &gPlttBufferUnfaded[BG_PLTT_ID(8)], PLTT_SIZE_4BPP);
    if ((u8)FindTaskIdByFunc(Task_BattleTransition_Intro) == TASK_NONE)
    {
        BlendPalette(BG_PLTT_ID(8), 16, gPaletteFade.y, gPaletteFade.blendColor & 0x7FFF);
        if (!--sSecondaryTilesetAnimCounterMax)
            sSecondaryTilesetAnimCallback = NULL;
    }
}
