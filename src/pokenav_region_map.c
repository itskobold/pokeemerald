#include "global.h"
#include "bg.h"
#include "decompress.h"
#include "gpu_regs.h"
#include "main.h"
#include "menu.h"
#include "palette.h"
#include "pokenav.h"
#include "region_map.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "task.h"
#include "text_window.h"
#include "window.h"
#include "constants/rgb.h"
#include "constants/songs.h"
#include "constants/region_map_sections.h"

#define GFXTAG_HELP_BAR 7
#define PALTAG_HELP_BAR 12

// map_help_bar.png is 240x16 (30x2 tiles). Drawn as a row of 16x8 sprites in the
// bottom strip WIN0 reserves, since BG0/BG1 are unavailable in BG mode 2.
#define HELP_BAR_TILES_WIDE  30
#define HELP_BAR_ROWS        2
#define HELP_BAR_SPRITE_COLS (HELP_BAR_TILES_WIDE / 2)

#define REGION_MAP_BOTTOM_HIDDEN_TILES 2

struct Pokenav_RegionMapMenu
{
    u8 unused[12];
    bool32 zoomDisabled;
    u32 (*callback)(struct Pokenav_RegionMapMenu *);
};

struct Pokenav_RegionMapGfx
{
    bool32 (*isTaskActiveCB)(void);
    u32 loopTaskId;
    u8 ALIGNED(2) tilemapBuffer[BG_SCREEN_SIZE];
};

static u32 HandleRegionMapInput(struct Pokenav_RegionMapMenu *);
static u32 HandleRegionMapInputZoomDisabled(struct Pokenav_RegionMapMenu *);
static u32 GetExitRegionMapMenuId(struct Pokenav_RegionMapMenu *);
static u32 LoopedTask_OpenRegionMap(s32);
static bool32 GetCurrentLoopedTaskActive(void);
static void LoadPokenavRegionMapGfx(struct Pokenav_RegionMapGfx *);
static bool32 TryFreeTempTileDataBuffers(void);
static bool32 IsDma3ManagerBusyWithBgCopy_(struct Pokenav_RegionMapGfx *);
static void ChangeBgYForZoom(bool32);
static bool32 IsChangeBgYForZoomActive(void);
static void Task_ChangeBgYForZoom(u8 taskId);
static u32 LoopedTask_UpdateInfoAfterCursorMove(s32);
static u32 LoopedTask_RegionMapZoomOut(s32);
static u32 LoopedTask_RegionMapZoomIn(s32);
static u32 LoopedTask_ExitRegionMap(s32);
static void CreateRegionMapHelpBarSprites(void);
static void FreeRegionMapHelpBarSprites(void);

static const u32 sMapHelpBar_Gfx[] = INCGFX_U32("graphics/pokenav/left_headers/map_help_bar.png", ".4bpp.lz");
static const u16 sMapHelpBar_Pal[] = INCGFX_U16("graphics/pokenav/left_headers/map_help_bar.png", ".gbapal");

// The region map is rendered across two affine BGs (BG2 + BG3) so the combined
// tileset can exceed one affine BG's 256-tile cap. Both affine BGs use the same
// affine transform and full 64x64 map; the tiles are partitioned between them.
// IMPORTANT: the PokeNav main-menu header persists on BG0 at char block 0
// (gPokenavMainMenuBgTemplates: charBaseIndex 0, mapBaseIndex 5), so none of the
// region map's map/char blocks may touch char block 0.
// VRAM layout (no overlaps, char block 0 reserved for the BG0 header):
//   BG2 top    char block 2 (0x8000), map blocks 6-7   (0x3000)
//   BG3 bottom char block 3 (0xC000), map blocks 12-13 (0x6000)
//   BG1 UI     char block 1 (0x4000), map block 14     (0x7000)
static const struct BgTemplate sRegionMapBgTemplates[3] =
{
    {
        .bg = 1,
        .charBaseIndex = 1,
        .mapBaseIndex = 0x0E,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 1,
        .baseTile = 0
    },
    {
        .bg = 2,
        .charBaseIndex = 2,
        .mapBaseIndex = 0x06,
        .screenSize = 2,
        .paletteMode = 1,
        .priority = 2,
        .baseTile = 0
    },
    {
        .bg = 3,
        .charBaseIndex = 3,
        .mapBaseIndex = 0x0C,
        .screenSize = 2,
        .paletteMode = 1,
        .priority = 2,
        .baseTile = 0
    },
};

static const LoopedTask sRegionMapLoopTaskFuncs[] =
{
    [POKENAV_MAP_FUNC_NONE]         = NULL,
    [POKENAV_MAP_FUNC_CURSOR_MOVED] = LoopedTask_UpdateInfoAfterCursorMove,
    [POKENAV_MAP_FUNC_ZOOM_OUT]     = LoopedTask_RegionMapZoomOut,
    [POKENAV_MAP_FUNC_ZOOM_IN]      = LoopedTask_RegionMapZoomIn,
    [POKENAV_MAP_FUNC_EXIT]         = LoopedTask_ExitRegionMap
};

static const struct CompressedSpriteSheet sMapHelpBarSpriteSheet =
{
    sMapHelpBar_Gfx, HELP_BAR_TILES_WIDE * HELP_BAR_ROWS * TILE_SIZE_4BPP, GFXTAG_HELP_BAR
};

static const struct SpritePalette sMapHelpBarSpritePalette =
{
    sMapHelpBar_Pal, PALTAG_HELP_BAR
};

static const struct OamData sMapHelpBarOam =
{
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(16x8),
    .x = 0,
    .size = SPRITE_SIZE(16x8),
    .tileNum = 0,
    .priority = 0, // in front of the affine map (BG priority 2)
    .paletteNum = 0,
};

static const struct SpriteTemplate sMapHelpBarSpriteTemplate =
{
    .tileTag = GFXTAG_HELP_BAR,
    .paletteTag = PALTAG_HELP_BAR,
    .oam = &sMapHelpBarOam,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static u8 sMapHelpBarSpriteIds[HELP_BAR_ROWS * HELP_BAR_SPRITE_COLS];

u32 PokenavCallback_Init_RegionMap(void)
{
    struct Pokenav_RegionMapMenu *state = AllocSubstruct(POKENAV_SUBSTRUCT_REGION_MAP_STATE, sizeof(struct Pokenav_RegionMapMenu));
    if (!state)
        return FALSE;

    if (!AllocSubstruct(POKENAV_SUBSTRUCT_REGION_MAP, sizeof(struct RegionMap)))
        return FALSE;

    state->zoomDisabled = IsEventIslandMapSecId(gMapHeader.location.regionMapSectionId);
    if (!state->zoomDisabled)
        state->callback = HandleRegionMapInput;
    else
        state->callback = HandleRegionMapInputZoomDisabled;

    return TRUE;
}

void FreeRegionMapSubstruct1(void)
{
    gSaveBlock2Ptr->regionMapZoom = IsRegionMapZoomed();
    FreePokenavSubstruct(POKENAV_SUBSTRUCT_REGION_MAP);
    FreePokenavSubstruct(POKENAV_SUBSTRUCT_REGION_MAP_STATE);
}

u32 GetRegionMapCallback(void)
{
    struct Pokenav_RegionMapMenu *state = GetSubstructPtr(POKENAV_SUBSTRUCT_REGION_MAP_STATE);
    return state->callback(state);
}

static u32 HandleRegionMapInput(struct Pokenav_RegionMapMenu *state)
{
    switch (DoRegionMapInputCallback())
    {
    case MAP_INPUT_MOVE_END:
        return POKENAV_MAP_FUNC_CURSOR_MOVED;
    case MAP_INPUT_A_BUTTON:
        if (!IsRegionMapZoomed())
            return POKENAV_MAP_FUNC_ZOOM_IN;
        return POKENAV_MAP_FUNC_ZOOM_OUT;
    case MAP_INPUT_B_BUTTON:
        state->callback = GetExitRegionMapMenuId;
        return POKENAV_MAP_FUNC_EXIT;
    }

    return POKENAV_MAP_FUNC_NONE;
}

static u32 HandleRegionMapInputZoomDisabled(struct Pokenav_RegionMapMenu *state)
{
    if (JOY_NEW(B_BUTTON))
    {
        state->callback = GetExitRegionMapMenuId;
        return POKENAV_MAP_FUNC_EXIT;
    }

    return POKENAV_MAP_FUNC_NONE;
}

static u32 GetExitRegionMapMenuId(struct Pokenav_RegionMapMenu *state)
{
    return POKENAV_MAIN_MENU_CURSOR_ON_MAP;
}

bool32 GetZoomDisabled(void)
{
    struct Pokenav_RegionMapMenu *state = GetSubstructPtr(POKENAV_SUBSTRUCT_REGION_MAP_STATE);
    return state->zoomDisabled;
}

bool32 OpenPokenavRegionMap(void)
{
    struct Pokenav_RegionMapGfx *state = AllocSubstruct(POKENAV_SUBSTRUCT_REGION_MAP_ZOOM, sizeof(struct Pokenav_RegionMapGfx));
    if (!state)
        return FALSE;

    state->loopTaskId = CreateLoopedTask(LoopedTask_OpenRegionMap, 1);
    state->isTaskActiveCB = GetCurrentLoopedTaskActive;
    return TRUE;
}

void CreateRegionMapLoopedTask(s32 index)
{
    struct Pokenav_RegionMapGfx *state = GetSubstructPtr(POKENAV_SUBSTRUCT_REGION_MAP_ZOOM);
    state->loopTaskId = CreateLoopedTask(sRegionMapLoopTaskFuncs[index], 1);
    state->isTaskActiveCB = GetCurrentLoopedTaskActive;
}

bool32 IsRegionMapLoopedTaskActive(void)
{
    struct Pokenav_RegionMapGfx *state = GetSubstructPtr(POKENAV_SUBSTRUCT_REGION_MAP_ZOOM);
    return state->isTaskActiveCB();
}

void FreeRegionMapSubstruct2(void)
{
    FreeRegionMapIconResources();
    FreeRegionMapHelpBarSprites();
    FreePokenavSubstruct(POKENAV_SUBSTRUCT_REGION_MAP);
    FreePokenavSubstruct(POKENAV_SUBSTRUCT_REGION_MAP_ZOOM);
    SetPokenavVBlankCallback();
    SetBgMode(0);
}

static void VBlankCB_RegionMap(void)
{
    TransferPlttBuffer();
    LoadOam();
    ProcessSpriteCopyRequests();
    UpdateRegionMapVideoRegs();
}

static bool32 GetCurrentLoopedTaskActive(void)
{
    struct Pokenav_RegionMapGfx *state = GetSubstructPtr(POKENAV_SUBSTRUCT_REGION_MAP_ZOOM);
    return IsLoopedTaskActive(state->loopTaskId);
}

static bool8 ShouldOpenRegionMapZoomed(void)
{
    if (GetZoomDisabled())
        return FALSE;

    return gSaveBlock2Ptr->regionMapZoom == TRUE;
}

static u32 LoopedTask_OpenRegionMap(s32 taskState)
{
    struct RegionMap *regionMap;
    struct Pokenav_RegionMapGfx *state = GetSubstructPtr(POKENAV_SUBSTRUCT_REGION_MAP_ZOOM);
    switch (taskState)
    {
    case 0:
        SetVBlankCallback_(NULL);
        HideBg(1);
        HideBg(2);
        HideBg(3);
        SetBgMode(2); // BG2 and BG3 both affine
        InitBgTemplates(sRegionMapBgTemplates, ARRAY_COUNT(sRegionMapBgTemplates));
        regionMap = GetSubstructPtr(POKENAV_SUBSTRUCT_REGION_MAP);
        InitRegionMapData(regionMap, &sRegionMapBgTemplates[1], &sRegionMapBgTemplates[2], ShouldOpenRegionMapZoomed());
        return LT_INC_AND_PAUSE;
    case 1:
        if (LoadRegionMapGfx())
            return LT_PAUSE;

        if (!GetZoomDisabled())
        {
            CreateRegionMapPlayerIcon(4, 9);
            CreateRegionMapCursor(5, 10);
            TrySetPlayerIconBlink();
        }
        else
        {
            // Dim the region map when zoom is disabled
            // (when the player is off the map)
            BlendRegionMap(RGB_BLACK, 6);
        }
        CreateRegionMapHelpBarSprites();
        return LT_INC_AND_PAUSE;
    case 2:
        LoadPokenavRegionMapGfx(state);
        return LT_INC_AND_CONTINUE;
    case 3:
        if (TryFreeTempTileDataBuffers())
            return LT_PAUSE;

        FadeToBlackExceptPrimary();
        return LT_INC_AND_PAUSE;
    case 4:
        if (IsDma3ManagerBusyWithBgCopy_(state))
            return LT_PAUSE;

        ShowBg(1);
        ShowBg(2);
        ShowBg(3);
        // Clip the affine map out of the bottom strip with WIN0: inside the window
        // (top) shows the map + sprites; outside (bottom) shows only sprites/backdrop.
        SetGpuReg(REG_OFFSET_WIN0H, WIN_RANGE(0, DISPLAY_WIDTH));
        SetGpuReg(REG_OFFSET_WIN0V, WIN_RANGE(0, DISPLAY_HEIGHT - REGION_MAP_BOTTOM_HIDDEN_TILES * 8));
        SetGpuReg(REG_OFFSET_WININ, WININ_WIN0_BG2 | WININ_WIN0_BG3 | WININ_WIN0_OBJ | WININ_WIN0_CLR);
        SetGpuReg(REG_OFFSET_WINOUT, WINOUT_WIN01_OBJ | WINOUT_WIN01_CLR);
        SetGpuRegBits(REG_OFFSET_DISPCNT, DISPCNT_WIN0_ON);
        SetVBlankCallback_(VBlankCB_RegionMap);
        return LT_INC_AND_PAUSE;
    case 5:
        PokenavFadeScreen(POKENAV_FADE_FROM_BLACK);
        return LT_INC_AND_PAUSE;
    case 6:
        if (IsPaletteFadeActive() || AreLeftHeaderSpritesMoving())
            return LT_PAUSE;
        return LT_INC_AND_CONTINUE;
    default:
        return LT_FINISH;
    }
}

static u32 LoopedTask_UpdateInfoAfterCursorMove(s32 taskState)
{
    // Currently does nothing
    // Retained for future use

    struct Pokenav_RegionMapGfx *state = GetSubstructPtr(POKENAV_SUBSTRUCT_REGION_MAP_ZOOM);
    switch (taskState)
    {
    case 0:
        return LT_INC_AND_PAUSE;
    case 1:
        if (IsDma3ManagerBusyWithBgCopy_(state))
            return LT_PAUSE;
        break;
    }

    return LT_FINISH;
}

static u32 LoopedTask_RegionMapZoomOut(s32 taskState)
{
    switch (taskState)
    {
    case 0:
        PlaySE(SE_SELECT);
        ChangeBgYForZoom(FALSE);
        SetRegionMapDataForZoom();
        return LT_INC_AND_PAUSE;
    case 1:
        if (UpdateRegionMapZoom() || IsChangeBgYForZoomActive())
            return LT_PAUSE;
        return LT_INC_AND_PAUSE;
    case 2:
        if (WaitForHelpBar())
            return LT_PAUSE;

        UpdateRegionMapRightHeaderTiles(POKENAV_GFX_MAP_MENU_ZOOMED_OUT);
        break;
    }

    return LT_FINISH;
}

static u32 LoopedTask_RegionMapZoomIn(s32 taskState)
{
    struct Pokenav_RegionMapGfx *state = GetSubstructPtr(POKENAV_SUBSTRUCT_REGION_MAP_ZOOM);
    switch (taskState)
    {
    case 0:
        PlaySE(SE_SELECT);
        return LT_INC_AND_PAUSE;
    case 1:
        if (IsDma3ManagerBusyWithBgCopy_(state))
            return LT_PAUSE;

        ChangeBgYForZoom(TRUE);
        SetRegionMapDataForZoom();
        return LT_INC_AND_PAUSE;
    case 2:
        if (UpdateRegionMapZoom() || IsChangeBgYForZoomActive())
            return LT_PAUSE;
        return LT_INC_AND_PAUSE;
    case 3:
        if (WaitForHelpBar())
            return LT_PAUSE;

        UpdateRegionMapRightHeaderTiles(POKENAV_GFX_MAP_MENU_ZOOMED_IN);
        break;
    }

    return LT_FINISH;
}

static u32 LoopedTask_ExitRegionMap(s32 taskState)
{
    switch (taskState)
    {
    case 0:
        PlaySE(SE_SELECT);
        PokenavFadeScreen(POKENAV_FADE_TO_BLACK);
        return LT_INC_AND_PAUSE;
    case 1:
        if (IsPaletteFadeActive())
            return LT_PAUSE;

        SetLeftHeaderSpritesInvisibility();
        SlideMenuHeaderDown();
        return LT_INC_AND_PAUSE;
    case 2:
        if (MainMenuLoopedTaskIsBusy())
            return LT_PAUSE;

        ClearGpuRegBits(REG_OFFSET_DISPCNT, DISPCNT_WIN0_ON); // disable WIN0 for the main menu
        HideBg(1);
        HideBg(2);
        HideBg(3);
        return LT_INC_AND_PAUSE;
    }

    return LT_FINISH;
}

// Draws map_help_bar.png as a row of 16x8 sprites along the bottom strip that
// WIN0 clips the affine map out of. tileNum is set per sprite because the gfx is
// in raster tile order (each 16x8 sprite is two horizontally-adjacent tiles).
static void CreateRegionMapHelpBarSprites(void)
{
    u16 baseTile;
    u8 row, col, spriteId;
    u16 i = 0;
    s16 top = DISPLAY_HEIGHT - HELP_BAR_ROWS * 8;

    LoadCompressedSpriteSheet(&sMapHelpBarSpriteSheet);
    LoadSpritePalette(&sMapHelpBarSpritePalette);
    baseTile = GetSpriteTileStartByTag(GFXTAG_HELP_BAR);

    for (row = 0; row < HELP_BAR_ROWS; row++)
    {
        for (col = 0; col < HELP_BAR_SPRITE_COLS; col++)
        {
            spriteId = CreateSprite(&sMapHelpBarSpriteTemplate, col * 16 + 8, top + row * 8 + 4, 0);
            sMapHelpBarSpriteIds[i++] = spriteId;
            if (spriteId != MAX_SPRITES)
                gSprites[spriteId].oam.tileNum = baseTile + row * HELP_BAR_TILES_WIDE + col * 2;
        }
    }
}

static void FreeRegionMapHelpBarSprites(void)
{
    u16 i;
    for (i = 0; i < ARRAY_COUNT(sMapHelpBarSpriteIds); i++)
    {
        if (sMapHelpBarSpriteIds[i] != MAX_SPRITES)
            DestroySprite(&gSprites[sMapHelpBarSpriteIds[i]]);
    }
    FreeSpriteTilesByTag(GFXTAG_HELP_BAR);
    FreeSpritePaletteByTag(PALTAG_HELP_BAR);
}

static void LoadPokenavRegionMapGfx(struct Pokenav_RegionMapGfx *state)
{
    BgDmaFill(1, PIXEL_FILL(0), 0x40, 1);
    BgDmaFill(1, PIXEL_FILL(1), 0x41, 1);
    CpuFill16(0x1040, state->tilemapBuffer, 0x800);
    SetBgTilemapBuffer(1, state->tilemapBuffer);
    if (!IsRegionMapZoomed())
        ChangeBgY(1, -0x6000, BG_COORD_SET);
    else
        ChangeBgY(1, 0, BG_COORD_SET);

    ChangeBgX(1, 0, BG_COORD_SET);
}

static bool32 TryFreeTempTileDataBuffers(void)
{
    return FreeTempTileDataBuffersIfPossible();
}

static bool32 IsDma3ManagerBusyWithBgCopy_(struct Pokenav_RegionMapGfx *state)
{
    return IsDma3ManagerBusyWithBgCopy();
}

#define tZoomIn data[0]

static void ChangeBgYForZoom(bool32 zoomIn)
{
    u8 taskId = CreateTask(Task_ChangeBgYForZoom, 3);
    gTasks[taskId].tZoomIn = zoomIn;
}

static bool32 IsChangeBgYForZoomActive(void)
{
    return FuncIsActiveTask(Task_ChangeBgYForZoom);
}

static void Task_ChangeBgYForZoom(u8 taskId)
{
    if (gTasks[taskId].tZoomIn)
    {
        if (ChangeBgY(1, 0x480, BG_COORD_ADD) >= 0)
        {
            ChangeBgY(1, 0, BG_COORD_SET);
            DestroyTask(taskId);
        }
    }
    else
    {
        if (ChangeBgY(1, 0x480, BG_COORD_SUB) <= -0x6000)
        {
            ChangeBgY(1, -0x6000, BG_COORD_SET);
            DestroyTask(taskId);
        }
    }
}

#undef tZoomIn
