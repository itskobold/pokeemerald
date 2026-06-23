#ifndef GUARD_WEATHER_H
#define GUARD_WEATHER_H

#include "sprite.h"
#include "constants/field_weather.h"

#define TAG_WEATHER_START 0x1200
enum {
    GFXTAG_CLOUD = TAG_WEATHER_START,
    GFXTAG_FOG_H,
    GFXTAG_ASH,
    GFXTAG_SANDSTORM,
    GFXTAG_BUBBLE,
    GFXTAG_RAIN,
};
enum {
    PALTAG_WEATHER = TAG_WEATHER_START,
    PALTAG_WEATHER_2
};

#define NUM_WEATHER_COLOR_MAPS 19

struct Weather
{
    union
    {
        struct
        {
            struct Sprite *rainSprites[MAX_RAIN_SPRITES];
            struct Sprite *snowflakeSprites[101];
        } s1;
        struct
        {
            u8 filler0[0xA0];
            struct Sprite *fogHSprites[NUM_FOG_HORIZONTAL_SPRITES];
            struct Sprite *ashSprites[NUM_ASH_SPRITES];
            struct Sprite *cloudSprites[NUM_CLOUD_SPRITES];
            struct Sprite *sandstormSprites1[NUM_SANDSTORM_SPRITES];
        } s2;
    } sprites;
    u8 colorMaps[NUM_WEATHER_COLOR_MAPS][32];
    s8 colorMapIndex;
    s8 targetColorMapIndex;
    u8 colorMapStepDelay;
    u8 colorMapStepCounter;
    u16 fadeDestColor;
    u8 palProcessingState;
    u8 fadeScreenCounter;
    bool8 readyForInit;
    u8 taskId;
    u8 fadeInFirstFrame;
    u8 fadeInTimer;
    u16 initStep;
    u16 finishStep;
    u8 currWeather;
    u8 nextWeather;
    u8 weatherGfxLoaded;
    bool8 weatherChangeComplete;
    u8 weatherPicSpritePalIndex;
    // Rain
    u16 rainSpriteVisibleCounter;
    u8 curRainSpriteIndex;
    u8 targetRainSpriteCount;
    u8 rainSpriteCount;
    u8 rainSpriteVisibleDelay;
    u8 isDownpour;
    u8 rainStrength;
    // Snow
    u16 snowflakeVisibleCounter;
    u16 snowflakeTimer;
    u8 snowflakeSpriteCount;
    u8 targetSnowflakeSpriteCount;
    // Thunderstorm
    u16 thunderTimer;        // general-purpose timer for state transitions
    u16 thunderSETimer;      // timer for thunder sound effect
    bool8 thunderAllowEnd;
    bool8 thunderLongBolt;   // true if this cycle will end in a long lightning bolt
    u8 thunderShortBolts;    // the number of short bolts this cycle
    bool8 thunderEnqueued;
    // Horizontal fog
    u16 fogHScrollPosX;
    u16 fogHScrollCounter;
    u16 fogHScrollOffset;
    u8 lightenedFogSpritePals[6];
    u8 lightenedFogSpritePalsCount;
    u8 fogHSpritesCreated;
    // Ash
    u16 ashBaseSpritesX;
    u16 ashUnused;
    u8 ashSpritesCreated;
    // Sandstorm
    u32 sandstormXOffset;
    u32 sandstormYOffset;
    u16 sandstormUnused;
    u16 sandstormBaseSpritesX;
    u16 sandstormPosY;
    u16 sandstormWaveIndex;
    u16 sandstormWaveCounter;
    u16 sandstormWavePhase;     // own wobble sway phase, free-running (separate from the clouds', advances faster)
    u16 sandstormRipplePhaseX;  // own ripple band phases (Q8), integrated from wind velocity like the clouds but quicker
    u16 sandstormRipplePhaseY;
    u8 sandstormSpritesCreated;
    // Clouds
    u8 cloudCover:4;            // current cover level 0..15 (0 = no clouds, else clouds/1..15)
    u8 targetCloudCover:4;      // set is stepped toward this (see SetCloudCover)
    u8 cloudCoverTimer;         // frame counter; one step per 20 frames
    s8 cloudBrightness;         // current overlay brightness (actual -2..3, stage 0 skipped)
    s8 targetCloudBrightness;   // stepped toward this (see SetCloudBrightness)
    u8 cloudBrightnessTimer;    // frame counter; one step per 20 frames
    bool8 pauseClouds;          // while set, cover & brightness fade to 0 (player hidden in a cliff)
    bool8 unpauseClouds;        // while set, cover & brightness fade back to their targets (inverse fade)
    bool8 externalPauseClouds;  // external pause request (field moves, battle entry); folds into the pause above and drops the overlay entirely once cleared

    u8 cloudClearedTimer;       // frames the clouds have been fully faded out; gates the cliff silhouettes
    u8 cliffSilhouetteBrightness; // current silhouette darkening magnitude 0..6; ramps in/out at 1 per frame
    s16 cloudsScrollXAccum;     // sub-pixel wind drift accumulators (see UpdateCloudsMovement)
    s16 cloudsScrollYAccum;
    u16 cloudsXOffset;
    u16 cloudsYOffset;
    u16 cloudsWavePhase;        // free-running counter (1/frame) driving the sway/ripple wobble (see UpdateCloudsSprite)
    u16 cloudsRipplePhaseX;     // ripple band phases (Q8), integrated per frame from the wind unit velocity
    u16 cloudsRipplePhaseY;     // (x2 band by south, y2 by east); aims ripple along windDirection smoothly
    u8 targetWindDirection;     // BAM angle the live wind direction steps toward (see SetWindDirection)
    u8 targetWindSpeed;         // 0..WIND_SPEED_MAX; live wind speed steps toward this (see SetWindSpeed)
    u8 windDirectionTimer;      // frame counter; one step per 5 frames
    u8 windSpeedTimer;          // frame counter; one step per 10 frames
    u8 cloudsSpritesCreated;
    // Bubbles
    u16 bubblesDelayCounter;
    u16 bubblesDelayIndex;
    u16 bubblesCoordsIndex;
    u16 bubblesSpriteCount;
    u8 bubblesSpritesCreated;

    u16 currBlendEVA;
    u16 currBlendEVB;
    u16 targetBlendEVA;
    u16 targetBlendEVB;
    u8 blendUpdateCounter;
    u8 blendFrameCounter;
    u8 blendDelay;
    // Drought
    s16 droughtBrightnessStage;
    s16 droughtLastBrightnessStage;
    s16 droughtTimer;
    s16 droughtState;
    u8 droughtUnused[9];
    s8 loadDroughtPalsIndex;
    u8 loadDroughtPalsOffset;
};

// field_weather.c
extern struct Weather gWeather;
extern struct Weather *const gWeatherPtr;
extern const u16 gFogPalette[];

// field_weather_effect.c
extern const u8 gWeatherFogHorizontalTiles[];

void StartWeather(void);
void SetNextWeather(u8 weather);
void SetCurrentAndNextWeather(u8 weather);
void SetCurrentAndNextWeatherNoDelay(u8 weather);
void ApplyWeatherColorMapIfIdle(s8 colorMapIndex);
void ApplyWeatherColorMapIfIdle_Gradual(u8 colorMapIndex, u8 targetColorMapIndex, u8 colorMapStepDelay);
void FadeScreen(u8 mode, s8 delay);
bool8 IsWeatherNotFadingIn(void);
void UpdateSpritePaletteWithWeather(u8 spritePaletteIndex);
void ApplyWeatherColorMapToPal(u8 paletteIndex);
void LoadCustomWeatherSpritePalette(const u16 *palette);
void ResetDroughtWeatherPaletteLoading(void);
bool8 LoadDroughtWeatherPalettes(void);
void DroughtStateInit(void);
void DroughtStateRun(void);
void Weather_SetBlendCoeffs(u8 eva, u8 evb);
void Weather_SetTargetBlendCoeffs(u8 eva, u8 evb, int delay);
bool8 Weather_UpdateBlend(void);
u8 GetCurrentWeather(void);
void SetRainStrengthFromSoundEffect(u16 soundEffect);
void PlayRainStoppingSoundEffect(void);
u8 IsWeatherChangeComplete(void);
void SetWeatherScreenFadeOut(void);
void SetWeatherPalStateIdle(void);
void PreservePaletteInWeather(u8 preservedPalIndex);
void ResetPreservedPalettesInWeather(void);

// field_weather_effect.c
void Sunny_InitVars(void);
void Sunny_Main(void);
void Sunny_InitAll(void);
bool8 Sunny_Finish(void);
void Rain_InitVars(void);
void Rain_Main(void);
void Rain_InitAll(void);
bool8 Rain_Finish(void);
void Snow_InitVars(void);
void Snow_Main(void);
void Snow_InitAll(void);
bool8 Snow_Finish(void);
void Thunderstorm_InitVars(void);
void Thunderstorm_Main(void);
void Thunderstorm_InitAll(void);
bool8 Thunderstorm_Finish(void);
void FogHorizontal_InitVars(void);
void FogHorizontal_Main(void);
bool8 FogHorizontal_Finish(void);
void Ash_InitVars(void);
void Ash_Main(void);
void Ash_InitAll(void);
bool8 Ash_Finish(void);
void Sandstorm_InitVars(void);
void Sandstorm_Main(void);
void Sandstorm_InitAll(void);
bool8 Sandstorm_Finish(void);
void InitClouds(void);
void UpdateClouds(void);
void SetCloudCover(u8 target);
void SetCloudBrightness(s8 target);
void SetWindDirection(u8 target);
void SetWindSpeed(u8 target);
u8 GetClosestWindDirection(u8 direction, s8 *offset);
void PauseClouds(void);
void SetCloudsExternalPause(bool8 paused);
bool32 AreCloudsClearedForEffect(void);
void HideCloudsForBattle(void);
bool32 ShouldDrawCliffSilhouettes(void);
void Drought_InitVars(void);
void Drought_Main(void);
void Drought_InitAll(void);
bool8 Drought_Finish(void);
void Downpour_InitVars(void);
void Downpour_InitAll(void);
void Bubbles_InitVars(void);
void Bubbles_Main(void);
void Bubbles_InitAll(void);
bool8 Bubbles_Finish(void);

u8 GetSavedWeather(void);
void SetSavedWeather(u32 weather);
void SetSavedWeatherFromCurrMapHeader(void);
void SetNextWeatherFromMapHeader(const struct MapHeader *mapHeader);
void SetWeather(u32 weather);
void DoCurrentWeather(void);
void ResumePausedWeather(void);

#endif // GUARD_WEATHER_H
