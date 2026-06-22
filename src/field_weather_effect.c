#include "global.h"
#include "battle_anim.h"
#include "event_object_movement.h"
#include "fieldmap.h"
#include "field_weather.h"
#include "overworld.h"
#include "palette.h"
#include "random.h"
#include "script.h"
#include "constants/weather.h"
#include "constants/biome.h"
#include "constants/map_types.h"
#include "constants/songs.h"
#include "sound.h"
#include "sprite.h"
#include "task.h"
#include "trig.h"
#include "gpu_regs.h"

const u16 gSandstormWeatherPalette[] = INCGFX_U16("graphics/weather/sandstorm.png", ".gbapal");

// The 9 cloud tiles tile together seamlessly as a 3x3 pattern. Stored as a 2D
// array so the images are contiguous and can be loaded as one sprite sheet;
// each 64x64 4bpp image is CLOUD_TILE_4BPP_SIZE bytes (64 hardware tiles).
#define CLOUD_TILE_4BPP_SIZE (64 * 64 / 2)
// One 3x3 set per non-empty cover level. Cover level N (1..15) uses set N-1 here (clouds/N); level 0 is
// "no clouds" and loads nothing (see ApplyCloudCover). The active set is chosen at load time by
// gWeatherPtr->cloudCover (see CreateCloudsSprites / ReloadCloudsTiles).
const u8 gWeatherCloudsTiles[CLOUD_TILE_SETS][CLOUD_TILE_IMAGES][CLOUD_TILE_4BPP_SIZE] = {
    {
        INCGFX_U8("graphics/weather/clouds/1/0.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/1/1.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/1/2.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/1/3.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/1/4.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/1/5.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/1/6.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/1/7.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/1/8.png", ".4bpp"),
    },
    {
        INCGFX_U8("graphics/weather/clouds/2/0.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/2/1.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/2/2.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/2/3.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/2/4.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/2/5.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/2/6.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/2/7.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/2/8.png", ".4bpp"),
    },
    {
        INCGFX_U8("graphics/weather/clouds/3/0.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/3/1.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/3/2.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/3/3.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/3/4.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/3/5.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/3/6.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/3/7.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/3/8.png", ".4bpp"),
    },
    {
        INCGFX_U8("graphics/weather/clouds/4/0.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/4/1.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/4/2.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/4/3.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/4/4.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/4/5.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/4/6.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/4/7.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/4/8.png", ".4bpp"),
    },
    {
        INCGFX_U8("graphics/weather/clouds/5/0.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/5/1.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/5/2.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/5/3.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/5/4.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/5/5.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/5/6.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/5/7.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/5/8.png", ".4bpp"),
    },
    {
        INCGFX_U8("graphics/weather/clouds/6/0.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/6/1.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/6/2.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/6/3.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/6/4.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/6/5.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/6/6.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/6/7.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/6/8.png", ".4bpp"),
    },
    {
        INCGFX_U8("graphics/weather/clouds/7/0.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/7/1.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/7/2.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/7/3.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/7/4.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/7/5.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/7/6.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/7/7.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/7/8.png", ".4bpp"),
    },
    {
        INCGFX_U8("graphics/weather/clouds/8/0.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/8/1.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/8/2.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/8/3.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/8/4.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/8/5.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/8/6.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/8/7.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/8/8.png", ".4bpp"),
    },
    {
        INCGFX_U8("graphics/weather/clouds/9/0.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/9/1.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/9/2.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/9/3.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/9/4.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/9/5.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/9/6.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/9/7.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/9/8.png", ".4bpp"),
    },
    {
        INCGFX_U8("graphics/weather/clouds/10/0.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/10/1.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/10/2.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/10/3.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/10/4.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/10/5.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/10/6.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/10/7.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/10/8.png", ".4bpp"),
    },
    {
        INCGFX_U8("graphics/weather/clouds/11/0.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/11/1.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/11/2.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/11/3.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/11/4.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/11/5.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/11/6.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/11/7.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/11/8.png", ".4bpp"),
    },
    {
        INCGFX_U8("graphics/weather/clouds/12/0.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/12/1.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/12/2.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/12/3.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/12/4.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/12/5.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/12/6.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/12/7.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/12/8.png", ".4bpp"),
    },
    {
        INCGFX_U8("graphics/weather/clouds/13/0.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/13/1.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/13/2.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/13/3.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/13/4.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/13/5.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/13/6.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/13/7.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/13/8.png", ".4bpp"),
    },
    {
        INCGFX_U8("graphics/weather/clouds/14/0.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/14/1.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/14/2.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/14/3.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/14/4.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/14/5.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/14/6.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/14/7.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/14/8.png", ".4bpp"),
    },
    {
        INCGFX_U8("graphics/weather/clouds/15/0.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/15/1.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/15/2.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/15/3.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/15/4.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/15/5.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/15/6.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/15/7.png", ".4bpp"),
        INCGFX_U8("graphics/weather/clouds/15/8.png", ".4bpp"),
    },
};
const u8 gWeatherFogHorizontalTiles[] = INCGFX_U8("graphics/weather/fog_horizontal.png", ".4bpp");
const u8 gWeatherSnow1Tiles[] = INCGFX_U8("graphics/weather/snow0.png", ".4bpp");
const u8 gWeatherSnow2Tiles[] = INCGFX_U8("graphics/weather/snow1.png", ".4bpp");
const u8 gWeatherBubbleTiles[] = INCGFX_U8("graphics/weather/bubble.png", ".4bpp");
const u8 gWeatherAshTiles[] = INCGFX_U8("graphics/weather/ash.png", ".4bpp");
const u8 gWeatherRainTiles[] = INCGFX_U8("graphics/weather/rain.png", ".4bpp");
const u8 gWeatherSandstormTiles[] = INCGFX_U8("graphics/weather/sandstorm.png", ".4bpp");

//------------------------------------------------------------------------------
// Weather blend controls
//------------------------------------------------------------------------------

// Controls for blending the fog weather effects
// Set to negative values for darkening, positive for lightening (suggested range [-2, 5])
#define UNDERWATER_BRIGHTNESS 2       // Horizontal fog under WEATHER_UNDERWATER_BUBBLES

#define WEATHER_BLEND_EFFECT(brightness) ((brightness) < 0 ? BLDCNT_EFFECT_DARKEN : BLDCNT_EFFECT_LIGHTEN)
#define WEATHER_BLEND_LEVEL(brightness)  ((brightness) < 0 ? -(brightness) : (brightness))

void Sunny_InitVars(void)
{
    gWeatherPtr->targetColorMapIndex = 0;
    gWeatherPtr->colorMapStepDelay = 20;
}

void Sunny_InitAll(void)
{
    Sunny_InitVars();
}

void Sunny_Main(void)
{
}

bool8 Sunny_Finish(void)
{
    return FALSE;
}

//------------------------------------------------------------------------------
// WEATHER_DROUGHT
//------------------------------------------------------------------------------

static void UpdateDroughtBlend(u8);

void Drought_InitVars(void)
{
    gWeatherPtr->initStep = 0;
    gWeatherPtr->weatherGfxLoaded = FALSE;
    gWeatherPtr->targetColorMapIndex = 0;
    gWeatherPtr->colorMapStepDelay = 0;
}

void Drought_InitAll(void)
{
    Drought_InitVars();
    while (gWeatherPtr->weatherGfxLoaded == FALSE)
        Drought_Main();
}

void Drought_Main(void)
{
    switch (gWeatherPtr->initStep)
    {
    case 0:
        if (gWeatherPtr->palProcessingState != WEATHER_PAL_STATE_CHANGING_WEATHER)
            gWeatherPtr->initStep++;
        break;
    case 1:
        ResetDroughtWeatherPaletteLoading();
        gWeatherPtr->initStep++;
        break;
    case 2:
        if (LoadDroughtWeatherPalettes() == FALSE)
            gWeatherPtr->initStep++;
        break;
    case 3:
        DroughtStateInit();
        gWeatherPtr->initStep++;
        break;
    case 4:
        DroughtStateRun();
        if (gWeatherPtr->droughtBrightnessStage == 6)
        {
            gWeatherPtr->weatherGfxLoaded = TRUE;
            gWeatherPtr->initStep++;
        }
        break;
    default:
        DroughtStateRun();
        break;
    }
}

bool8 Drought_Finish(void)
{
    return FALSE;
}

void StartDroughtWeatherBlend(void)
{
    CreateTask(UpdateDroughtBlend, 80);
}

#define tState      data[0]
#define tBlendY     data[1]
#define tBlendDelay data[2]
#define tWinRange   data[3]

static void UpdateDroughtBlend(u8 taskId)
{
    struct Task *task = &gTasks[taskId];

    switch (task->tState)
    {
    case 0:
        task->tBlendY = 0;
        task->tBlendDelay = 0;
        task->tWinRange = REG_WININ;
        SetGpuReg(REG_OFFSET_WININ, WININ_WIN0_ALL | WININ_WIN1_ALL);
        SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT1_BG1 | BLDCNT_TGT1_BG2 | BLDCNT_TGT1_BG3 | BLDCNT_TGT1_OBJ | BLDCNT_EFFECT_LIGHTEN);
        SetGpuReg(REG_OFFSET_BLDY, 0);
        task->tState++;
        // fall through
    case 1:
        task->tBlendY += 3;
        if (task->tBlendY > 16)
            task->tBlendY = 16;
        SetGpuReg(REG_OFFSET_BLDY, task->tBlendY);
        if (task->tBlendY >= 16)
            task->tState++;
        break;
    case 2:
        task->tBlendDelay++;
        if (task->tBlendDelay > 9)
        {
            task->tBlendDelay = 0;
            task->tBlendY--;
            if (task->tBlendY <= 0)
            {
                task->tBlendY = 0;
                task->tState++;
            }
            SetGpuReg(REG_OFFSET_BLDY, task->tBlendY);
        }
        break;
    case 3:
        SetGpuReg(REG_OFFSET_BLDCNT, 0);
        SetGpuReg(REG_OFFSET_BLDY, 0);
        SetGpuReg(REG_OFFSET_WININ, task->tWinRange);
        task->tState++;
        break;
    case 4:
        ScriptContext_Enable();
        DestroyTask(taskId);
        break;
    }
}

#undef tState
#undef tBlendY
#undef tBlendDelay
#undef tWinRange

//------------------------------------------------------------------------------
// WEATHER_RAIN
//------------------------------------------------------------------------------

static void LoadRainSpriteSheet(void);
static bool8 CreateRainSprite(void);
static void UpdateRainSprite(struct Sprite *sprite);
static bool8 UpdateVisibleRainSprites(void);
static void DestroyRainSprites(void);

static const struct Coords16 sRainSpriteCoords[] =
{
    {  0,   0},
    {  0, 160},
    {  0,  64},
    {144, 224},
    {144, 128},
    { 32,  32},
    { 32, 192},
    { 32,  96},
    { 72, 128},
    { 72,  32},
    { 72, 192},
    {216,  96},
    {216,   0},
    {104, 160},
    {104,  64},
    {104, 224},
    {144,   0},
    {144, 160},
    {144,  64},
    { 32, 224},
    { 32, 128},
    { 72,  32},
    { 72, 192},
    { 48,  96},
};

static const struct OamData sRainSpriteOamData =
{
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = FALSE,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(16x32),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(16x32),
    .tileNum = 0,
    .priority = 1,
    .paletteNum = 2,
    .affineParam = 0,
};

static const union AnimCmd sRainSpriteFallAnimCmd[] =
{
    ANIMCMD_FRAME(0, 16),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd sRainSpriteSplashAnimCmd[] =
{
    ANIMCMD_FRAME(8, 3),
    ANIMCMD_FRAME(32, 2),
    ANIMCMD_FRAME(40, 2),
    ANIMCMD_END,
};

static const union AnimCmd sRainSpriteHeavySplashAnimCmd[] =
{
    ANIMCMD_FRAME(8, 3),
    ANIMCMD_FRAME(16, 3),
    ANIMCMD_FRAME(24, 4),
    ANIMCMD_END,
};

static const union AnimCmd *const sRainSpriteAnimCmds[] =
{
    sRainSpriteFallAnimCmd,
    sRainSpriteSplashAnimCmd,
    sRainSpriteHeavySplashAnimCmd,
};

static const struct SpriteTemplate sRainSpriteTemplate =
{
    .tileTag = GFXTAG_RAIN,
    .paletteTag = PALTAG_WEATHER,
    .oam = &sRainSpriteOamData,
    .anims = sRainSpriteAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = UpdateRainSprite,
};

// Q28.4 fixed-point format values
static const s16 sRainSpriteMovement[][2] =
{
    {-0x68,  0xD0},
    {-0xA0, 0x140},
};

// First byte is the number of frames a raindrop falls before it splashes.
// Second byte is the maximum number of frames a raindrop can "wait" before
// it appears and starts falling. (This is only for the initial raindrop spawn.)
static const u16 sRainSpriteFallingDurations[][2] =
{
    {18, 7},
    {12, 10},
};

static const struct SpriteSheet sRainSpriteSheet =
{
    .data = gWeatherRainTiles,
    .size = sizeof(gWeatherRainTiles),
    .tag = GFXTAG_RAIN,
};

void Rain_InitVars(void)
{
    gWeatherPtr->initStep = 0;
    gWeatherPtr->weatherGfxLoaded = FALSE;
    gWeatherPtr->rainSpriteVisibleCounter = 0;
    gWeatherPtr->rainSpriteVisibleDelay = 8;
    gWeatherPtr->isDownpour = FALSE;
    gWeatherPtr->targetRainSpriteCount = 10;
    gWeatherPtr->targetColorMapIndex = 3;
    gWeatherPtr->colorMapStepDelay = 20;
    SetRainStrengthFromSoundEffect(SE_RAIN);
}

void Rain_InitAll(void)
{
    Rain_InitVars();
    while (!gWeatherPtr->weatherGfxLoaded)
        Rain_Main();
}

void Rain_Main(void)
{
    switch (gWeatherPtr->initStep)
    {
    case 0:
        LoadRainSpriteSheet();
        gWeatherPtr->initStep++;
        break;
    case 1:
        if (!CreateRainSprite())
            gWeatherPtr->initStep++;
        break;
    case 2:
        if (!UpdateVisibleRainSprites())
        {
            gWeatherPtr->weatherGfxLoaded = TRUE;
            gWeatherPtr->initStep++;
        }
        break;
    }
}

bool8 Rain_Finish(void)
{
    switch (gWeatherPtr->finishStep)
    {
    case 0:
        if (gWeatherPtr->nextWeather == WEATHER_RAIN
         || gWeatherPtr->nextWeather == WEATHER_RAIN_THUNDERSTORM
         || gWeatherPtr->nextWeather == WEATHER_DOWNPOUR)
        {
            gWeatherPtr->finishStep = 0xFF;
            return FALSE;
        }
        else
        {
            gWeatherPtr->targetRainSpriteCount = 0;
            gWeatherPtr->finishStep++;
        }
        // fall through
    case 1:
        if (!UpdateVisibleRainSprites())
        {
            DestroyRainSprites();
            gWeatherPtr->finishStep++;
            return FALSE;
        }
        return TRUE;
    }
    return FALSE;
}

#define tCounter data[0]
#define tRandom  data[1]
#define tPosX    data[2]
#define tPosY    data[3]
#define tState   data[4]
#define tActive  data[5]
#define tWaiting data[6]

static void StartRainSpriteFall(struct Sprite *sprite)
{
    u32 rand;
    u16 numFallingFrames;
    int tileX;
    int tileY;

    if (sprite->tRandom == 0)
        sprite->tRandom = 361;

    rand = ISO_RANDOMIZE2(sprite->tRandom);
    sprite->tRandom = ((rand & 0x7FFF0000) >> 16) % 600;

    numFallingFrames = sRainSpriteFallingDurations[gWeatherPtr->isDownpour][0];

    tileX = sprite->tRandom % 30;
    sprite->tPosX = tileX * 8; // Useless assignment, leftover from before fixed-point values were used

    tileY = sprite->tRandom / 30;
    sprite->tPosY = tileY * 8; // Useless assignment, leftover from before fixed-point values were used

    sprite->tPosX = tileX;
    sprite->tPosX <<= 7; // This is tileX * 8, using a fixed-point value with 4 decimal places

    sprite->tPosY = tileY;
    sprite->tPosY <<= 7; // This is tileX * 8, using a fixed-point value with 4 decimal places

    // "Rewind" the rain sprites, from their ending position.
    sprite->tPosX -= sRainSpriteMovement[gWeatherPtr->isDownpour][0] * numFallingFrames;
    sprite->tPosY -= sRainSpriteMovement[gWeatherPtr->isDownpour][1] * numFallingFrames;

    StartSpriteAnim(sprite, 0);
    sprite->tState = 0;
    sprite->coordOffsetEnabled = FALSE;
    sprite->tCounter = numFallingFrames;
}

static void UpdateRainSprite(struct Sprite *sprite)
{
    if (sprite->tState == 0)
    {
        // Raindrop is in its "falling" motion.
        sprite->tPosX += sRainSpriteMovement[gWeatherPtr->isDownpour][0];
        sprite->tPosY += sRainSpriteMovement[gWeatherPtr->isDownpour][1];
        sprite->x = sprite->tPosX >> 4;
        sprite->y = sprite->tPosY >> 4;

        if (sprite->tActive
         && (sprite->x >= -8 && sprite->x <= DISPLAY_WIDTH + 8)
         && sprite->y >= -16 && sprite->y <= DISPLAY_HEIGHT + 16)
            sprite->invisible = FALSE;
        else
            sprite->invisible = TRUE;

        if (--sprite->tCounter == 0)
        {
            // Make raindrop splash on the ground
            StartSpriteAnim(sprite, gWeatherPtr->isDownpour + 1);
            sprite->tState = 1;
            sprite->x -= gSpriteCoordOffsetX;
            sprite->y -= gSpriteCoordOffsetY;
            sprite->coordOffsetEnabled = TRUE;
        }
    }
    else if (sprite->animEnded)
    {
        // The splashing animation ended.
        sprite->invisible = TRUE;
        StartRainSpriteFall(sprite);
    }
}

static void WaitRainSprite(struct Sprite *sprite)
{
    if (sprite->tCounter == 0)
    {
        StartRainSpriteFall(sprite);
        sprite->callback = UpdateRainSprite;
    }
    else
    {
        sprite->tCounter--;
    }
}

static void InitRainSpriteMovement(struct Sprite *sprite, u16 val)
{
    u16 numFallingFrames = sRainSpriteFallingDurations[gWeatherPtr->isDownpour][0];
    u16 numAdvanceRng = val / (sRainSpriteFallingDurations[gWeatherPtr->isDownpour][1] + numFallingFrames);
    u16 frameVal = val % (sRainSpriteFallingDurations[gWeatherPtr->isDownpour][1] + numFallingFrames);

    while (--numAdvanceRng != 0xFFFF)
        StartRainSpriteFall(sprite);

    if (frameVal < numFallingFrames)
    {
        while (--frameVal != 0xFFFF)
            UpdateRainSprite(sprite);

        sprite->tWaiting = 0;
    }
    else
    {
        sprite->tCounter = frameVal - numFallingFrames;
        sprite->invisible = TRUE;
        sprite->tWaiting = 1;
    }
}

static void LoadRainSpriteSheet(void)
{
    LoadSpriteSheet(&sRainSpriteSheet);
}

static bool8 CreateRainSprite(void)
{
    u8 spriteIndex;
    u8 spriteId;

    if (gWeatherPtr->rainSpriteCount == MAX_RAIN_SPRITES)
        return FALSE;

    spriteIndex = gWeatherPtr->rainSpriteCount;
    spriteId = CreateSpriteAtEnd(&sRainSpriteTemplate,
      sRainSpriteCoords[spriteIndex].x, sRainSpriteCoords[spriteIndex].y, 78);

    if (spriteId != MAX_SPRITES)
    {
        gSprites[spriteId].tActive = FALSE;
        gSprites[spriteId].tRandom = spriteIndex * 145;
        while (gSprites[spriteId].tRandom >= 600)
            gSprites[spriteId].tRandom -= 600;

        StartRainSpriteFall(&gSprites[spriteId]);
        InitRainSpriteMovement(&gSprites[spriteId], spriteIndex * 9);
        gSprites[spriteId].invisible = TRUE;
        gWeatherPtr->sprites.s1.rainSprites[spriteIndex] = &gSprites[spriteId];
    }
    else
    {
        gWeatherPtr->sprites.s1.rainSprites[spriteIndex] = NULL;
    }

    if (++gWeatherPtr->rainSpriteCount == MAX_RAIN_SPRITES)
    {
        u16 i;
        for (i = 0; i < MAX_RAIN_SPRITES; i++)
        {
            if (gWeatherPtr->sprites.s1.rainSprites[i])
            {
                if (!gWeatherPtr->sprites.s1.rainSprites[i]->tWaiting)
                    gWeatherPtr->sprites.s1.rainSprites[i]->callback = UpdateRainSprite;
                else
                    gWeatherPtr->sprites.s1.rainSprites[i]->callback = WaitRainSprite;
            }
        }

        return FALSE;
    }

    return TRUE;
}

static bool8 UpdateVisibleRainSprites(void)
{
    if (gWeatherPtr->curRainSpriteIndex == gWeatherPtr->targetRainSpriteCount)
        return FALSE;

    if (++gWeatherPtr->rainSpriteVisibleCounter > gWeatherPtr->rainSpriteVisibleDelay)
    {
        gWeatherPtr->rainSpriteVisibleCounter = 0;
        if (gWeatherPtr->curRainSpriteIndex < gWeatherPtr->targetRainSpriteCount)
        {
            gWeatherPtr->sprites.s1.rainSprites[gWeatherPtr->curRainSpriteIndex++]->tActive = TRUE;
        }
        else
        {
            gWeatherPtr->curRainSpriteIndex--;
            gWeatherPtr->sprites.s1.rainSprites[gWeatherPtr->curRainSpriteIndex]->tActive = FALSE;
            gWeatherPtr->sprites.s1.rainSprites[gWeatherPtr->curRainSpriteIndex]->invisible = TRUE;
        }
    }
    return TRUE;
}

static void DestroyRainSprites(void)
{
    u16 i;

    for (i = 0; i < gWeatherPtr->rainSpriteCount; i++)
    {
        if (gWeatherPtr->sprites.s1.rainSprites[i] != NULL)
            DestroySprite(gWeatherPtr->sprites.s1.rainSprites[i]);
    }
    gWeatherPtr->rainSpriteCount = 0;
    FreeSpriteTilesByTag(GFXTAG_RAIN);
}

#undef tCounter
#undef tRandom
#undef tPosX
#undef tPosY
#undef tState
#undef tActive
#undef tWaiting

//------------------------------------------------------------------------------
// Snow
//------------------------------------------------------------------------------

static void UpdateSnowflakeSprite(struct Sprite *);
static bool8 UpdateVisibleSnowflakeSprites(void);
static bool8 CreateSnowflakeSprite(void);
static bool8 DestroySnowflakeSprite(void);
static void InitSnowflakeSpriteMovement(struct Sprite *);

void Snow_InitVars(void)
{
    gWeatherPtr->initStep = 0;
    gWeatherPtr->weatherGfxLoaded = FALSE;
    gWeatherPtr->targetColorMapIndex = 3;
    gWeatherPtr->colorMapStepDelay = 20;
    gWeatherPtr->targetSnowflakeSpriteCount = NUM_SNOWFLAKE_SPRITES;
    gWeatherPtr->snowflakeVisibleCounter = 0;
}

void Snow_InitAll(void)
{
    u16 i;

    Snow_InitVars();
    while (gWeatherPtr->weatherGfxLoaded == FALSE)
    {
        Snow_Main();
        for (i = 0; i < gWeatherPtr->snowflakeSpriteCount; i++)
            UpdateSnowflakeSprite(gWeatherPtr->sprites.s1.snowflakeSprites[i]);
    }
}

void Snow_Main(void)
{
    if (gWeatherPtr->initStep == 0 && !UpdateVisibleSnowflakeSprites())
    {
        gWeatherPtr->weatherGfxLoaded = TRUE;
        gWeatherPtr->initStep++;
    }
}

bool8 Snow_Finish(void)
{
    switch (gWeatherPtr->finishStep)
    {
    case 0:
        gWeatherPtr->targetSnowflakeSpriteCount = 0;
        gWeatherPtr->snowflakeVisibleCounter = 0;
        gWeatherPtr->finishStep++;
        // fall through
    case 1:
        if (!UpdateVisibleSnowflakeSprites())
        {
            gWeatherPtr->finishStep++;
            return FALSE;
        }
        return TRUE;
    }

    return FALSE;
}

static bool8 UpdateVisibleSnowflakeSprites(void)
{
    if (gWeatherPtr->snowflakeSpriteCount == gWeatherPtr->targetSnowflakeSpriteCount)
        return FALSE;

    if (++gWeatherPtr->snowflakeVisibleCounter > 36)
    {
        gWeatherPtr->snowflakeVisibleCounter = 0;
        if (gWeatherPtr->snowflakeSpriteCount < gWeatherPtr->targetSnowflakeSpriteCount)
            CreateSnowflakeSprite();
        else
            DestroySnowflakeSprite();
    }

    return gWeatherPtr->snowflakeSpriteCount != gWeatherPtr->targetSnowflakeSpriteCount;
}

static const struct OamData sSnowflakeSpriteOamData =
{
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = FALSE,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(8x8),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(8x8),
    .tileNum = 0,
    .priority = 1,
    .paletteNum = 0,
    .affineParam = 0,
};

static const struct SpriteFrameImage sSnowflakeSpriteImages[] =
{
    {gWeatherSnow1Tiles, sizeof(gWeatherSnow1Tiles)},
    {gWeatherSnow2Tiles, sizeof(gWeatherSnow2Tiles)},
};

static const union AnimCmd sSnowflakeAnimCmd0[] =
{
    ANIMCMD_FRAME(0, 16),
    ANIMCMD_END,
};

static const union AnimCmd sSnowflakeAnimCmd1[] =
{
    ANIMCMD_FRAME(1, 16),
    ANIMCMD_END,
};

static const union AnimCmd *const sSnowflakeAnimCmds[] =
{
    sSnowflakeAnimCmd0,
    sSnowflakeAnimCmd1,
};

static const struct SpriteTemplate sSnowflakeSpriteTemplate =
{
    .tileTag = TAG_NONE,
    .paletteTag = PALTAG_WEATHER,
    .oam = &sSnowflakeSpriteOamData,
    .anims = sSnowflakeAnimCmds,
    .images = sSnowflakeSpriteImages,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = UpdateSnowflakeSprite,
};

#define tPosY         data[0]
#define tDeltaY       data[1]
#define tWaveDelta    data[2]
#define tWaveIndex    data[3]
#define tSnowflakeId  data[4]
#define tFallCounter  data[5]
#define tFallDuration data[6]
#define tDeltaY2      data[7]

static bool8 CreateSnowflakeSprite(void)
{
    u8 spriteId = CreateSpriteAtEnd(&sSnowflakeSpriteTemplate, 0, 0, 78);
    if (spriteId == MAX_SPRITES)
        return FALSE;

    gSprites[spriteId].tSnowflakeId = gWeatherPtr->snowflakeSpriteCount;
    InitSnowflakeSpriteMovement(&gSprites[spriteId]);
    gSprites[spriteId].coordOffsetEnabled = TRUE;
    gWeatherPtr->sprites.s1.snowflakeSprites[gWeatherPtr->snowflakeSpriteCount++] = &gSprites[spriteId];
    return TRUE;
}

static bool8 DestroySnowflakeSprite(void)
{
    if (gWeatherPtr->snowflakeSpriteCount)
    {
        DestroySprite(gWeatherPtr->sprites.s1.snowflakeSprites[--gWeatherPtr->snowflakeSpriteCount]);
        return TRUE;
    }

    return FALSE;
}

static void InitSnowflakeSpriteMovement(struct Sprite *sprite)
{
    u16 rand;
    u16 x = ((sprite->tSnowflakeId * 5) & 7) * 30 + (Random() % 30);

    sprite->y = -3 - (gSpriteCoordOffsetY + sprite->centerToCornerVecY);
    sprite->x = x - (gSpriteCoordOffsetX + sprite->centerToCornerVecX);
    sprite->tPosY = sprite->y * 128;
    sprite->x2 = 0;
    rand = Random();
    sprite->tDeltaY = (rand & 3) * 5 + 64;
    sprite->tDeltaY2 = sprite->tDeltaY;
    StartSpriteAnim(sprite, (rand & 1) ? 0 : 1);
    sprite->tWaveIndex = 0;
    sprite->tWaveDelta = ((rand & 3) == 0) ? 2 : 1;
    sprite->tFallDuration = (rand & 0x1F) + 210;
    sprite->tFallCounter = 0;
}

static void WaitSnowflakeSprite(struct Sprite *sprite)
{
    // Timer is never incremented
    if (gWeatherPtr->snowflakeTimer > 18)
    {
        sprite->invisible = FALSE;
        sprite->callback = UpdateSnowflakeSprite;
        sprite->y = 250 - (gSpriteCoordOffsetY + sprite->centerToCornerVecY);
        sprite->tPosY = sprite->y * 128;
        gWeatherPtr->snowflakeTimer = 0;
    }
}

static void UpdateSnowflakeSprite(struct Sprite *sprite)
{
    s16 x;
    s16 y;

    sprite->tPosY += sprite->tDeltaY;
    sprite->y = sprite->tPosY >> 7;
    sprite->tWaveIndex += sprite->tWaveDelta;
    sprite->tWaveIndex &= 0xFF;
    sprite->x2 = gSineTable[sprite->tWaveIndex] / 64;

    x = (sprite->x + sprite->centerToCornerVecX + gSpriteCoordOffsetX) & 0x1FF;
    if (x & 0x100)
        x |= -0x100;

    if (x < -3)
        sprite->x = 242 - (gSpriteCoordOffsetX + sprite->centerToCornerVecX);
    else if (x > 242)
        sprite->x = -3 - (gSpriteCoordOffsetX + sprite->centerToCornerVecX);

    y = (sprite->y + sprite->centerToCornerVecY + gSpriteCoordOffsetY) & 0xFF;
    if (y > 163 && y < 171)
    {
        sprite->y = 250 - (gSpriteCoordOffsetY + sprite->centerToCornerVecY);
        sprite->tPosY = sprite->y * 128;
        sprite->tFallCounter = 0;
        sprite->tFallDuration = 220;
    }
    else if (y > 242 && y < 250)
    {
        sprite->y = 163;
        sprite->tPosY = sprite->y * 128;
        sprite->tFallCounter = 0;
        sprite->tFallDuration = 220;
        sprite->invisible = TRUE;
        sprite->callback = WaitSnowflakeSprite;
    }

    if (++sprite->tFallCounter == sprite->tFallDuration)
    {
        InitSnowflakeSpriteMovement(sprite);
        sprite->y = 250;
        sprite->invisible = TRUE;
        sprite->callback = WaitSnowflakeSprite;
    }
}

#undef tPosY
#undef tDeltaY
#undef tWaveDelta
#undef tWaveIndex
#undef tSnowflakeId
#undef tFallCounter
#undef tFallDuration
#undef tDeltaY2

//------------------------------------------------------------------------------
// WEATHER_RAIN_THUNDERSTORM
//------------------------------------------------------------------------------

enum {
    // This block of states is run only once
    // when first setting up the thunderstorm
    THUNDER_STATE_LOAD_RAIN,
    THUNDER_STATE_CREATE_RAIN,
    THUNDER_STATE_INIT_RAIN,
    THUNDER_STATE_WAIT_CHANGE,

    // The thunderstorm loops through these states,
    // not necessarily in order.
    THUNDER_STATE_NEW_CYCLE,
    THUNDER_STATE_NEW_CYCLE_WAIT,
    THUNDER_STATE_INIT_CYCLE_1,
    THUNDER_STATE_INIT_CYCLE_2,
    THUNDER_STATE_SHORT_BOLT,
    THUNDER_STATE_TRY_NEW_BOLT,
    THUNDER_STATE_WAIT_BOLT_SHORT,
    THUNDER_STATE_INIT_BOLT_LONG,
    THUNDER_STATE_WAIT_BOLT_LONG,
    THUNDER_STATE_FADE_BOLT_LONG,
    THUNDER_STATE_END_BOLT_LONG,
};

void Thunderstorm_InitVars(void)
{
    gWeatherPtr->initStep = THUNDER_STATE_LOAD_RAIN;
    gWeatherPtr->weatherGfxLoaded = FALSE;
    gWeatherPtr->rainSpriteVisibleCounter = 0;
    gWeatherPtr->rainSpriteVisibleDelay = 4;
    gWeatherPtr->isDownpour = FALSE;
    gWeatherPtr->targetRainSpriteCount = 16;
    gWeatherPtr->targetColorMapIndex = 3;
    gWeatherPtr->colorMapStepDelay = 20;
    gWeatherPtr->weatherGfxLoaded = FALSE;  // duplicate assignment
    gWeatherPtr->thunderEnqueued = FALSE;
    SetRainStrengthFromSoundEffect(SE_THUNDERSTORM);
}

void Thunderstorm_InitAll(void)
{
    Thunderstorm_InitVars();
    while (gWeatherPtr->weatherGfxLoaded == FALSE)
        Thunderstorm_Main();
}

//------------------------------------------------------------------------------
// WEATHER_DOWNPOUR
//------------------------------------------------------------------------------

static void UpdateThunderSound(void);
static void EnqueueThunder(u16);

void Downpour_InitVars(void)
{
    gWeatherPtr->initStep = THUNDER_STATE_LOAD_RAIN;
    gWeatherPtr->weatherGfxLoaded = FALSE;
    gWeatherPtr->rainSpriteVisibleCounter = 0;
    gWeatherPtr->rainSpriteVisibleDelay = 4;
    gWeatherPtr->isDownpour = TRUE;
    gWeatherPtr->targetRainSpriteCount = 24;
    gWeatherPtr->targetColorMapIndex = 3;
    gWeatherPtr->colorMapStepDelay = 20;
    gWeatherPtr->weatherGfxLoaded = FALSE;  // duplicate assignment
    SetRainStrengthFromSoundEffect(SE_DOWNPOUR);
}

void Downpour_InitAll(void)
{
    Downpour_InitVars();
    while (gWeatherPtr->weatherGfxLoaded == FALSE)
        Thunderstorm_Main();
}

// In a given cycle, there will be some shorter bolts of lightning, potentially
// followed by a longer bolt. As a "regex", the pattern is:
//   (SHORT_BOLT){1,2}(LONG_BOLT)?
//
// Thunder only plays on the final bolt of the cycle.
void Thunderstorm_Main(void)
{
    UpdateThunderSound();
    switch (gWeatherPtr->initStep)
    {
    case THUNDER_STATE_LOAD_RAIN:
        LoadRainSpriteSheet();
        gWeatherPtr->initStep++;
        break;
    case THUNDER_STATE_CREATE_RAIN:
        if (!CreateRainSprite())
            gWeatherPtr->initStep++;
        break;
    case THUNDER_STATE_INIT_RAIN:
        if (!UpdateVisibleRainSprites())
        {
            gWeatherPtr->weatherGfxLoaded = TRUE;
            gWeatherPtr->initStep++;
        }
        break;
    case THUNDER_STATE_WAIT_CHANGE:
        if (gWeatherPtr->palProcessingState != WEATHER_PAL_STATE_CHANGING_WEATHER)
            gWeatherPtr->initStep = THUNDER_STATE_INIT_CYCLE_1;
        break;
    case THUNDER_STATE_NEW_CYCLE:
        gWeatherPtr->thunderAllowEnd = TRUE;
        gWeatherPtr->thunderTimer = (Random() % 360) + 360;
        gWeatherPtr->initStep++;
        // fall through
    case THUNDER_STATE_NEW_CYCLE_WAIT:
        // Wait between 360-720 frames before starting a new cycle.
        if (--gWeatherPtr->thunderTimer == 0)
            gWeatherPtr->initStep++;
        break;
    case THUNDER_STATE_INIT_CYCLE_1:
        gWeatherPtr->thunderAllowEnd = TRUE;
        gWeatherPtr->thunderLongBolt = Random() % 2;
        gWeatherPtr->initStep++;
        break;
    case THUNDER_STATE_INIT_CYCLE_2:
        gWeatherPtr->thunderShortBolts = (Random() & 1) + 1;
        gWeatherPtr->initStep++;
        // fall through
    case THUNDER_STATE_SHORT_BOLT:
        // Short bolt of lightning strikes.
        ApplyWeatherColorMapIfIdle(19);
        // If final lightning bolt, enqueue thunder.
        if (!gWeatherPtr->thunderLongBolt && gWeatherPtr->thunderShortBolts == 1)
            EnqueueThunder(20);

        gWeatherPtr->thunderTimer = (Random() % 3) + 6;
        gWeatherPtr->initStep++;
        break;
    case THUNDER_STATE_TRY_NEW_BOLT:
        if (--gWeatherPtr->thunderTimer == 0)
        {
            // Short bolt of lightning ends.
            ApplyWeatherColorMapIfIdle(3);
            gWeatherPtr->thunderAllowEnd = TRUE;
            if (--gWeatherPtr->thunderShortBolts != 0)
            {
                // Wait a little, then do another short bolt.
                gWeatherPtr->thunderTimer = (Random() % 16) + 60;
                gWeatherPtr->initStep = THUNDER_STATE_WAIT_BOLT_SHORT;
            }
            else if (!gWeatherPtr->thunderLongBolt)
            {
                // No more bolts, restart loop.
                gWeatherPtr->initStep = THUNDER_STATE_NEW_CYCLE;
            }
            else
            {
                // Set up long bolt.
                gWeatherPtr->initStep = THUNDER_STATE_INIT_BOLT_LONG;
            }
        }
        break;
    case THUNDER_STATE_WAIT_BOLT_SHORT:
        if (--gWeatherPtr->thunderTimer == 0)
            gWeatherPtr->initStep = THUNDER_STATE_SHORT_BOLT;
        break;
    case THUNDER_STATE_INIT_BOLT_LONG:
        gWeatherPtr->thunderTimer = (Random() % 16) + 60;
        gWeatherPtr->initStep++;
        break;
    case THUNDER_STATE_WAIT_BOLT_LONG:
        if (--gWeatherPtr->thunderTimer == 0)
        {
            // Do long bolt. Enqueue thunder with a potentially longer delay.
            EnqueueThunder(100);
            ApplyWeatherColorMapIfIdle(19);
            gWeatherPtr->thunderTimer = (Random() & 0xF) + 30;
            gWeatherPtr->initStep++;
        }
        break;
    case THUNDER_STATE_FADE_BOLT_LONG:
        if (--gWeatherPtr->thunderTimer == 0)
        {
            // Fade long bolt out over time.
            ApplyWeatherColorMapIfIdle_Gradual(19, 3, 5);
            gWeatherPtr->initStep++;
        }
        break;
    case THUNDER_STATE_END_BOLT_LONG:
        if (gWeatherPtr->palProcessingState == WEATHER_PAL_STATE_IDLE)
        {
            gWeatherPtr->thunderAllowEnd = TRUE;
            gWeatherPtr->initStep = THUNDER_STATE_NEW_CYCLE;
        }
        break;
    }
}

bool8 Thunderstorm_Finish(void)
{
    switch (gWeatherPtr->finishStep)
    {
    case 0:
        gWeatherPtr->thunderAllowEnd = FALSE;
        gWeatherPtr->finishStep++;
        // fall through
    case 1:
        Thunderstorm_Main();
        if (gWeatherPtr->thunderAllowEnd)
        {
            if (gWeatherPtr->nextWeather == WEATHER_RAIN
             || gWeatherPtr->nextWeather == WEATHER_RAIN_THUNDERSTORM
             || gWeatherPtr->nextWeather == WEATHER_DOWNPOUR)
                return FALSE;

            gWeatherPtr->targetRainSpriteCount = 0;
            gWeatherPtr->finishStep++;
        }
        break;
    case 2:
        if (!UpdateVisibleRainSprites())
        {
            DestroyRainSprites();
            gWeatherPtr->thunderEnqueued = FALSE;
            gWeatherPtr->finishStep++;
            return FALSE;
        }
        break;
    default:
        return FALSE;
    }
    return TRUE;
}

// Enqueue a thunder sound effect for at most `waitFrames` frames from now.
static void EnqueueThunder(u16 waitFrames)
{
    if (!gWeatherPtr->thunderEnqueued)
    {
        gWeatherPtr->thunderSETimer = Random() % waitFrames;
        gWeatherPtr->thunderEnqueued = TRUE;
    }
}

static void UpdateThunderSound(void)
{
    if (gWeatherPtr->thunderEnqueued == TRUE)
    {
        if (gWeatherPtr->thunderSETimer == 0)
        {
            if (IsSEPlaying())
                return;

            if (Random() & 1)
                PlaySE(SE_THUNDER);
            else
                PlaySE(SE_THUNDER2);

            gWeatherPtr->thunderEnqueued = FALSE;
        }
        else
        {
            gWeatherPtr->thunderSETimer--;
        }
    }
}

//------------------------------------------------------------------------------
// Horizontal fog (used only by WEATHER_UNDERWATER_BUBBLES)
//------------------------------------------------------------------------------

static const u16 sUnusedData[] = {0, 6, 6, 12, 18, 42, 300, 300};

static u8 sFogHBlendY;

static const struct OamData sOamData_FogH =
{
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_WINDOW,
    .mosaic = FALSE,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(64x64),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(64x64),
    .tileNum = 0,
    .priority = 1,
    .paletteNum = 0,
    .affineParam = 0,
};

static const union AnimCmd sAnim_FogH_0[] =
{
    ANIMCMD_FRAME(0, 16),
    ANIMCMD_END,
};

static const union AnimCmd sAnim_FogH_1[] =
{
    ANIMCMD_FRAME(32, 16),
    ANIMCMD_END,
};

static const union AnimCmd sAnim_FogH_2[] =
{
    ANIMCMD_FRAME(64, 16),
    ANIMCMD_END,
};

static const union AnimCmd sAnim_FogH_3[] =
{
    ANIMCMD_FRAME(96, 16),
    ANIMCMD_END,
};

static const union AnimCmd sAnim_FogH_4[] =
{
    ANIMCMD_FRAME(128, 16),
    ANIMCMD_END,
};

static const union AnimCmd sAnim_FogH_5[] =
{
    ANIMCMD_FRAME(160, 16),
    ANIMCMD_END,
};

static const union AnimCmd *const sAnims_FogH[] =
{
    sAnim_FogH_0,
    sAnim_FogH_1,
    sAnim_FogH_2,
    sAnim_FogH_3,
    sAnim_FogH_4,
    sAnim_FogH_5,
};

static const union AffineAnimCmd sAffineAnim_FogH[] =
{
    AFFINEANIMCMD_FRAME(0x200, 0x200, 0, 0),
    AFFINEANIMCMD_END,
};

static const union AffineAnimCmd *const sAffineAnims_FogH[] =
{
    sAffineAnim_FogH,
};

static void FogHorizontalSpriteCallback(struct Sprite *);
static const struct SpriteTemplate sFogHorizontalSpriteTemplate =
{
    .tileTag = GFXTAG_FOG_H,
    .paletteTag = PALTAG_WEATHER,
    .oam = &sOamData_FogH,
    .anims = sAnims_FogH,
    .images = NULL,
    .affineAnims = sAffineAnims_FogH,
    .callback = FogHorizontalSpriteCallback,
};

void FogHorizontal_Main(void);
static void CreateFogHorizontalSprites(void);
static void DestroyFogHorizontalSprites(void);

void FogHorizontal_InitVars(void)
{
    gWeatherPtr->initStep = 0;
    gWeatherPtr->weatherGfxLoaded = FALSE;
    gWeatherPtr->targetColorMapIndex = 0;
    gWeatherPtr->colorMapStepDelay = 20;
    if (gWeatherPtr->fogHSpritesCreated == 0)
    {
        gWeatherPtr->fogHScrollCounter = 0;
        gWeatherPtr->fogHScrollOffset = 0;
        gWeatherPtr->fogHScrollPosX = 0;
    }
}

// The fog sprites are object-window sprites: they carve out moving regions in
// which the background is brightened, giving a soft cloud-like haze (no alpha).
void FogHorizontal_Main(void)
{
    gWeatherPtr->fogHScrollPosX = (gSpriteCoordOffsetX - gWeatherPtr->fogHScrollOffset) & 0xFF;
    if (++gWeatherPtr->fogHScrollCounter > 3)
    {
        gWeatherPtr->fogHScrollCounter = 0;
        gWeatherPtr->fogHScrollOffset++;
    }
    switch (gWeatherPtr->initStep)
    {
    case 0:
        CreateFogHorizontalSprites();
        sFogHBlendY = 0;
        SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT1_BG1 | BLDCNT_TGT1_BG2 | BLDCNT_TGT1_BG3
                                   | BLDCNT_TGT1_OBJ | WEATHER_BLEND_EFFECT(UNDERWATER_BRIGHTNESS));
        SetGpuReg(REG_OFFSET_BLDY, 0);
        SetGpuReg(REG_OFFSET_WINOUT, WINOUT_WIN01_BG_ALL | WINOUT_WIN01_OBJ
                                   | WINOUT_WINOBJ_BG_ALL | WINOUT_WINOBJ_OBJ | WINOUT_WINOBJ_CLR);
        ClearGpuRegBits(REG_OFFSET_DISPCNT, DISPCNT_WIN0_ON | DISPCNT_WIN1_ON);
        SetGpuRegBits(REG_OFFSET_DISPCNT, DISPCNT_OBJWIN_ON);
        gWeatherPtr->initStep++;
        break;
    case 1:
        if (sFogHBlendY < WEATHER_BLEND_LEVEL(UNDERWATER_BRIGHTNESS))
        {
            sFogHBlendY++;
            SetGpuReg(REG_OFFSET_BLDY, sFogHBlendY);
        }
        else
        {
            gWeatherPtr->weatherGfxLoaded = TRUE;
            gWeatherPtr->initStep++;
        }
        break;
    }
}

bool8 FogHorizontal_Finish(void)
{
    gWeatherPtr->fogHScrollPosX = (gSpriteCoordOffsetX - gWeatherPtr->fogHScrollOffset) & 0xFF;
    if (++gWeatherPtr->fogHScrollCounter > 3)
    {
        gWeatherPtr->fogHScrollCounter = 0;
        gWeatherPtr->fogHScrollOffset++;
    }

    switch (gWeatherPtr->finishStep)
    {
    case 0:
        if (sFogHBlendY != 0)
        {
            sFogHBlendY--;
            SetGpuReg(REG_OFFSET_BLDY, sFogHBlendY);
            return TRUE;
        }
        gWeatherPtr->finishStep++;
        // fall through
    case 1:
        DestroyFogHorizontalSprites();
        SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT2_BG1 | BLDCNT_TGT2_BG2 | BLDCNT_TGT2_BG3
                                   | BLDCNT_TGT2_OBJ | BLDCNT_EFFECT_BLEND);
        SetGpuReg(REG_OFFSET_BLDY, 0);
        SetGpuReg(REG_OFFSET_BLDALPHA, BLDALPHA_BLEND(13, 7));
        SetGpuReg(REG_OFFSET_WINOUT, WINOUT_WIN01_BG0 | WINOUT_WINOBJ_BG0);
        ClearGpuRegBits(REG_OFFSET_DISPCNT, DISPCNT_OBJWIN_ON);
        SetGpuRegBits(REG_OFFSET_DISPCNT, DISPCNT_WIN0_ON | DISPCNT_WIN1_ON);
        gWeatherPtr->finishStep++;
        return TRUE;
    }
    return FALSE;
}

#define tSpriteColumn data[0]

static void FogHorizontalSpriteCallback(struct Sprite *sprite)
{
    sprite->y2 = (u8)gSpriteCoordOffsetY;
    sprite->x = gWeatherPtr->fogHScrollPosX + 32 + sprite->tSpriteColumn * 64;
    if (sprite->x >= DISPLAY_WIDTH + 32)
    {
        sprite->x = (DISPLAY_WIDTH * 2) + gWeatherPtr->fogHScrollPosX - (4 - sprite->tSpriteColumn) * 64;
        sprite->x &= 0x1FF;
    }
}

static void CreateFogHorizontalSprites(void)
{
    u16 i;
    u8 spriteId;
    struct Sprite *sprite;

    if (!gWeatherPtr->fogHSpritesCreated)
    {
        struct SpriteSheet fogHorizontalSpriteSheet = {
            .data = gWeatherFogHorizontalTiles,
            .size = sizeof(gWeatherFogHorizontalTiles),
            .tag = GFXTAG_FOG_H,
        };
        LoadSpriteSheet(&fogHorizontalSpriteSheet);
        for (i = 0; i < NUM_FOG_HORIZONTAL_SPRITES; i++)
        {
            spriteId = CreateSpriteAtEnd(&sFogHorizontalSpriteTemplate, 0, 0, 0xFF);
            if (spriteId != MAX_SPRITES)
            {
                sprite = &gSprites[spriteId];
                sprite->tSpriteColumn = i % 5;
                sprite->x = (i % 5) * 64 + 32;
                sprite->y = (i / 5) * 64 + 32;
                gWeatherPtr->sprites.s2.fogHSprites[i] = sprite;
            }
            else
            {
                gWeatherPtr->sprites.s2.fogHSprites[i] = NULL;
            }
        }

        gWeatherPtr->fogHSpritesCreated = TRUE;
    }
}

static void DestroyFogHorizontalSprites(void)
{
    u16 i;

    if (gWeatherPtr->fogHSpritesCreated)
    {
        for (i = 0; i < NUM_FOG_HORIZONTAL_SPRITES; i++)
        {
            if (gWeatherPtr->sprites.s2.fogHSprites[i] != NULL)
                DestroySprite(gWeatherPtr->sprites.s2.fogHSprites[i]);
        }

        FreeSpriteTilesByTag(GFXTAG_FOG_H);
        gWeatherPtr->fogHSpritesCreated = 0;
    }
}

#undef tSpriteColumn

//------------------------------------------------------------------------------
// WEATHER_VOLCANIC_ASH
//------------------------------------------------------------------------------

static void LoadAshSpriteSheet(void);
static void CreateAshSprites(void);
static void DestroyAshSprites(void);
static void UpdateAshSprite(struct Sprite *);

void Ash_InitVars(void)
{
    gWeatherPtr->initStep = 0;
    gWeatherPtr->weatherGfxLoaded = FALSE;
    gWeatherPtr->targetColorMapIndex = 0;
    gWeatherPtr->colorMapStepDelay = 20;
    gWeatherPtr->ashUnused = 20; // Never read
}

void Ash_InitAll(void)
{
    Ash_InitVars();
    while (gWeatherPtr->weatherGfxLoaded == FALSE)
        Ash_Main();
}

void Ash_Main(void)
{
    gWeatherPtr->ashBaseSpritesX = gSpriteCoordOffsetX & 0x1FF;
    while (gWeatherPtr->ashBaseSpritesX >= DISPLAY_WIDTH)
        gWeatherPtr->ashBaseSpritesX -= DISPLAY_WIDTH;

    switch (gWeatherPtr->initStep)
    {
    case 0:
        LoadAshSpriteSheet();
        gWeatherPtr->initStep++;
        break;
    case 1:
        if (!gWeatherPtr->ashSpritesCreated)
            CreateAshSprites();
        gWeatherPtr->weatherGfxLoaded = TRUE;
        gWeatherPtr->initStep++;
        break;
    }
}

bool8 Ash_Finish(void)
{
    DestroyAshSprites();
    return FALSE;
}

static const struct SpriteSheet sAshSpriteSheet =
{
    .data = gWeatherAshTiles,
    .size = sizeof(gWeatherAshTiles),
    .tag = GFXTAG_ASH,
};

static void LoadAshSpriteSheet(void)
{
    LoadSpriteSheet(&sAshSpriteSheet);
}

static const struct OamData sAshSpriteOamData =
{
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(64x64),
    .x = 0,
    .size = SPRITE_SIZE(64x64),
    .tileNum = 0,
    .priority = 1,
    .paletteNum = 15,
};

static const union AnimCmd sAshSpriteAnimCmd0[] =
{
    ANIMCMD_FRAME(0, 60),
    ANIMCMD_FRAME(64, 60),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd *const sAshSpriteAnimCmds[] =
{
    sAshSpriteAnimCmd0,
};

static const struct SpriteTemplate sAshSpriteTemplate =
{
    .tileTag = GFXTAG_ASH,
    .paletteTag = PALTAG_WEATHER,
    .oam = &sAshSpriteOamData,
    .anims = sAshSpriteAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = UpdateAshSprite,
};

#define tOffsetY      data[0]
#define tCounterY     data[1]
#define tSpriteColumn data[2]
#define tSpriteRow    data[3]

static void CreateAshSprites(void)
{
    u8 i;
    u8 spriteId;
    struct Sprite *sprite;

    if (!gWeatherPtr->ashSpritesCreated)
    {
        for (i = 0; i < NUM_ASH_SPRITES; i++)
        {
            spriteId = CreateSpriteAtEnd(&sAshSpriteTemplate, 0, 0, 0x4E);
            if (spriteId != MAX_SPRITES)
            {
                sprite = &gSprites[spriteId];
                sprite->tCounterY = 0;
                sprite->tSpriteColumn = (u8)(i % 5);
                sprite->tSpriteRow = (u8)(i / 5);
                sprite->tOffsetY = sprite->tSpriteRow * 64 + 32;
                gWeatherPtr->sprites.s2.ashSprites[i] = sprite;
            }
            else
            {
                gWeatherPtr->sprites.s2.ashSprites[i] = NULL;
            }
        }

        gWeatherPtr->ashSpritesCreated = TRUE;
    }
}

static void DestroyAshSprites(void)
{
    u16 i;

    if (gWeatherPtr->ashSpritesCreated)
    {
        for (i = 0; i < NUM_ASH_SPRITES; i++)
        {
            if (gWeatherPtr->sprites.s2.ashSprites[i] != NULL)
                DestroySprite(gWeatherPtr->sprites.s2.ashSprites[i]);
        }

        FreeSpriteTilesByTag(GFXTAG_ASH);
        gWeatherPtr->ashSpritesCreated = FALSE;
    }
}

static void UpdateAshSprite(struct Sprite *sprite)
{
    if (++sprite->tCounterY > 5)
    {
        sprite->tCounterY = 0;
        sprite->tOffsetY++;
    }

    sprite->y = gSpriteCoordOffsetY + sprite->tOffsetY;
    sprite->x = gWeatherPtr->ashBaseSpritesX + 32 + sprite->tSpriteColumn * 64;
    if (sprite->x >= DISPLAY_WIDTH + 32)
    {
        sprite->x = gWeatherPtr->ashBaseSpritesX + (DISPLAY_WIDTH * 2) - (4 - sprite->tSpriteColumn) * 64;
        sprite->x &= 0x1FF;
    }
}

#undef tOffsetY
#undef tCounterY
#undef tSpriteColumn
#undef tSpriteRow

//------------------------------------------------------------------------------
// Clouds (persistent overlay, always on except for a few weathers)
//------------------------------------------------------------------------------

#define CLOUD_TILE          64                              // size of one cloud tile (px)
#define CLOUD_PARALLAX_NUM  1                                // clouds track camera at NUM/DEN (1/32) speed
#define CLOUD_PARALLAX_DEN  32
#define CLOUD_IMAGE_TILES   (CLOUD_TILE_4BPP_SIZE / TILE_SIZE_4BPP) // hardware tiles per image
#define CLOUD_PATTERN_PX    (CLOUD_PATTERN_DIM * CLOUD_TILE)        // pattern period (px)
// The leftmost column can sit as high as x=-1, leaving no margin past the screen's left edge, so the
// sway/ripple wobble (x2) occasionally nudges a row right and exposes a 1px seam there. Shift the whole
// field left by this buffer; every tile moves together so the field stays seamless, and the spare 5th
// column keeps the right edge covered.
#define CLOUD_LEFT_BUFFER   1   // px of extra coverage past the left screen edge

// Subtle wobble layered on top of the drift to keep the field from looking frozen. Two pieces, both
// driven by cloudsWavePhase (one unit/frame) and applied via sprite->x2/y2 so they nudge position only,
// never the tile-image selection. SWAY is a slow circle the whole field rides together (perfectly
// seamless). RIPPLE is a horizontal shimmer whose phase tracks screen-Y, so horizontal neighbours share
// an offset (seam stays shut) while vertical neighbours differ only ~0.1px at this wavelength - reads as
// a watery ripple over the fuzzy clouds. Amplitudes are small to keep seams invisible.
#define CLOUD_SWAY_AMP      4   // px, whole-field circular sway (halved phase rate => ~8.5s/cycle)
#define CLOUD_RIPPLE_AMP    3   // px, horizontal ripple displacement
#define CLOUD_RIPPLE_SHIFT  4   // screen-Y -> phase shift; larger = longer vertical wavelength

static void UpdateCloudsMovement(void);
static void UpdateCloudCover(void);
static void UpdateCloudBrightness(void);
static bool32 StepCloudCover(u8 target);
static bool32 StepCloudBrightness(s8 target);
static void UnpauseClouds(void);
static void ReleaseCloudBlend(void);
static void UpdateCloudPauseTrigger(bool32 silhouettesActive);
static void ApplyCloudBrightness(void);
static void ApplyCliffSilhouetteBlend(void);
static void ApplyCloudCover(void);
static void ReloadCloudsTiles(void);
static void CreateCloudsSprites(void);
static void DestroyCloudsSprites(void);
static void UpdateCloudsSprite(struct Sprite *);

static bool8 sCloudsShown;
static u8 sCloudBlendFadeY; // last screen-fade level the cloud blend was applied at (see UpdateClouds)

// Clouds are a persistent overlay drawn on top of most weathers. They are
// suppressed for weathers that need the blend registers for themselves.
static bool8 CloudsActiveForWeather(u8 weather)
{
    switch (weather)
    {
    case WEATHER_NONE:
    case WEATHER_DROUGHT:
    case WEATHER_UNDERWATER_BUBBLES:
        return FALSE;
    default:
        return TRUE;
    }
}

// The overlay only makes sense on open-air overworld maps, so it is limited to
// the overworld biome group and outdoor location types.
static bool8 CloudsAllowedOnCurrentMap(void)
{
    if (gMapHeader.biomeGroup != BIOME_GROUP_OVERWORLD)
        return FALSE;

    switch (GetCurrentMapType())
    {
    case MAP_TYPE_TOWN:
    case MAP_TYPE_CITY:
    case MAP_TYPE_ROUTE:
    case MAP_TYPE_OCEAN_ROUTE:
        return TRUE;
    default:
        return FALSE;
    }
}

// The sprites persist across weathers, but they must only carve the object
// window while the overlay is active - otherwise they would leak into another
// weather that uses the object window itself (e.g. underwater bubbles).
static void SetCloudsSpritesInvisible(bool8 invisible)
{
    u16 i;

    for (i = 0; i < NUM_CLOUD_SPRITES; i++)
    {
        if (gWeatherPtr->sprites.s2.cloudSprites[i] != NULL)
            gWeatherPtr->sprites.s2.cloudSprites[i]->invisible = invisible;
    }
}

// Bring the cloud sprites in line with the current cover level. Level 0 means "no clouds": the sprites
// are freed entirely so nothing is drawn or carves the object window (this is what lets the cliff
// silhouettes own the window unobstructed). The sprite sheet itself stays allocated for the whole map
// session: freeing and reallocating it mid-session fragments OBJ VRAM and corrupts the reload once other
// sprites (e.g. silhouetted NPCs that come on-screen behind the cliff) grab the freed tile range.
static void ApplyCloudCover(void)
{
    if (gWeatherPtr->cloudCover != 0)
        ReloadCloudsTiles(); // swap this level's set into the persistent sheet
    // Hidden whenever the overlay is off OR cover is 0, so at level 0 they carve nothing.
    SetCloudsSpritesInvisible(!(sCloudsShown && gWeatherPtr->cloudCover != 0));
}

// Screen fades (map transitions, battle entry, etc.) blend the palettes toward black/white in software,
// but the cloud overlay's brightening/darkening is a hardware BLDY blend applied after the palette lookup,
// so it ignores the fade - its cloud-shaped patches would stay lit while the rest of the screen goes dark.
// Scale the BLDY level down by the screen-fade progress so the overlay fades out (and back in) with the
// screen. gPaletteFade.y is the current blend coefficient: 0 = no fade, 16 = fully faded.
static u8 FadeScaledBlendLevel(u8 level)
{
    u32 fade = gPaletteFade.y;

    if (fade >= 16)
        return 0;
    return level * (16 - fade) / 16;
}

// Push the current overlay brightness into the blend registers.
static void ApplyCloudBrightness(void)
{
    s8 brightness = gWeatherPtr->cloudBrightness;

    SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT1_BG1 | BLDCNT_TGT1_BG2 | BLDCNT_TGT1_BG3
                               | BLDCNT_TGT1_OBJ | WEATHER_BLEND_EFFECT(brightness));
    SetGpuReg(REG_OFFSET_BLDY, FadeScaledBlendLevel(WEATHER_BLEND_LEVEL(brightness)));
}

// Max darkening magnitude applied to the silhouettes of cliff-buried objects, and the per-frame ramp
// toward it (see UpdateCliffSilhouetteBrightness). Stored as a positive magnitude, applied as negative.
#define CLIFF_SILHOUETTE_BRIGHTNESS_MAX 6
#define CLIFF_SILHOUETTE_STEP 1

// The buried player has hidden the clouds, so the overlay's object window is free: reuse it to darken
// the BG wherever a buried object's sprite carves it (those sprites are switched to object-window mode
// in UpdateObjectEventSpriteVisibility), painting each as a flat dark silhouette of itself. The darkening
// ramps in/out (see UpdateCliffSilhouetteBrightness) rather than snapping to full.
static void ApplyCliffSilhouetteBlend(void)
{
    s8 brightness = -gWeatherPtr->cliffSilhouetteBrightness;

    SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT1_BG1 | BLDCNT_TGT1_BG2 | BLDCNT_TGT1_BG3
                               | BLDCNT_TGT1_OBJ | WEATHER_BLEND_EFFECT(brightness));
    SetGpuReg(REG_OFFSET_BLDY, FadeScaledBlendLevel(WEATHER_BLEND_LEVEL(brightness)));
}

// Step the silhouette darkening one notch (CLIFF_SILHOUETTE_STEP) toward its max while shown, or back to
// 0 while hidden, so the silhouettes fade in as the player buries and fade out before the clouds return.
static void UpdateCliffSilhouetteBrightness(bool32 show)
{
    s32 level = gWeatherPtr->cliffSilhouetteBrightness;
    s32 target = show ? CLIFF_SILHOUETTE_BRIGHTNESS_MAX : 0;

    if (level < target)
        level = min(level + CLIFF_SILHOUETTE_STEP, target);
    else if (level > target)
        level = max(level - CLIFF_SILHOUETTE_STEP, target);

    gWeatherPtr->cliffSilhouetteBrightness = level;
}

// Clouds tear down the same frame their cover reaches 0, but their sprites' OAM isn't cleared until the
// next sprite-system pass. Wait this many fully-cleared frames before lighting the silhouettes so the
// darkening never lands on that lingering final cloud frame (which read as a 1-frame overlap).
#define CLOUD_CLEARED_SETTLE_FRAMES 2

// Whether the silhouette ramp should be driving toward full this frame: the player itself is fully buried
// in a cliff and the clouds have been fully faded out (see cloudClearedTimer) for long enough to settle,
// so the overlay's blend/object-window machinery is idle and free for the silhouettes to borrow.
static bool32 ShouldRampUpCliffSilhouettes(void)
{
    struct ObjectEvent *player = &gObjectEvents[gPlayerAvatar.objectEventId];

    return player->active
        && player->cliffLayer == CLIFF_LAYER_OBSCURED
        && gWeatherPtr->cloudClearedTimer >= CLOUD_CLEARED_SETTLE_FRAMES;
}

// Whether buried-object silhouettes are visible this frame: true the whole time the darkening is non-zero,
// so the buried sprites stay in object-window mode through both the ramp-in and the ramp-out fade.
bool32 ShouldDrawCliffSilhouettes(void)
{
    return gWeatherPtr->cliffSilhouetteBrightness > 0;
}

static void ShowClouds(void)
{
    SetCloudsSpritesInvisible(gWeatherPtr->cloudCover == 0); // at level 0 they stay hidden (carve nothing)
    ApplyCloudBrightness();
    SetGpuReg(REG_OFFSET_WINOUT, WINOUT_WIN01_BG_ALL | WINOUT_WIN01_OBJ
                               | WINOUT_WINOBJ_BG_ALL | WINOUT_WINOBJ_OBJ | WINOUT_WINOBJ_CLR);
    ClearGpuRegBits(REG_OFFSET_DISPCNT, DISPCNT_WIN0_ON | DISPCNT_WIN1_ON);
    SetGpuRegBits(REG_OFFSET_DISPCNT, DISPCNT_OBJWIN_ON);
}

static void HideClouds(void)
{
    SetCloudsSpritesInvisible(TRUE);
    SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT2_BG1 | BLDCNT_TGT2_BG2 | BLDCNT_TGT2_BG3
                               | BLDCNT_TGT2_OBJ | BLDCNT_EFFECT_BLEND);
    SetGpuReg(REG_OFFSET_BLDY, 0);
    SetGpuReg(REG_OFFSET_BLDALPHA, BLDALPHA_BLEND(13, 7));
    SetGpuReg(REG_OFFSET_WINOUT, WINOUT_WIN01_BG0 | WINOUT_WINOBJ_BG0);
    ClearGpuRegBits(REG_OFFSET_DISPCNT, DISPCNT_OBJWIN_ON);
    SetGpuRegBits(REG_OFFSET_DISPCNT, DISPCNT_WIN0_ON | DISPCNT_WIN1_ON);
}

// Called once from StartWeather. The sprites persist for the life of the
// weather task (one map session) and stay invisible until shown.
void InitClouds(void)
{
    sCloudsShown = FALSE;
    sCloudBlendFadeY = gPaletteFade.y;
    gWeatherPtr->cloudCover = gSaveBlock1Ptr->weatherState.cloudCover; // persists across map transitions
    gWeatherPtr->targetCloudCover = gSaveBlock1Ptr->weatherState.cloudCover;
    gWeatherPtr->cloudCoverTimer = 0;
    gWeatherPtr->cloudBrightness = FromCloudBrightnessRaw(gSaveBlock1Ptr->weatherState.cloudBrightness);
    gWeatherPtr->targetCloudBrightness = gWeatherPtr->cloudBrightness;
    gWeatherPtr->cloudBrightnessTimer = 0;
    gWeatherPtr->pauseClouds = FALSE;
    gWeatherPtr->unpauseClouds = FALSE;
    gWeatherPtr->externalPauseClouds = FALSE; // also clears any battle-entry pause when returning to the field
    gWeatherPtr->cloudClearedTimer = 0;
    gWeatherPtr->cliffSilhouetteBrightness = 0;
    gWeatherPtr->cloudsScrollXCounter = 0;
    gWeatherPtr->cloudsScrollYCounter = 0;
    gWeatherPtr->cloudsXOffset = 0;
    gWeatherPtr->cloudsYOffset = 0;
    gWeatherPtr->cloudsWavePhase = 0;
    CreateCloudsSprites(); // allocate the persistent sheet + sprites once for the whole session
    ApplyCloudCover();     // swap in the saved level's tiles; stays hidden until an active weather shows it
}

// Called every frame from the weather task to toggle the overlay by weather.
void UpdateClouds(void)
{
    bool8 cloudsActive = CloudsActiveForWeather(gWeatherPtr->currWeather)
                      && CloudsAllowedOnCurrentMap();
    bool8 silhouettes;
    bool8 active;

    // Ramp the silhouette darkening toward full while the buried player has cleared the clouds, and back
    // out otherwise (CLIFF_SILHOUETTE_STEP per frame). silhouettes stays set through the ramp-out so the
    // clouds wait for the fade to finish before they return (see UpdateCloudPauseTrigger).
    UpdateCliffSilhouetteBrightness(ShouldRampUpCliffSilhouettes());
    silhouettes = ShouldDrawCliffSilhouettes();

    UpdateCloudPauseTrigger(silhouettes);

    // While paused (player buried in a cliff), cover & brightness fade to 0. When the pause lifts they
    // fade back to their targets with the inverse motion (one step per frame, see UnpauseClouds). Only
    // once fully restored do the normal 20-frame steppers take back over.
    if (gWeatherPtr->pauseClouds)
        PauseClouds();
    else if (gWeatherPtr->unpauseClouds)
        UnpauseClouds();
    else
    {
        UpdateCloudCover();
        UpdateCloudBrightness();
    }

    // Count frames since the clouds finished fading out so the silhouettes can wait for the last cloud
    // frame's OAM to clear (see CLOUD_CLEARED_SETTLE_FRAMES). Reset the moment they're not fully cleared.
    if (gWeatherPtr->pauseClouds && gWeatherPtr->cloudCover == 0 && gWeatherPtr->cloudBrightness == 0)
    {
        if (gWeatherPtr->cloudClearedTimer < 255)
            gWeatherPtr->cloudClearedTimer++;
    }
    else
    {
        gWeatherPtr->cloudClearedTimer = 0;
    }

    // Once the clouds have fully faded behind the buried player, keep the object-window overlay up (even
    // on maps with no cloud weather) so the buried-object silhouettes have a window to carve.
    active = cloudsActive || silhouettes;

    // An external pause (field move / battle entry) instead drops the overlay entirely once it has faded
    // out, handing its window & blend registers back to the effect that asked for them (HideClouds below).
    if (gWeatherPtr->externalPauseClouds && gWeatherPtr->cloudCover == 0 && gWeatherPtr->cloudBrightness == 0)
        active = FALSE;

    if (cloudsActive)
        UpdateCloudsMovement();

    if (active && !sCloudsShown)
    {
        ShowClouds();
        sCloudsShown = TRUE;
    }
    else if (!active && sCloudsShown)
    {
        HideClouds();
        sCloudsShown = FALSE;
    }

    // Override the (released) cloud blend with the ramped silhouette darkening while buried. Both blend
    // appliers scale by the screen-fade progress (see FadeScaledBlendLevel); the silhouette path already
    // reapplies every frame, so a fade in/out is tracked there for free. The cloud path is only reapplied
    // on a brightness step, so refresh it whenever the fade level moves so the overlay fades with the
    // screen rather than hanging as lit patches over a black/white screen.
    if (silhouettes)
        ApplyCliffSilhouetteBlend();
    else if (sCloudsShown && gPaletteFade.y != sCloudBlendFadeY)
        ApplyCloudBrightness();

    sCloudBlendFadeY = gPaletteFade.y;
}

// Drift the clouds diagonally (down-left). The offsets wrap at one full pattern
// period so the wrap lands on an identical phase and stays seamless.
static void UpdateCloudsMovement(void)
{
    if (++gWeatherPtr->cloudsScrollXCounter > 2)
    {
        gWeatherPtr->cloudsScrollXCounter = 0;
        if (++gWeatherPtr->cloudsXOffset >= CLOUD_PATTERN_PX)
            gWeatherPtr->cloudsXOffset -= CLOUD_PATTERN_PX;
    }

    if (++gWeatherPtr->cloudsScrollYCounter > 4)
    {
        gWeatherPtr->cloudsScrollYCounter = 0;
        if (++gWeatherPtr->cloudsYOffset >= CLOUD_PATTERN_PX)
            gWeatherPtr->cloudsYOffset -= CLOUD_PATTERN_PX;
    }

    gWeatherPtr->cloudsWavePhase++; // drives the sway/ripple wobble (see UpdateCloudsSprite)
}

// Request a target cloud cover; the overlay steps toward it (see UpdateCloudCover).
void SetCloudCover(u8 target)
{
    if (target >= CLOUD_COVER_SETS)
        target = CLOUD_COVER_SETS - 1;
    gWeatherPtr->targetCloudCover = target;
}

// Move the current cover one level toward target, swapping in the new tile set and persisting the value.
// Returns TRUE if a step was taken (i.e. it was not already at the target).
static bool32 StepCloudCover(u8 target)
{
    if (gWeatherPtr->cloudCover == target)
        return FALSE;

    if (gWeatherPtr->cloudCover < target)
        gWeatherPtr->cloudCover++;
    else
        gWeatherPtr->cloudCover--;

    gSaveBlock1Ptr->weatherState.cloudCover = gWeatherPtr->cloudCover; // persist current level
    ApplyCloudCover(); // (un)load sprites as the level crosses 0, else just swap the tiles
    return TRUE;
}

// Step the current cloud cover one level toward the target every 20 frames.
static void UpdateCloudCover(void)
{
    if (gWeatherPtr->cloudCover == gWeatherPtr->targetCloudCover)
        return;

    if (++gWeatherPtr->cloudCoverTimer < 20)
        return;
    gWeatherPtr->cloudCoverTimer = 0;

    StepCloudCover(gWeatherPtr->targetCloudCover);
}

// Request a target overlay brightness; it is stepped toward (see UpdateCloudBrightness).
// Stage 0 is skipped, so 0 is never a valid target.
void SetCloudBrightness(s8 target)
{
    if (target < CLOUD_BRIGHTNESS_MIN)
        target = CLOUD_BRIGHTNESS_MIN;
    else if (target > CLOUD_BRIGHTNESS_MAX)
        target = CLOUD_BRIGHTNESS_MAX;
    if (target == 0)
        target = 1;
    gWeatherPtr->targetCloudBrightness = target;
}

// Move the brightness one level toward target, persisting and reapplying it. Stage 0 is skipped: +1
// jumps straight to -1 and back. Returns TRUE if a step was taken (targets are never 0, so this can't
// be asked to land on 0; PauseClouds drives to 0 by its own path).
static bool32 StepCloudBrightness(s8 target)
{
    if (gWeatherPtr->cloudBrightness == target)
        return FALSE;

    if (gWeatherPtr->cloudBrightness < target)
    {
        if (++gWeatherPtr->cloudBrightness == 0)
            gWeatherPtr->cloudBrightness = 1;
    }
    else
    {
        if (--gWeatherPtr->cloudBrightness == 0)
            gWeatherPtr->cloudBrightness = -1;
    }

    gSaveBlock1Ptr->weatherState.cloudBrightness = ToCloudBrightnessRaw(gWeatherPtr->cloudBrightness); // persist
    if (sCloudsShown)
        ApplyCloudBrightness();
    return TRUE;
}

// Step the brightness one level toward the target every 20 frames.
static void UpdateCloudBrightness(void)
{
    if (gWeatherPtr->cloudBrightness == gWeatherPtr->targetCloudBrightness)
        return;

    if (++gWeatherPtr->cloudBrightnessTimer < 20)
        return;
    gWeatherPtr->cloudBrightnessTimer = 0;

    StepCloudBrightness(gWeatherPtr->targetCloudBrightness);
}

// Per-frame cover step while (un)pausing: faster at higher levels so the fade in/out is quick.
static s32 CloudPauseCoverStep(s32 cover)
{
    if (cover > 10)
        return 3;
    if (cover > 5)
        return 2;
    return 1;
}

// Per frame while paused: step cover and brightness toward 0 so both reach 0 on the same frame. The one
// with the larger magnitude steps alone until they match, then both step in lockstep (cover takes its
// faster, level-keyed step). Unlike the normal steppers this is allowed to land brightness on 0 and does
// not persist to the save block, so the real cover/brightness (and their targets) are kept for when the
// pause is lifted.
void PauseClouds(void)
{
    s32 cover = gWeatherPtr->cloudCover;            // 0..15, never negative
    s32 brightness = gWeatherPtr->cloudBrightness;  // -2..3
    s32 absCover = cover;
    s32 absBrightness = (brightness < 0) ? -brightness : brightness;
    bool32 stepCover, stepBrightness;

    if (absCover == 0 && absBrightness == 0)
        return;

    if (absCover > absBrightness)
        stepCover = TRUE, stepBrightness = FALSE;
    else if (absBrightness > absCover)
        stepCover = FALSE, stepBrightness = TRUE;
    else // equal magnitude (both non-zero): step both
        stepCover = stepBrightness = TRUE;

    if (stepCover)
    {
        s32 step = CloudPauseCoverStep(cover);
        gWeatherPtr->cloudCover = cover - step; // step is sized so this never drops below 0
        ApplyCloudCover(); // frees the sprites once it reaches 0 so the silhouettes have the window to themselves
    }
    if (stepBrightness)
    {
        gWeatherPtr->cloudBrightness = brightness + (brightness > 0 ? -1 : 1);
        if (sCloudsShown)
            ApplyCloudBrightness();
    }

    // Fully faded now: hand the blend registers back so another effect can drive them while we're hidden.
    if (gWeatherPtr->cloudCover == 0 && gWeatherPtr->cloudBrightness == 0)
        ReleaseCloudBlend();
}

// Per frame while unpausing: the inverse of PauseClouds. Step cover and brightness back toward their
// targets (the values held through the pause), one level per frame, until both arrive. Stepping each
// toward its own target mirrors the pause exactly: both leave 0 together, the nearer target lands first.
static void UnpauseClouds(void)
{
    s32 steps = CloudPauseCoverStep(gWeatherPtr->cloudCover); // same level-keyed rate, climbing back up
    bool32 coverBusy = FALSE;
    bool32 brightnessBusy;

    while (steps-- > 0 && StepCloudCover(gWeatherPtr->targetCloudCover))
        coverBusy = TRUE;
    brightnessBusy = StepCloudBrightness(gWeatherPtr->targetCloudBrightness);

    if (!coverBusy && !brightnessBusy)
        gWeatherPtr->unpauseClouds = FALSE; // restored; normal steppers take over
}

// Drop the cloud overlay's claim on the blend registers so other processes can take control.
// Reclaimed the next time the brightness is applied (ShowClouds / StepCloudBrightness).
static void ReleaseCloudBlend(void)
{
    SetGpuReg(REG_OFFSET_BLDCNT, 0);
    SetGpuReg(REG_OFFSET_BLDY, 0);
}

// Keep the clouds paused while the player is buried in a cliff OR the silhouettes are still ramping out;
// once both are clear, kick off the inverse fade back to the held targets. Holding the pause through the
// silhouette ramp-out (not just the player's cliffLayer) lets that darkening finish fading before the
// clouds return.
static void UpdateCloudPauseTrigger(bool32 silhouettesActive)
{
    struct ObjectEvent *player = &gObjectEvents[gPlayerAvatar.objectEventId];
    bool32 keepPaused = gWeatherPtr->externalPauseClouds; // field move / battle entry, regardless of the player

    if (player->active && (player->cliffLayer == CLIFF_LAYER_OBSCURED || silhouettesActive))
        keepPaused = TRUE;

    if (keepPaused)
    {
        gWeatherPtr->pauseClouds = TRUE;
        gWeatherPtr->unpauseClouds = FALSE;
    }
    else
    {
        if (gWeatherPtr->pauseClouds)
            gWeatherPtr->unpauseClouds = TRUE; // begin the inverse fade back to the held targets
        gWeatherPtr->pauseClouds = FALSE;
    }
}

// Request/release an external cloud pause for an effect that needs the overlay's window & blend registers
// (a field move's show-mon banner). Set: the overlay fades out and is dropped entirely (see UpdateClouds).
// Cleared: it fades back in to the held cover/brightness. The targets are kept through the pause, so the
// fade-in lands where it left off.
void SetCloudsExternalPause(bool8 paused)
{
    gWeatherPtr->externalPauseClouds = paused;
}

// Whether an externally-paused overlay has finished fading out and handed its window/blend registers back
// (HideClouds restored the baseline WINOUT). An effect that overrides those registers waits on this so it
// doesn't blank the cloud sprites' still-receding object-window regions (which would show as black patches).
// Also TRUE when no pause is in effect or the overlay was never shown (e.g. indoors), so nothing waits then.
bool32 AreCloudsClearedForEffect(void)
{
    return !gWeatherPtr->externalPauseClouds || !sCloudsShown;
}

// Tear the overlay down at once (no fade) for the battle transition, which grabs the blend registers the
// next frame (no time to wait out a fade like the field-move banner does, see AreCloudsClearedForEffect).
// The held cover/brightness targets aren't touched, so the overlay is rebuilt when the field reloads
// (InitClouds clears the pause). Leaves the pause set so the overlay stays down until then.
void HideCloudsForBattle(void)
{
    gWeatherPtr->externalPauseClouds = TRUE; // keeps the overlay suppressed for any remaining field frames
    if (sCloudsShown) // only tear down the registers if the overlay is actually driving them
    {
        gWeatherPtr->cloudCover = 0;
        gWeatherPtr->cloudBrightness = 0;
        ApplyCloudCover(); // hide the sprites now
        HideClouds();      // hand the window & blend registers back to the transition
        sCloudsShown = FALSE;
    }
}

// Overwrite the cloud tiles already in VRAM with the active set's images. The
// tile range stays put, so the existing sprites keep pointing at the right tiles.
static void ReloadCloudsTiles(void)
{
    u16 tileStart = GetSpriteTileStartByTag(GFXTAG_CLOUD);

    if (tileStart != 0xFFFF)
        RequestSpriteCopy((const u8 *)gWeatherCloudsTiles[gWeatherPtr->cloudCover - 1], // level N -> set N-1
                          (u8 *)OBJ_VRAM0 + TILE_SIZE_4BPP * tileStart,
                          sizeof(gWeatherCloudsTiles[0]));
}

static const struct SpriteSheet sCloudsSpriteSheet =
{
    .data = (const u8 *)gWeatherCloudsTiles[0], // data/size set per cloudCover at load time
    .size = sizeof(gWeatherCloudsTiles[0]),
    .tag = GFXTAG_CLOUD,
};

static const struct OamData sCloudsSpriteOamData =
{
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_WINDOW,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(64x64),
    .x = 0,
    .size = SPRITE_SIZE(64x64),
    .tileNum = 0,
    .priority = 0,
    .paletteNum = 0,
};

// One anim per source image; anim N selects the Nth 64x64 image (its tile
// offset within the sheet is N * CLOUD_IMAGE_TILES).
#define CLOUDS_ANIM(n)                              \
    static const union AnimCmd sCloudsAnim##n[] =   \
    {                                               \
        ANIMCMD_FRAME((n) * CLOUD_IMAGE_TILES, 16), \
        ANIMCMD_END,                                \
    }

CLOUDS_ANIM(0); CLOUDS_ANIM(1); CLOUDS_ANIM(2);
CLOUDS_ANIM(3); CLOUDS_ANIM(4); CLOUDS_ANIM(5);
CLOUDS_ANIM(6); CLOUDS_ANIM(7); CLOUDS_ANIM(8);

static const union AnimCmd *const sCloudsSpriteAnimCmds[] =
{
    sCloudsAnim0, sCloudsAnim1, sCloudsAnim2,
    sCloudsAnim3, sCloudsAnim4, sCloudsAnim5,
    sCloudsAnim6, sCloudsAnim7, sCloudsAnim8,
};

static const struct SpriteTemplate sCloudsSpriteTemplate =
{
    .tileTag = GFXTAG_CLOUD,
    .paletteTag = PALTAG_WEATHER,
    .oam = &sCloudsSpriteOamData,
    .anims = sCloudsSpriteAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = UpdateCloudsSprite,
};

#define tSpriteColumn data[0]
#define tSpriteRow    data[1]
#define tImageIndex   data[2]

static void CreateCloudsSprites(void)
{
    u16 i;
    struct SpriteSheet cloudsSpriteSheet;
    u8 spriteId;
    struct Sprite *sprite;

    if (!gWeatherPtr->cloudsSpritesCreated)
    {
        cloudsSpriteSheet = sCloudsSpriteSheet;
        cloudsSpriteSheet.data = (const u8 *)gWeatherCloudsTiles[0]; // placeholder set; ApplyCloudCover swaps in the real one
        LoadSpriteSheet(&cloudsSpriteSheet);
        for (i = 0; i < NUM_CLOUD_SPRITES; i++)
        {
            spriteId = CreateSpriteAtEnd(&sCloudsSpriteTemplate, 0, 0, 0xFF);
            if (spriteId != MAX_SPRITES)
            {
                sprite = &gSprites[spriteId];
                sprite->tSpriteColumn = i % CLOUD_COLS;
                sprite->tSpriteRow = i / CLOUD_COLS;
                sprite->tImageIndex = -1; // force the first frame to pick an image
                gWeatherPtr->sprites.s2.cloudSprites[i] = sprite;
            }
            else
            {
                gWeatherPtr->sprites.s2.cloudSprites[i] = NULL;
            }
        }

        gWeatherPtr->cloudsSpritesCreated = TRUE;
    }
}

static void DestroyCloudsSprites(void)
{
    u16 i;

    if (gWeatherPtr->cloudsSpritesCreated)
    {
        for (i = 0; i < NUM_CLOUD_SPRITES; i++)
        {
            if (gWeatherPtr->sprites.s2.cloudSprites[i])
                DestroySprite(gWeatherPtr->sprites.s2.cloudSprites[i]);
            // Clear the pointer: with mid-session unloading at cover 0 (see ApplyCloudCover) the array is
            // walked again later (SetCloudsSpritesInvisible), and a stale pointer would scribble into the
            // freed/reused sprite slot.
            gWeatherPtr->sprites.s2.cloudSprites[i] = NULL;
        }

        FreeSpriteTilesByTag(GFXTAG_CLOUD);
        gWeatherPtr->cloudsSpritesCreated = FALSE;
    }
}

// Each sprite is a 64px-aligned cell of an infinitely tiling 3x3 pattern. From
// its grid slot and the (camera + drift) scroll offset, derive both the on-screen
// position and which of the 9 source images it should show. Because on-screen
// sprites always map to a contiguous run of pattern cells, the result is a
// seamless, scrolling cloud field.
static void UpdateCloudsSprite(struct Sprite *sprite)
{
    // Track the camera at 1.5x speed so the clouds parallax against the world.
    s32 baseX = (-gSpriteCoordOffsetX * CLOUD_PARALLAX_NUM / CLOUD_PARALLAX_DEN) - gWeatherPtr->cloudsXOffset;
    s32 baseY = (-gSpriteCoordOffsetY * CLOUD_PARALLAX_NUM / CLOUD_PARALLAX_DEN) + gWeatherPtr->cloudsYOffset;
    s32 col = sprite->tSpriteColumn;
    s32 row = sprite->tSpriteRow;
    // Tile top-left, aligned to the scroll phase so each sprite shows exactly
    // one source tile; covers [-CLOUD_TILE .. screen) as the phase drifts.
    s32 x = (baseX & (CLOUD_TILE - 1)) - CLOUD_TILE - CLOUD_LEFT_BUFFER + col * CLOUD_TILE;
    s32 y = (baseY & (CLOUD_TILE - 1)) - CLOUD_TILE + row * CLOUD_TILE;
    // Pattern cell this sprite currently occupies (anchored in world space).
    s32 patternCol = (col - (baseX >> 6) - 1) % CLOUD_PATTERN_DIM;
    s32 patternRow = (row - (baseY >> 6) - 1) % CLOUD_PATTERN_DIM;
    s32 imageIndex;

    if (patternCol < 0)
        patternCol += CLOUD_PATTERN_DIM;
    if (patternRow < 0)
        patternRow += CLOUD_PATTERN_DIM;
    imageIndex = patternRow * CLOUD_PATTERN_DIM + patternCol;

    // sprite->x/y are centers; the -32 center-to-corner vec puts the top-left at x/y.
    sprite->x = x + CLOUD_TILE / 2;
    sprite->y = y + CLOUD_TILE / 2;

    // Layer a gentle wobble on top (x2/y2 only, so the tile-image pick above is untouched). Uniform sway
    // the whole field shares + a directional ripple flowing toward the lower-left. Seam rule for a per-tile
    // shift: x2 may vary only with y, y2 only with x (a shear) - else neighbours separate normal to their
    // shared edge and a gap opens. So it's two crossed shear waves: the horizontal-offset bands travel down
    // (y - phase), the vertical-offset bands travel left (x + phase); together the warp drifts lower-left.
    // Amplitudes stay small (see CLOUD_*_AMP). gSineTable is Q8 (+/-256), so *AMP >> 8 yields +/-AMP px.
    {
        u16 phase = gWeatherPtr->cloudsWavePhase;
        s16 swayX = (gSineTable[(phase >> 1) & 0xFF] * CLOUD_SWAY_AMP) >> 8;
        s16 swayY = (gSineTable[((phase >> 1) + 64) & 0xFF] * CLOUD_SWAY_AMP) >> 8;
        s16 rippleX = (gSineTable[((sprite->y >> CLOUD_RIPPLE_SHIFT) - phase) & 0xFF] * CLOUD_RIPPLE_AMP) >> 8;
        s16 rippleY = (gSineTable[((sprite->x >> CLOUD_RIPPLE_SHIFT) + phase) & 0xFF] * CLOUD_RIPPLE_AMP) >> 8;
        sprite->x2 = swayX + rippleX;
        sprite->y2 = swayY + rippleY;
    }
    if (sprite->tImageIndex != imageIndex)
    {
        sprite->tImageIndex = imageIndex;
        StartSpriteAnim(sprite, imageIndex);
    }
}

#undef tSpriteColumn
#undef tSpriteRow
#undef tImageIndex

//------------------------------------------------------------------------------
// WEATHER_SANDSTORM
//------------------------------------------------------------------------------

static void UpdateSandstormWaveIndex(void);
static void UpdateSandstormMovement(void);
static void CreateSandstormSprites(void);
static void CreateSwirlSandstormSprites(void);
static void DestroySandstormSprites(void);
static void UpdateSandstormSprite(struct Sprite *);
static void WaitSandSwirlSpriteEntrance(struct Sprite *);
static void UpdateSandstormSwirlSprite(struct Sprite *);

#define MIN_SANDSTORM_WAVE_INDEX 0x20

void Sandstorm_InitVars(void)
{
    gWeatherPtr->initStep = 0;
    gWeatherPtr->weatherGfxLoaded = 0;
    gWeatherPtr->targetColorMapIndex = 0;
    gWeatherPtr->colorMapStepDelay = 20;
    if (!gWeatherPtr->sandstormSpritesCreated)
    {
        gWeatherPtr->sandstormXOffset = gWeatherPtr->sandstormYOffset = 0;
        gWeatherPtr->sandstormWaveIndex = 8;
        gWeatherPtr->sandstormWaveCounter = 0;
        // Dead code. How does the compiler not optimize this out?
        if (gWeatherPtr->sandstormWaveIndex >= 0x80 - MIN_SANDSTORM_WAVE_INDEX)
            gWeatherPtr->sandstormWaveIndex = 0x80 - gWeatherPtr->sandstormWaveIndex;

        Weather_SetBlendCoeffs(0, 16);
    }
}

void Sandstorm_InitAll(void)
{
    Sandstorm_InitVars();
    while (!gWeatherPtr->weatherGfxLoaded)
        Sandstorm_Main();
}

void Sandstorm_Main(void)
{
    UpdateSandstormMovement();
    UpdateSandstormWaveIndex();
    if (gWeatherPtr->sandstormWaveIndex >= 0x80 - MIN_SANDSTORM_WAVE_INDEX)
        gWeatherPtr->sandstormWaveIndex = MIN_SANDSTORM_WAVE_INDEX;

    switch (gWeatherPtr->initStep)
    {
    case 0:
        CreateSandstormSprites();
        CreateSwirlSandstormSprites();
        gWeatherPtr->initStep++;
        break;
    case 1:
        Weather_SetTargetBlendCoeffs(16, 0, 0);
        gWeatherPtr->initStep++;
        break;
    case 2:
        if (Weather_UpdateBlend())
        {
            gWeatherPtr->weatherGfxLoaded = TRUE;
            gWeatherPtr->initStep++;
        }
        break;
    }
}

bool8 Sandstorm_Finish(void)
{
    UpdateSandstormMovement();
    UpdateSandstormWaveIndex();
    switch (gWeatherPtr->finishStep)
    {
    case 0:
        Weather_SetTargetBlendCoeffs(0, 16, 0);
        gWeatherPtr->finishStep++;
        break;
    case 1:
        if (Weather_UpdateBlend())
            gWeatherPtr->finishStep++;
        break;
    case 2:
        DestroySandstormSprites();
        gWeatherPtr->finishStep++;
        break;
    default:
        return FALSE;
    }

    return TRUE;
}

static void UpdateSandstormWaveIndex(void)
{
    if (gWeatherPtr->sandstormWaveCounter++ > 4)
    {
        gWeatherPtr->sandstormWaveIndex++;
        gWeatherPtr->sandstormWaveCounter = 0;
    }
}

static void UpdateSandstormMovement(void)
{
    gWeatherPtr->sandstormXOffset -= gSineTable[gWeatherPtr->sandstormWaveIndex] * 4;
    gWeatherPtr->sandstormYOffset -= gSineTable[gWeatherPtr->sandstormWaveIndex];
    gWeatherPtr->sandstormBaseSpritesX = (gSpriteCoordOffsetX + (gWeatherPtr->sandstormXOffset >> 8)) & 0xFF;
    gWeatherPtr->sandstormPosY = gSpriteCoordOffsetY + (gWeatherPtr->sandstormYOffset >> 8);
}

static void DestroySandstormSprites(void)
{
    u16 i;

    if (gWeatherPtr->sandstormSpritesCreated)
    {
        for (i = 0; i < NUM_SANDSTORM_SPRITES; i++)
        {
            if (gWeatherPtr->sprites.s2.sandstormSprites1[i])
                DestroySprite(gWeatherPtr->sprites.s2.sandstormSprites1[i]);
        }

        gWeatherPtr->sandstormSpritesCreated = FALSE;
        FreeSpriteTilesByTag(GFXTAG_SANDSTORM);
    }

    if (gWeatherPtr->sandstormSwirlSpritesCreated)
    {
        for (i = 0; i < NUM_SWIRL_SANDSTORM_SPRITES; i++)
        {
            if (gWeatherPtr->sprites.s2.sandstormSprites2[i] != NULL)
                DestroySprite(gWeatherPtr->sprites.s2.sandstormSprites2[i]);
        }

        gWeatherPtr->sandstormSwirlSpritesCreated = FALSE;
    }
}

static const struct OamData sSandstormSpriteOamData =
{
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_BLEND,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(64x64),
    .x = 0,
    .size = SPRITE_SIZE(64x64),
    .tileNum = 0,
    .priority = 1,
    .paletteNum = 0,
};

static const union AnimCmd sSandstormSpriteAnimCmd0[] =
{
    ANIMCMD_FRAME(0, 3),
    ANIMCMD_END,
};

static const union AnimCmd sSandstormSpriteAnimCmd1[] =
{
    ANIMCMD_FRAME(64, 3),
    ANIMCMD_END,
};

static const union AnimCmd *const sSandstormSpriteAnimCmds[] =
{
    sSandstormSpriteAnimCmd0,
    sSandstormSpriteAnimCmd1,
};

static const struct SpriteTemplate sSandstormSpriteTemplate =
{
    .tileTag = GFXTAG_SANDSTORM,
    .paletteTag = PALTAG_WEATHER_2,
    .oam = &sSandstormSpriteOamData,
    .anims = sSandstormSpriteAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = UpdateSandstormSprite,
};

static const struct SpriteSheet sSandstormSpriteSheet =
{
    .data = gWeatherSandstormTiles,
    .size = sizeof(gWeatherSandstormTiles),
    .tag = GFXTAG_SANDSTORM,
};

// Regular sandstorm sprites
#define tSpriteColumn  data[0]
#define tSpriteRow     data[1]

// Swirly sandstorm sprites
#define tRadius        data[0]
#define tWaveIndex     data[1]
#define tRadiusCounter data[2]
#define tEntranceDelay data[3]

static void CreateSandstormSprites(void)
{
    u16 i;
    u8 spriteId;

    if (!gWeatherPtr->sandstormSpritesCreated)
    {
        LoadSpriteSheet(&sSandstormSpriteSheet);
        LoadCustomWeatherSpritePalette(gSandstormWeatherPalette);
        for (i = 0; i < NUM_SANDSTORM_SPRITES; i++)
        {
            spriteId = CreateSpriteAtEnd(&sSandstormSpriteTemplate, 0, (i / 5) * 64, 1);
            if (spriteId != MAX_SPRITES)
            {
                gWeatherPtr->sprites.s2.sandstormSprites1[i] = &gSprites[spriteId];
                gWeatherPtr->sprites.s2.sandstormSprites1[i]->tSpriteColumn = i % 5;
                gWeatherPtr->sprites.s2.sandstormSprites1[i]->tSpriteRow = i / 5;
            }
            else
            {
                gWeatherPtr->sprites.s2.sandstormSprites1[i] = NULL;
            }
        }

        gWeatherPtr->sandstormSpritesCreated = TRUE;
    }
}

static const u16 sSwirlEntranceDelays[] = {0, 120, 80, 160, 40, 0};

static void CreateSwirlSandstormSprites(void)
{
    u16 i;
    u8 spriteId;

    if (!gWeatherPtr->sandstormSwirlSpritesCreated)
    {
        for (i = 0; i < NUM_SWIRL_SANDSTORM_SPRITES; i++)
        {
            spriteId = CreateSpriteAtEnd(&sSandstormSpriteTemplate, i * 48 + 24, 208, 1);
            if (spriteId != MAX_SPRITES)
            {
                gWeatherPtr->sprites.s2.sandstormSprites2[i] = &gSprites[spriteId];
                gWeatherPtr->sprites.s2.sandstormSprites2[i]->oam.size = ST_OAM_SIZE_2;
                gWeatherPtr->sprites.s2.sandstormSprites2[i]->tSpriteRow = i * 51;
                gWeatherPtr->sprites.s2.sandstormSprites2[i]->tRadius = 8;
                gWeatherPtr->sprites.s2.sandstormSprites2[i]->tRadiusCounter = 0;
                gWeatherPtr->sprites.s2.sandstormSprites2[i]->data[4] = 0x6730; // unused value
                gWeatherPtr->sprites.s2.sandstormSprites2[i]->tEntranceDelay = sSwirlEntranceDelays[i];
                StartSpriteAnim(gWeatherPtr->sprites.s2.sandstormSprites2[i], 1);
                CalcCenterToCornerVec(gWeatherPtr->sprites.s2.sandstormSprites2[i], SPRITE_SHAPE(32x32), SPRITE_SIZE(32x32), ST_OAM_AFFINE_OFF);
                gWeatherPtr->sprites.s2.sandstormSprites2[i]->callback = WaitSandSwirlSpriteEntrance;
            }
            else
            {
                gWeatherPtr->sprites.s2.sandstormSprites2[i] = NULL;
            }

            gWeatherPtr->sandstormSwirlSpritesCreated = TRUE;
        }
    }
}

static void UpdateSandstormSprite(struct Sprite *sprite)
{
    sprite->y2 = gWeatherPtr->sandstormPosY;
    sprite->x = gWeatherPtr->sandstormBaseSpritesX + 32 + sprite->tSpriteColumn * 64;
    if (sprite->x >= DISPLAY_WIDTH + 32)
    {
        sprite->x = gWeatherPtr->sandstormBaseSpritesX + (DISPLAY_WIDTH * 2) - (4 - sprite->tSpriteColumn) * 64;
        sprite->x &= 0x1FF;
    }
}

static void WaitSandSwirlSpriteEntrance(struct Sprite *sprite)
{
    if (--sprite->tEntranceDelay == -1)
        sprite->callback = UpdateSandstormSwirlSprite;
}

static void UpdateSandstormSwirlSprite(struct Sprite *sprite)
{
    u32 x, y;

    if (--sprite->y < -48)
    {
        sprite->y = DISPLAY_HEIGHT + 48;
        sprite->tRadius = 4;
    }

    x = sprite->tRadius * gSineTable[sprite->tWaveIndex];
    y = sprite->tRadius * gSineTable[sprite->tWaveIndex + 0x40];
    sprite->x2 = x >> 8;
    sprite->y2 = y >> 8;
    sprite->tWaveIndex = (sprite->tWaveIndex + 10) & 0xFF;
    if (++sprite->tRadiusCounter > 8)
    {
        sprite->tRadiusCounter = 0;
        sprite->tRadius++;
    }
}

#undef tSpriteColumn
#undef tSpriteRow

#undef tRadius
#undef tWaveIndex
#undef tRadiusCounter
#undef tEntranceDelay

//------------------------------------------------------------------------------
// WEATHER_UNDERWATER_BUBBLES
//------------------------------------------------------------------------------

static void CreateBubbleSprite(u16);
static void DestroyBubbleSprites(void);
static void UpdateBubbleSprite(struct Sprite *);

static const u8 sBubbleStartDelays[] = {40, 90, 60, 90, 2, 60, 40, 30};

static const struct SpriteSheet sWeatherBubbleSpriteSheet =
{
    .data = gWeatherBubbleTiles,
    .size = sizeof(gWeatherBubbleTiles),
    .tag = GFXTAG_BUBBLE,
};

static const s16 sBubbleStartCoords[][2] =
{
    {120, 160},
    {376, 160},
    { 40, 140},
    {296, 140},
    {180, 130},
    {436, 130},
    { 60, 160},
    {436, 160},
    {220, 180},
    {476, 180},
    { 10,  90},
    {266,  90},
    {256, 160},
};

void Bubbles_InitVars(void)
{
    FogHorizontal_InitVars();
    if (!gWeatherPtr->bubblesSpritesCreated)
    {
        LoadSpriteSheet(&sWeatherBubbleSpriteSheet);
        gWeatherPtr->bubblesDelayIndex = 0;
        gWeatherPtr->bubblesDelayCounter = sBubbleStartDelays[0];
        gWeatherPtr->bubblesCoordsIndex = 0;
        gWeatherPtr->bubblesSpriteCount = 0;
    }
}

void Bubbles_InitAll(void)
{
    Bubbles_InitVars();
    while (!gWeatherPtr->weatherGfxLoaded)
        Bubbles_Main();
}

void Bubbles_Main(void)
{
    FogHorizontal_Main();
    if (++gWeatherPtr->bubblesDelayCounter > sBubbleStartDelays[gWeatherPtr->bubblesDelayIndex])
    {
        gWeatherPtr->bubblesDelayCounter = 0;
        if (++gWeatherPtr->bubblesDelayIndex > ARRAY_COUNT(sBubbleStartDelays) - 1)
            gWeatherPtr->bubblesDelayIndex = 0;

        CreateBubbleSprite(gWeatherPtr->bubblesCoordsIndex);
        if (++gWeatherPtr->bubblesCoordsIndex > ARRAY_COUNT(sBubbleStartCoords) - 1)
            gWeatherPtr->bubblesCoordsIndex = 0;
    }
}

bool8 Bubbles_Finish(void)
{
    if (!FogHorizontal_Finish())
    {
        DestroyBubbleSprites();
        return FALSE;
    }

    return TRUE;
}

static const union AnimCmd sBubbleSpriteAnimCmd0[] =
{
    ANIMCMD_FRAME(0, 16),
    ANIMCMD_FRAME(1, 16),
    ANIMCMD_END,
};

static const union AnimCmd *const sBubbleSpriteAnimCmds[] =
{
    sBubbleSpriteAnimCmd0,
};

static const struct SpriteTemplate sBubbleSpriteTemplate =
{
    .tileTag = GFXTAG_BUBBLE,
    .paletteTag = PALTAG_WEATHER,
    .oam = &gOamData_AffineOff_ObjNormal_8x8,
    .anims = sBubbleSpriteAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = UpdateBubbleSprite,
};

#define tScrollXCounter data[0]
#define tScrollXDir     data[1]
#define tCounter        data[2]

static void CreateBubbleSprite(u16 coordsIndex)
{
    s16 x = sBubbleStartCoords[coordsIndex][0];
    s16 y = sBubbleStartCoords[coordsIndex][1] - gSpriteCoordOffsetY;
    u8 spriteId = CreateSpriteAtEnd(&sBubbleSpriteTemplate, x, y, 0);
    if (spriteId != MAX_SPRITES)
    {
        gSprites[spriteId].oam.priority = 1;
        gSprites[spriteId].coordOffsetEnabled = TRUE;
        gSprites[spriteId].tScrollXCounter = 0;
        gSprites[spriteId].tScrollXDir = 0;
        gSprites[spriteId].tCounter = 0;
        gWeatherPtr->bubblesSpriteCount++;
    }
}

static void DestroyBubbleSprites(void)
{
    u16 i;

    if (gWeatherPtr->bubblesSpriteCount)
    {
        for (i = 0; i < MAX_SPRITES; i++)
        {
            if (gSprites[i].template == &sBubbleSpriteTemplate)
                DestroySprite(&gSprites[i]);
        }

        FreeSpriteTilesByTag(GFXTAG_BUBBLE);
        gWeatherPtr->bubblesSpriteCount = 0;
    }
}

static void UpdateBubbleSprite(struct Sprite *sprite)
{
    ++sprite->tScrollXCounter;
    if (++sprite->tScrollXCounter > 8) // double increment
    {
        sprite->tScrollXCounter = 0;
        if (sprite->tScrollXDir == 0)
        {
            if (++sprite->x2 > 4)
                sprite->tScrollXDir = 1;
        }
        else
        {
            if (--sprite->x2 <= 0)
                sprite->tScrollXDir = 0;
        }
    }

    sprite->y -= 3;
    if (++sprite->tCounter >= 120)
        DestroySprite(sprite);
}

#undef tScrollXCounter
#undef tScrollXDir
#undef tCounter

//------------------------------------------------------------------------------

static void UpdateRainCounter(u8, u8);

void SetSavedWeather(u32 weather)
{
    u8 oldWeather = gSaveBlock1Ptr->weather;
    gSaveBlock1Ptr->weather = weather;
    UpdateRainCounter(gSaveBlock1Ptr->weather, oldWeather);
}

u8 GetSavedWeather(void)
{
    return gSaveBlock1Ptr->weather;
}

void SetSavedWeatherFromCurrMapHeader(void)
{
    u8 oldWeather = gSaveBlock1Ptr->weather;
    gSaveBlock1Ptr->weather = gMapHeader.weather;
    UpdateRainCounter(gSaveBlock1Ptr->weather, oldWeather);
}

// Transitions the DISPLAYED weather to a map's weather without persisting it to the saveblock. Used by
// the roaming camera to preview a connected map's weather across a boundary while the saved weather
// stays the player's home-map weather.
void SetNextWeatherFromMapHeader(const struct MapHeader *mapHeader)
{
    SetNextWeather(mapHeader->weather);
}

void SetWeather(u32 weather)
{
    SetSavedWeather(weather);
    SetNextWeather(GetSavedWeather());
}

void SetWeather_Unused(u32 weather)
{
    SetSavedWeather(weather);
    SetCurrentAndNextWeather(GetSavedWeather());
}

void DoCurrentWeather(void)
{
    SetNextWeather(GetSavedWeather());
}

void ResumePausedWeather(void)
{
    // Returning to the field while the camera roams a connected map should resume THAT map's weather,
    // not the player's saved weather — display only, the saveblock keeps the player's. On the home map
    // (and on a fresh game/save load, where the camera sits on the player) this is the saved weather.
    const struct MapHeader *roamingMap = GetRoamingCameraMapHeader();
    u8 weather = (roamingMap != NULL) ? roamingMap->weather : GetSavedWeather();

    SetCurrentAndNextWeather(weather);
}

static void UpdateRainCounter(u8 newWeather, u8 oldWeather)
{
    if (newWeather != oldWeather
     && (newWeather == WEATHER_RAIN || newWeather == WEATHER_RAIN_THUNDERSTORM))
        IncrementGameStat(GAME_STAT_GOT_RAINED_ON);
}


