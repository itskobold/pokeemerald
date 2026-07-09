#include "global.h"
#include "event_object_movement.h"
#include "field_camera.h"
#include "field_effect.h"
#include "field_effect_helpers.h"
#include "field_weather.h"
#include "fieldmap.h"
#include "gpu_regs.h"
#include "metatile_behavior.h"
#include "palette.h"
#include "sound.h"
#include "sprite.h"
#include "trig.h"
#include "constants/field_effects.h"
#include "constants/rgb.h"
#include "constants/songs.h"

#define OBJ_EVENT_PAL_TAG_NONE 0x11FF // duplicate of define in event_object_movement.c
#define OBJ_EVENT_PAL_TAG_BRIDGE_REFLECTION 0x1102 // duplicate of define in event_object_movement.c

extern u16 gReflectionPaletteBuffer[];

static void UpdateObjectReflectionSprite(struct Sprite *);
static void LoadObjectReflectionPalette(struct ObjectEvent *objectEvent, struct Sprite *sprite);
static void LoadSpecialReflectionPalette(struct Sprite *);
static void UpdateSurfBlobReflectionSprite(struct Sprite *);
static void SetUpSurfBlobReflection(u8 blobSpriteId, u8 playerObjId);
static void LoadFieldEffectPalette_(u8 fieldEffect, bool8 updateGammaType);
static void UpdateGrassFieldEffectSubpriority(struct Sprite *, u8);
static void RaiseFieldEffectSubpriorityBehindObjects(struct Sprite *, u8);
static void FadeFootprintsTireTracks_Step0(struct Sprite *);
static void FadeFootprintsTireTracks_Step1(struct Sprite *);
static void UpdateFeetInFlowingWaterFieldEffect(struct Sprite *);
static void UpdateAshFieldEffect_Wait(struct Sprite *);
static void UpdateAshFieldEffect_Show(struct Sprite *);
static void UpdateAshFieldEffect_End(struct Sprite *);
static void SynchronizeSurfAnim(struct ObjectEvent *, struct Sprite *);
static void SynchronizeSurfPosition(struct ObjectEvent *, struct Sprite *);
static void UpdateBobbingEffect(struct ObjectEvent *, struct Sprite *, struct Sprite *);
static void SpriteCB_UnderwaterSurfBlob(struct Sprite *);
static u32 ShowDisguiseFieldEffect(u8, u8, u8);

// Data used by all the field effects that share UpdateJumpImpactEffect
#define sJumpElevation  data[0]
#define sJumpFldEff     data[1]

// Data used by all the field effects that share WaitFieldEffectSpriteAnim
#define sWaitFldEff  data[0]

#define sReflectionObjEventId       data[0]
#define sReflectionObjEventLocalId  data[1]
#define sReflectionVerticalOffset   data[2]
#define sIsStillReflection          data[7]

void SetUpReflection(struct ObjectEvent *objectEvent, struct Sprite *sprite, bool8 stillReflection)
{
    struct Sprite *reflectionSprite;

    reflectionSprite = &gSprites[CreateCopySpriteAt(sprite, sprite->x, sprite->y, 152)];
    reflectionSprite->callback = UpdateObjectReflectionSprite;
    // OAM priority 3 puts the reflection between the two background planes: over BG3 (the reflective
    // tile's surface, priority 3 - the sprite wins the tie) and under BG2 (normal background,
    // priority 2), so shoreline/edge tiles on BG2 correctly clip a reflection that spills past.
    reflectionSprite->oam.priority = 3;
    reflectionSprite->usingSheet = TRUE;
    reflectionSprite->anims = gDummySpriteAnimTable;
    StartSpriteAnim(reflectionSprite, 0);
    reflectionSprite->affineAnims = gDummySpriteAffineAnimTable;
    reflectionSprite->affineAnimBeginning = TRUE;
    reflectionSprite->subspriteMode = SUBSPRITES_OFF;
    reflectionSprite->sReflectionObjEventId = sprite->data[0];
    reflectionSprite->sReflectionObjEventLocalId = objectEvent->localId;
    reflectionSprite->sIsStillReflection = stillReflection;
    LoadObjectReflectionPalette(objectEvent, reflectionSprite);

    if (!stillReflection)
        reflectionSprite->oam.affineMode = ST_OAM_AFFINE_NORMAL;
}

static s16 GetReflectionVerticalOffset(struct ObjectEvent *objectEvent)
{
    return GetObjectEventGraphicsInfo(objectEvent->graphicsId)->height - 2;
}

static void LoadObjectReflectionPalette(struct ObjectEvent *objectEvent, struct Sprite *reflectionSprite)
{
    u8 bridgeType;
    u16 bridgeReflectionVerticalOffsets[] = {
        [BRIDGE_TYPE_POND_LOW - 1] = 12,
        [BRIDGE_TYPE_POND_MED - 1] = 28,
        [BRIDGE_TYPE_POND_HIGH - 1] = 44
    };
    reflectionSprite->sReflectionVerticalOffset = 0;
    if (!GetObjectEventGraphicsInfo(objectEvent->graphicsId)->disableReflectionPaletteLoad
     && ((bridgeType = MetatileBehavior_GetBridgeType(objectEvent->previousMetatileBehavior))
      || (bridgeType = MetatileBehavior_GetBridgeType(objectEvent->currentMetatileBehavior))))
    {
        // When walking on a bridge high above water (Route 120), the reflection is a solid dark blue color.
        // This is so the sprite blends in with the dark water metatile underneath the bridge.
        reflectionSprite->sReflectionVerticalOffset = bridgeReflectionVerticalOffsets[bridgeType - 1];
        LoadObjectEventPalette(OBJ_EVENT_PAL_TAG_BRIDGE_REFLECTION);
        reflectionSprite->oam.paletteNum = IndexOfSpritePaletteTag(OBJ_EVENT_PAL_TAG_BRIDGE_REFLECTION);
        UpdatePaletteGammaType(reflectionSprite->oam.paletteNum, GAMMA_NORMAL);
        UpdateSpritePaletteWithWeather(reflectionSprite->oam.paletteNum);
    }
    else
    {
        LoadSpecialReflectionPalette(reflectionSprite);
    }
}

// Builds a darkened copy of the reflected object's palette on the fly and loads it into a
// dynamic slot (DOWP), tagged as the source palette's tag + 0x1000.
static void LoadSpecialReflectionPalette(struct Sprite *sprite)
{
    u32 R, G, B, i;
    u16 color;
    u16 *pal;
    struct SpritePalette reflectionPalette;

    CpuCopy16(&gPlttBufferUnfaded[OBJ_PLTT_ID(sprite->oam.paletteNum)], gReflectionPaletteBuffer, PLTT_SIZE_4BPP);
    pal = gReflectionPaletteBuffer;
    for (i = 0; i < 16; ++i)
    {
        color = pal[i];
        R = GET_R(color) + 8;
        G = GET_G(color) + 8;
        B = GET_B(color) + 16;
        if (R > 31) R = 31;
        if (G > 31) G = 31;
        if (B > 31) B = 31;
        pal[i] = RGB(R, G, B);
    }
    reflectionPalette.data = gReflectionPaletteBuffer;
    reflectionPalette.tag = GetSpritePaletteTagByPaletteNum(sprite->oam.paletteNum) + 0x1000;
    LoadSpritePalette(&reflectionPalette);
    sprite->oam.paletteNum = IndexOfSpritePaletteTag(reflectionPalette.tag);
    UpdatePaletteGammaType(sprite->oam.paletteNum, GAMMA_ALT);
    UpdateSpritePaletteWithWeather(sprite->oam.paletteNum);
}

// Dynamically loads the OBJ palette a field effect borrows from an object event (DOWP).
static void LoadFieldEffectPalette_(u8 fieldEffect, bool8 updateGammaType)
{
    const struct SpriteTemplate *spriteTemplate = gFieldEffectObjectTemplatePointers[fieldEffect];

    if (spriteTemplate->paletteTag != TAG_NONE)
    {
        LoadObjectEventPalette(spriteTemplate->paletteTag);
        if (updateGammaType)
            UpdatePaletteGammaType(IndexOfSpritePaletteTag(spriteTemplate->paletteTag), GAMMA_NORMAL);
    }
}

void LoadFieldEffectPalette(u8 fieldEffect)
{
    LoadFieldEffectPalette_(fieldEffect, TRUE);
}

static void UpdateObjectReflectionSprite(struct Sprite *reflectionSprite)
{
    struct ObjectEvent *objectEvent = &gObjectEvents[reflectionSprite->sReflectionObjEventId];
    struct Sprite *mainSprite = &gSprites[objectEvent->spriteId];
    if (!objectEvent->active || !objectEvent->hasReflection || objectEvent->localId != reflectionSprite->sReflectionObjEventLocalId)
    {
        reflectionSprite->inUse = FALSE;
    }
    else
    {
        reflectionSprite->oam.shape = mainSprite->oam.shape;
        reflectionSprite->oam.size = mainSprite->oam.size;
        reflectionSprite->oam.matrixNum = mainSprite->oam.matrixNum | ST_OAM_VFLIP;
        reflectionSprite->oam.tileNum = mainSprite->oam.tileNum;
        reflectionSprite->subspriteTables = mainSprite->subspriteTables;
        reflectionSprite->subspriteTableNum = mainSprite->subspriteTableNum;
        reflectionSprite->invisible = mainSprite->invisible;
        reflectionSprite->x = mainSprite->x;
        reflectionSprite->y = mainSprite->y + GetReflectionVerticalOffset(objectEvent) + reflectionSprite->sReflectionVerticalOffset;
        reflectionSprite->centerToCornerVecX = mainSprite->centerToCornerVecX;
        reflectionSprite->centerToCornerVecY = mainSprite->centerToCornerVecY;
        reflectionSprite->x2 = mainSprite->x2;
        reflectionSprite->y2 = -mainSprite->y2;
        reflectionSprite->coordOffsetEnabled = mainSprite->coordOffsetEnabled;

        if (objectEvent->hideReflection == TRUE)
            reflectionSprite->invisible = TRUE;

        // Hold the reflection hidden until the buried-object silhouette darkening has fully faded, so
        // it doesn't pop in mid-fade as the object emerges from behind a cliff onto the water.
        if (ShouldDrawCliffSilhouettes())
            reflectionSprite->invisible = TRUE;

        if (reflectionSprite->sIsStillReflection == FALSE)
        {
            // Sets the reflection sprite's rot/scale matrix to the appropriate
            // matrix based on whether or not the main sprite is horizontally flipped.
            // If the sprite is facing to the east, then it is flipped, and its matrixNum is 8.
            reflectionSprite->oam.matrixNum = 0;
            if (mainSprite->oam.matrixNum & ST_OAM_HFLIP)
                reflectionSprite->oam.matrixNum = 1;
        }
    }
}

#undef sReflectionObjEventId
#undef sReflectionObjEventLocalId
#undef sReflectionVerticalOffset
#undef sIsStillReflection

extern const struct SpriteTemplate *const gFieldEffectObjectTemplatePointers[];

#define sPrevX data[0]
#define sPrevY data[1]

u8 CreateWarpArrowSprite(void)
{
    u8 spriteId;
    LoadFieldEffectPalette_(FLDEFFOBJ_ARROW, FALSE);
    spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_ARROW], 0, 0, 82);
    if (spriteId != MAX_SPRITES)
    {
        struct Sprite *sprite = &gSprites[spriteId];
        sprite->oam.priority = 1;
        sprite->coordOffsetEnabled = TRUE;
        sprite->invisible = TRUE;
    }
    return spriteId;
}

void SetSpriteInvisible(u8 spriteId)
{
    gSprites[spriteId].invisible = TRUE;
}

void ShowWarpArrowSprite(u8 spriteId, u8 direction, s16 x, s16 y)
{
    struct Sprite *sprite = &gSprites[spriteId];
    if (sprite->invisible || sprite->sPrevX != x || sprite->sPrevY != y)
    {
        s16 x2, y2;
        SetSpritePosToMapCoords(x, y, &x2, &y2);
        sprite = &gSprites[spriteId];
        sprite->x = x2 + 8;
        sprite->y = y2 + 8;
        sprite->invisible = FALSE;
        sprite->sPrevX = x;
        sprite->sPrevY = y;
        StartSpriteAnim(sprite, direction - 1);
    }
}

#undef sPrevX
#undef sPrevY

static const u8 sShadowEffectTemplateIds[] = {
    FLDEFFOBJ_SHADOW_S,
    FLDEFFOBJ_SHADOW_M,
    FLDEFFOBJ_SHADOW_L,
    FLDEFFOBJ_SHADOW_XL
};

const u16 gShadowVerticalOffsets[] = {
    4,
    4,
    4,
    16
};

// Sprite data for FLDEFF_SHADOW
#define sLocalId  data[0]
#define sMapNum   data[1]
#define sMapGroup data[2]
#define sYOffset  data[3]

u32 FldEff_Shadow(void)
{
    u8 objectEventId = GetObjectEventIdByLocalIdAndMap(gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    const struct ObjectEventGraphicsInfo *graphicsInfo = GetObjectEventGraphicsInfo(gObjectEvents[objectEventId].graphicsId);
    u8 spriteId;
    LoadFieldEffectPalette_(sShadowEffectTemplateIds[graphicsInfo->shadowSize], FALSE);
    spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[sShadowEffectTemplateIds[graphicsInfo->shadowSize]], 0, 0, OBJ_SUBPRIORITY_SHADOW);
    if (spriteId != MAX_SPRITES)
    {
        gSprites[spriteId].coordOffsetEnabled = TRUE;
        gSprites[spriteId].sLocalId = gFieldEffectArguments[0];
        gSprites[spriteId].sMapNum = gFieldEffectArguments[1];
        gSprites[spriteId].sMapGroup = gFieldEffectArguments[2];
        gSprites[spriteId].sYOffset = (graphicsInfo->height >> 1) - gShadowVerticalOffsets[graphicsInfo->shadowSize];
    }
    return 0;
}

// Emits the whole 16x16 grass tile at OAM priority 1 (over the cliff foreground plane) while leaving the
// sprite's oam.priority at 2 so it still SORTS in the priority-2 band (tucks behind heads by subpriority,
// not over them). Without this a grass sprite whose tile is cliff-promoted draws behind the promoted cliff
// face (foreground plane, BG priority 1) and vanishes.
static const struct Subsprite sGrassCliffPromoteSubsprites[] = {
    { .x = -8, .y = -8, .shape = SPRITE_SHAPE(16x16), .size = SPRITE_SIZE(16x16), .tileOffset = 0, .priority = 1 },
};
static const struct SubspriteTable sGrassCliffPromoteSubspriteTable[] = {
    { ARRAY_COUNT(sGrassCliffPromoteSubsprites), sGrassCliffPromoteSubsprites },
};

// Keep a grass effect visible over a cliff promotion: emit priority 1 via a subsprite (over the foreground
// plane) while leaving oam.priority/subpriority alone. Keyed on the grass's OWN tile so it tracks the tile
// it sits on, not the object's current tile (they diverge as the object steps off). NOTE: promotion is
// driven by behind-cliff objects, so this toggles as one walks under the tile — that coupling is inherent
// to the dynamic cliff plane and is the price of the grass not vanishing behind the cliff face.
static void MatchFieldEffectToFrontSplit(struct Sprite *sprite, struct ObjectEvent *objEvent, s16 tileX, s16 tileY)
{
    if (objEvent->cliffLayer == CLIFF_LAYER_FRONT && IsTileCliffPromoted(tileX, tileY))
    {
        sprite->subspriteTables = sGrassCliffPromoteSubspriteTable;
        sprite->subspriteTableNum = 0;
        sprite->subspriteMode = SUBSPRITES_ON;
    }
    else if (sprite->subspriteTables == sGrassCliffPromoteSubspriteTable)
    {
        sprite->subspriteTables = NULL;
        sprite->subspriteMode = SUBSPRITES_OFF;
    }
}

// Post-promotion pass (OverworldBasic, after the cliff-promoted set is rebuilt): match every live grass
// effect to this frame's set. Done here, not in the grass callbacks, else it reads last frame's set.
void UpdateGrassFieldEffectsFrontSplit(void)
{
    u32 i;

    for (i = 0; i < MAX_SPRITES; i++)
    {
        struct Sprite *sprite = &gSprites[i];
        u8 objectEventId, localId, mapNum, mapGroup;
        s16 tileX, tileY;

        if (!sprite->inUse)
            continue;

        // Resolve the triggering object from the effect's stored id/map (data layouts differ per effect).
        if (sprite->callback == UpdateTallGrassFieldEffect || sprite->callback == UpdateLongGrassFieldEffect)
        {
            mapNum = sprite->data[3];       // sMapNum (low byte); sLocalId is the high byte
            localId = sprite->data[3] >> 8;
            mapGroup = sprite->data[4];     // sMapGroup
        }
        else if (sprite->callback == UpdateShortGrassFieldEffect)
        {
            localId = sprite->data[0];      // sLocalId / sMapNum / sMapGroup
            mapNum = sprite->data[1];
            mapGroup = sprite->data[2];
        }
        else
        {
            continue;
        }

        if (TryGetObjectEventIdByLocalIdAndMap(localId, mapNum, mapGroup, &objectEventId))
            continue;

        // Tall/long grass is tile-anchored (own tile in sX/sY); short grass follows its object.
        if (sprite->callback == UpdateShortGrassFieldEffect)
        {
            tileX = gObjectEvents[objectEventId].currentCoords.x;
            tileY = gObjectEvents[objectEventId].currentCoords.y;
        }
        else
        {
            tileX = sprite->data[1];        // sX
            tileY = sprite->data[2];        // sY
        }

        MatchFieldEffectToFrontSplit(sprite, &gObjectEvents[objectEventId], tileX, tileY);
    }
}

void UpdateShadowFieldEffect(struct Sprite *sprite)
{
    u8 objectEventId;

    if (TryGetObjectEventIdByLocalIdAndMap(sprite->sLocalId, sprite->sMapNum, sprite->sMapGroup, &objectEventId))
    {
        FieldEffectStop(sprite, FLDEFF_SHADOW);
    }
    else
    {
        struct ObjectEvent *objectEvent = &gObjectEvents[objectEventId];
        struct Sprite *linkedSprite = &gSprites[objectEvent->spriteId];
        // Always the normal field-sprite priority, even under a TOP-band owner: the shadow is a
        // ground mark, and inheriting OAM priority 1 would draw it over every other object's body.
        sprite->oam.priority = 2;
        sprite->x = linkedSprite->x;
        sprite->y = linkedSprite->y + sprite->sYOffset;
        if (!objectEvent->active || !objectEvent->hasShadow
         || MetatileBehavior_IsPokeGrass(objectEvent->currentMetatileBehavior)
         || MetatileBehavior_IsSurfableWaterOrUnderwater(objectEvent->currentMetatileBehavior)
         || MetatileBehavior_IsSurfableWaterOrUnderwater(objectEvent->previousMetatileBehavior)
         || MapGridGetMetatileReflectionAt(objectEvent->currentCoords.x, objectEvent->currentCoords.y)
         || MapGridGetMetatileReflectionAt(objectEvent->previousCoords.x, objectEvent->previousCoords.y))
        {
            FieldEffectStop(sprite, FLDEFF_SHADOW);
        }
        else
        {
            // Hide while behind a cliff, else the shadow pokes out in front of the cliff face.
            UpdateObjectEventSpriteInvisibility(sprite, objectEvent->cliffLayer != CLIFF_LAYER_FRONT);
        }
    }
}

#undef sLocalId
#undef sMapNum
#undef sMapGroup
#undef sYOffset

// Sprite data for FLDEFF_TALL_GRASS and FLDEFF_LONG_GRASS
#define sElevation   data[0]
#define sX           data[1]
#define sY           data[2]
#define sMapNum      data[3]      // Lower 8 bits
#define sLocalId     data[3] >> 8 // Upper 8 bits
#define sMapGroup    data[4]
#define sCurrentMap  data[5]
#define sObjectMoved data[7]

u32 FldEff_TallGrass(void)
{
    u8 spriteId;
    s16 x = gFieldEffectArguments[0];
    s16 y = gFieldEffectArguments[1];
    SetSpritePosToOffsetMapCoords(&x, &y, 8, 8);
    spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_TALL_GRASS], x, y, 0);
    if (spriteId != MAX_SPRITES)
    {
        struct Sprite *sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->sElevation = gFieldEffectArguments[2];
        sprite->sX = gFieldEffectArguments[0];
        sprite->sY = gFieldEffectArguments[1];
        sprite->sMapNum = gFieldEffectArguments[4]; // Also sLocalId
        sprite->sMapGroup = gFieldEffectArguments[5];
        sprite->sCurrentMap = gFieldEffectArguments[6];

        if (gFieldEffectArguments[7])
            SeekSpriteAnim(sprite, 4); // Skip to end of anim
    }
    return 0;
}

void UpdateTallGrassFieldEffect(struct Sprite *sprite)
{
    u8 metatileBehavior;
    u8 localId;
    u8 objectEventId;
    u8 mapNum = sprite->sCurrentMap >> 8;
    u8 mapGroup = sprite->sCurrentMap;

    if (gCamera.active && (gSaveBlock1Ptr->location.mapNum != mapNum || gSaveBlock1Ptr->location.mapGroup != mapGroup))
    {
        sprite->sX -= gCamera.x;
        sprite->sY -= gCamera.y;
        sprite->sCurrentMap = ((u8)gSaveBlock1Ptr->location.mapNum << 8) | (u8)gSaveBlock1Ptr->location.mapGroup;
    }
    localId = sprite->sLocalId;
    mapNum = sprite->sMapNum;
    mapGroup = sprite->sMapGroup;
    metatileBehavior = MapGridGetMetatileBehaviorAt(sprite->sX, sprite->sY);

    if (TryGetObjectEventIdByLocalIdAndMap(localId, mapNum, mapGroup, &objectEventId)
     || !MetatileBehavior_IsTallGrass(metatileBehavior)
     || (sprite->sObjectMoved && sprite->animEnded))
    {
        FieldEffectStop(sprite, FLDEFF_TALL_GRASS);
    }
    else
    {
        // Check if the object that triggered the effect has moved away
        struct ObjectEvent *objectEvent = &gObjectEvents[objectEventId];
        if ((objectEvent->currentCoords.x != sprite->sX || objectEvent->currentCoords.y != sprite->sY)
        && (objectEvent->previousCoords.x != sprite->sX || objectEvent->previousCoords.y != sprite->sY))
            sprite->sObjectMoved = TRUE;

        // Metatile behavior var re-used as subpriority
        metatileBehavior = 0;
        if (sprite->animCmdIndex == 0)
            metatileBehavior = 4;

        // Hide while the triggering object is behind a cliff, else the grass pokes out in front of it.
        UpdateObjectEventSpriteInvisibility(sprite, objectEvent->cliffLayer != CLIFF_LAYER_FRONT);
        UpdateGrassFieldEffectSubpriority(sprite, metatileBehavior);
    }
}

u32 FldEff_JumpTallGrass(void)
{
    u8 spriteId;

    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 12);
    spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_JUMP_TALL_GRASS], gFieldEffectArguments[0], gFieldEffectArguments[1], 0);
    if (spriteId != MAX_SPRITES)
    {
        struct Sprite *sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->sJumpElevation = gFieldEffectArguments[2];
        sprite->sJumpFldEff = FLDEFF_JUMP_TALL_GRASS;
    }
    return 0;
}

u8 FindTallGrassFieldEffectSpriteId(u8 localId, u8 mapNum, u8 mapGroup, s16 x, s16 y)
{
    u8 i;
    for (i = 0; i < MAX_SPRITES; i ++)
    {
        if (gSprites[i].inUse)
        {
            struct Sprite *sprite = &gSprites[i];
            if (sprite->callback == UpdateTallGrassFieldEffect
                && (x == sprite->sX && y == sprite->sY)
                && localId == (u8)(sprite->sLocalId)
                && mapNum == (sprite->sMapNum & 0xFF)
                && mapGroup == sprite->sMapGroup)
                return i;
        }
    }
    return MAX_SPRITES;
}

u32 FldEff_LongGrass(void)
{
    u8 spriteId;
    s16 x = gFieldEffectArguments[0];
    s16 y = gFieldEffectArguments[1];
    SetSpritePosToOffsetMapCoords(&x, &y, 8, 8);
    spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_LONG_GRASS], x, y, 0);
    if (spriteId != MAX_SPRITES)
    {
        struct Sprite *sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = 2;
        sprite->sElevation = gFieldEffectArguments[2];
        sprite->sX = gFieldEffectArguments[0];
        sprite->sY = gFieldEffectArguments[1];
        sprite->sMapNum = gFieldEffectArguments[4]; // Also sLocalId
        sprite->sMapGroup = gFieldEffectArguments[5];
        sprite->sCurrentMap = gFieldEffectArguments[6];

        if (gFieldEffectArguments[7])
            SeekSpriteAnim(sprite, 6); // Skip to end of anim
    }
    return 0;
}

void UpdateLongGrassFieldEffect(struct Sprite *sprite)
{
    u8 metatileBehavior;
    u8 localId;
    u8 objectEventId;
    u8 mapNum = sprite->sCurrentMap >> 8;
    u8 mapGroup = sprite->sCurrentMap;

    if (gCamera.active && (gSaveBlock1Ptr->location.mapNum != mapNum || gSaveBlock1Ptr->location.mapGroup != mapGroup))
    {
        sprite->sX -= gCamera.x;
        sprite->sY -= gCamera.y;
        sprite->sCurrentMap = ((u8)gSaveBlock1Ptr->location.mapNum << 8) | (u8)gSaveBlock1Ptr->location.mapGroup;
    }
    localId = sprite->sLocalId;
    mapNum = sprite->sMapNum;
    mapGroup = sprite->sMapGroup;
    metatileBehavior = MapGridGetMetatileBehaviorAt(sprite->sX, sprite->sY);
    if (TryGetObjectEventIdByLocalIdAndMap(localId, mapNum, mapGroup, &objectEventId)
     || !MetatileBehavior_IsLongGrass(metatileBehavior)
     || (sprite->sObjectMoved && sprite->animEnded))
    {
        FieldEffectStop(sprite, FLDEFF_LONG_GRASS);
    }
    else
    {
        // Check if the object that triggered the effect has moved away
        struct ObjectEvent *objectEvent = &gObjectEvents[objectEventId];
        if ((objectEvent->currentCoords.x != sprite->sX || objectEvent->currentCoords.y != sprite->sY)
         && (objectEvent->previousCoords.x != sprite->sX || objectEvent->previousCoords.y != sprite->sY))
            sprite->sObjectMoved = TRUE;

        // Hide while the triggering object is behind a cliff, else the grass pokes out in front of it.
        UpdateObjectEventSpriteInvisibility(sprite, objectEvent->cliffLayer != CLIFF_LAYER_FRONT);
        UpdateGrassFieldEffectSubpriority(sprite, 0);
    }
}

#undef sElevation
#undef sX
#undef sY
#undef sMapNum
#undef sLocalId
#undef sMapGroup
#undef sCurrentMap
#undef sObjectMoved

// Effectively unused as it's not possible in vanilla to jump onto long grass (no adjacent ledges, and can't ride the Acro Bike in it).
// The graphics for this effect do not visually correspond to long grass either. Perhaps these graphics were its original design?
u32 FldEff_JumpLongGrass(void)
{
    u8 spriteId;

    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_JUMP_LONG_GRASS], gFieldEffectArguments[0], gFieldEffectArguments[1], 0);
    if (spriteId != MAX_SPRITES)
    {
        struct Sprite *sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->sJumpElevation = gFieldEffectArguments[2];
        sprite->sJumpFldEff = FLDEFF_JUMP_LONG_GRASS;
    }
    return 0;
}

// Sprite data for FLDEFF_SHORT_GRASS
#define sLocalId  data[0]
#define sMapNum   data[1]
#define sMapGroup data[2]
#define sPrevX    data[3]
#define sPrevY    data[4]

u32 FldEff_ShortGrass(void)
{
    u8 objectEventId = GetObjectEventIdByLocalIdAndMap(gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    struct ObjectEvent *objectEvent = &gObjectEvents[objectEventId];
    u8 spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_SHORT_GRASS], 0, 0, 0);
    if (spriteId != MAX_SPRITES)
    {
        struct Sprite *sprite = &(gSprites[spriteId]);
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gSprites[objectEvent->spriteId].oam.priority;
        sprite->sLocalId = gFieldEffectArguments[0];
        sprite->sMapNum = gFieldEffectArguments[1];
        sprite->sMapGroup = gFieldEffectArguments[2];
        sprite->sPrevX = gSprites[objectEvent->spriteId].x;
        sprite->sPrevY = gSprites[objectEvent->spriteId].y;
    }
    return 0;
}

void UpdateShortGrassFieldEffect(struct Sprite *sprite)
{
    u8 objectEventId;

    if (TryGetObjectEventIdByLocalIdAndMap(sprite->sLocalId, sprite->sMapNum, sprite->sMapGroup, &objectEventId) || !gObjectEvents[objectEventId].inShortGrass)
    {
        FieldEffectStop(sprite, FLDEFF_SHORT_GRASS);
    }
    else
    {
        const struct ObjectEventGraphicsInfo *graphicsInfo = GetObjectEventGraphicsInfo(gObjectEvents[objectEventId].graphicsId);
        struct Sprite *linkedSprite = &gSprites[gObjectEvents[objectEventId].spriteId];
        s16 parentY = linkedSprite->y;
        s16 parentX = linkedSprite->x;
        if (parentX != sprite->sPrevX || parentY != sprite->sPrevY)
        {
            // Parent sprite moved, try to restart the animation
            sprite->sPrevX = parentX;
            sprite->sPrevY = parentY;
            if (sprite->animEnded)
                StartSpriteAnim(sprite, 0);
        }
        sprite->x = parentX;
        sprite->y = parentY;
        // Offset the grass sprite halfway down the parent sprite.
        sprite->y2 = (graphicsInfo->height >> 1) - 8;
        sprite->subpriority = linkedSprite->subpriority - 1;
        RaiseFieldEffectSubpriorityBehindObjects(sprite, objectEventId);
        sprite->oam.priority = linkedSprite->oam.priority;
        // Hide while the object is behind a cliff: this sprite follows it, so it'd otherwise poke
        // out in front of the cliff face.
        UpdateObjectEventSpriteInvisibility(sprite, linkedSprite->invisible
            || gObjectEvents[objectEventId].cliffLayer != CLIFF_LAYER_FRONT);
    }
}

#undef sLocalId
#undef sMapNum
#undef sMapGroup
#undef sPrevX
#undef sPrevY

// Sprite data for FLDEFF_SAND_FOOTPRINTS, FLDEFF_DEEP_SAND_FOOTPRINTS, and FLDEFF_BIKE_TIRE_TRACKS
#define sState   data[0]
#define sTimer   data[1]
#define sFldEff  data[7]

u32 FldEff_SandFootprints(void)
{
    u8 spriteId;

    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_SAND_FOOTPRINTS], gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    if (spriteId != MAX_SPRITES)
    {
        struct Sprite *sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->sFldEff = FLDEFF_SAND_FOOTPRINTS;
        StartSpriteAnim(sprite, gFieldEffectArguments[4]);
    }
    return 0;
}

u32 FldEff_DeepSandFootprints(void)
{
    u8 spriteId;

    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_DEEP_SAND_FOOTPRINTS], gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    if (spriteId != MAX_SPRITES)
    {
        struct Sprite *sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->sFldEff = FLDEFF_DEEP_SAND_FOOTPRINTS;
        StartSpriteAnim(sprite, gFieldEffectArguments[4]);
    }
    return spriteId;
}

u32 FldEff_BikeTireTracks(void)
{
    u8 spriteId;

    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_BIKE_TIRE_TRACKS], gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    if (spriteId != MAX_SPRITES)
    {
        struct Sprite *sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->sFldEff = FLDEFF_BIKE_TIRE_TRACKS;
        StartSpriteAnim(sprite, gFieldEffectArguments[4]);
    }
    return spriteId;
}

void (*const gFadeFootprintsTireTracksFuncs[])(struct Sprite *) = {
    FadeFootprintsTireTracks_Step0,
    FadeFootprintsTireTracks_Step1
};

void UpdateFootprintsTireTracksFieldEffect(struct Sprite *sprite)
{
    gFadeFootprintsTireTracksFuncs[sprite->sState](sprite);
}

static void FadeFootprintsTireTracks_Step0(struct Sprite *sprite)
{
    // Wait 40 frames before the flickering starts.
    if (++sprite->sTimer > 40)
        sprite->sState = 1;

    UpdateObjectEventSpriteInvisibility(sprite, FALSE);
}

static void FadeFootprintsTireTracks_Step1(struct Sprite *sprite)
{
    sprite->invisible ^= 1;
    sprite->sTimer++;
    UpdateObjectEventSpriteInvisibility(sprite, sprite->invisible);
    if (sprite->sTimer > 56)
        FieldEffectStop(sprite, sprite->sFldEff);
}

#undef sState
#undef sTimer
#undef sFldEff

// Sprite data for FLDEFF_SPLASH
#define sLocalId  data[0]
#define sMapNum   data[1]
#define sMapGroup data[2]

u32 FldEff_Splash(void)
{
    u8 objectEventId = GetObjectEventIdByLocalIdAndMap(gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    struct ObjectEvent *objectEvent = &gObjectEvents[objectEventId];
    u8 spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_SPLASH], 0, 0, 0);
    if (spriteId != MAX_SPRITES)
    {
        struct Sprite *linkedSprite;
        const struct ObjectEventGraphicsInfo *graphicsInfo = GetObjectEventGraphicsInfo(objectEvent->graphicsId);
        struct Sprite *sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        linkedSprite = &gSprites[objectEvent->spriteId];
        sprite->oam.priority = linkedSprite->oam.priority;
        sprite->sLocalId = gFieldEffectArguments[0];
        sprite->sMapNum = gFieldEffectArguments[1];
        sprite->sMapGroup = gFieldEffectArguments[2];
        sprite->y2 = (graphicsInfo->height >> 1) - 4;
        PlaySE(SE_PUDDLE);
    }
    return 0;
}

void UpdateSplashFieldEffect(struct Sprite *sprite)
{
    u8 objectEventId;

    if (sprite->animEnded || TryGetObjectEventIdByLocalIdAndMap(sprite->sLocalId, sprite->sMapNum, sprite->sMapGroup, &objectEventId))
    {
        FieldEffectStop(sprite, FLDEFF_SPLASH);
    }
    else
    {
        sprite->x = gSprites[gObjectEvents[objectEventId].spriteId].x;
        sprite->y = gSprites[gObjectEvents[objectEventId].spriteId].y;
        // Hide while the object is behind a cliff, else the splash pokes out in front of it.
        UpdateObjectEventSpriteInvisibility(sprite, gObjectEvents[objectEventId].cliffLayer != CLIFF_LAYER_FRONT);
    }
}

#undef sLocalId
#undef sMapNum
#undef sMapGroup

u32 FldEff_JumpSmallSplash(void)
{
    u8 spriteId;

    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 12);
    spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_JUMP_SMALL_SPLASH], gFieldEffectArguments[0], gFieldEffectArguments[1], 0);
    if (spriteId != MAX_SPRITES)
    {
        struct Sprite *sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->sJumpElevation = gFieldEffectArguments[2];
        sprite->sJumpFldEff = FLDEFF_JUMP_SMALL_SPLASH;
    }
    return 0;
}

u32 FldEff_JumpBigSplash(void)
{
    u8 spriteId;

    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_JUMP_BIG_SPLASH], gFieldEffectArguments[0], gFieldEffectArguments[1], 0);
    if (spriteId != MAX_SPRITES)
    {
        struct Sprite *sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->sJumpElevation = gFieldEffectArguments[2];
        sprite->sJumpFldEff = FLDEFF_JUMP_BIG_SPLASH;
    }
    return 0;
}

// Sprite data for FLDEFF_FEET_IN_FLOWING_WATER
#define sLocalId  data[0]
#define sMapNum   data[1]
#define sMapGroup data[2]
#define sPrevX    data[3]
#define sPrevY    data[4]

u32 FldEff_FeetInFlowingWater(void)
{
    u8 objectEventId = GetObjectEventIdByLocalIdAndMap(gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    struct ObjectEvent *objectEvent = &gObjectEvents[objectEventId];
    u8 spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_SPLASH], 0, 0, 0);
    if (spriteId != MAX_SPRITES)
    {
        const struct ObjectEventGraphicsInfo *graphicsInfo = GetObjectEventGraphicsInfo(objectEvent->graphicsId);
        struct Sprite *sprite = &gSprites[spriteId];
        sprite->callback = UpdateFeetInFlowingWaterFieldEffect;
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gSprites[objectEvent->spriteId].oam.priority;
        sprite->sLocalId = gFieldEffectArguments[0];
        sprite->sMapNum = gFieldEffectArguments[1];
        sprite->sMapGroup = gFieldEffectArguments[2];
        sprite->sPrevX = -1;
        sprite->sPrevY = -1;
        sprite->y2 = (graphicsInfo->height >> 1) - 4;
        StartSpriteAnim(sprite, 1);
    }
    return 0;
}

static void UpdateFeetInFlowingWaterFieldEffect(struct Sprite *sprite)
{
    u8 objectEventId;

    if (TryGetObjectEventIdByLocalIdAndMap(sprite->sLocalId, sprite->sMapNum, sprite->sMapGroup, &objectEventId) || !gObjectEvents[objectEventId].inShallowFlowingWater)
    {
        FieldEffectStop(sprite, FLDEFF_FEET_IN_FLOWING_WATER);
    }
    else
    {
        struct ObjectEvent *objectEvent = &gObjectEvents[objectEventId];
        struct Sprite *linkedSprite = &gSprites[objectEvent->spriteId];
        sprite->x = linkedSprite->x;
        sprite->y = linkedSprite->y;
        sprite->subpriority = linkedSprite->subpriority;
        RaiseFieldEffectSubpriorityBehindObjects(sprite, objectEventId);
        // Hide while the object is behind a cliff, else the splash pokes out in front of it.
        UpdateObjectEventSpriteInvisibility(sprite, objectEvent->cliffLayer != CLIFF_LAYER_FRONT);
        if (objectEvent->currentCoords.x != sprite->sPrevX || objectEvent->currentCoords.y != sprite->sPrevY)
        {
            sprite->sPrevX = objectEvent->currentCoords.x;
            sprite->sPrevY = objectEvent->currentCoords.y;
            if (!sprite->invisible)
                PlaySE(SE_PUDDLE);
        }
    }
}

#undef sLocalId
#undef sMapNum
#undef sMapGroup
#undef sPrevX
#undef sPrevY

u32 FldEff_Ripple(void)
{
    u8 spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_RIPPLE], gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    if (spriteId != MAX_SPRITES)
    {
        struct Sprite *sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->sWaitFldEff = FLDEFF_RIPPLE;
    }
    return 0;
}

// Sprite data for FLDEFF_HOT_SPRINGS_WATER
#define sLocalId  data[0]
#define sMapNum   data[1]
#define sMapGroup data[2]
#define sPrevX    data[3]
#define sPrevY    data[4]

u32 FldEff_HotSpringsWater(void)
{
    u8 objectEventId = GetObjectEventIdByLocalIdAndMap(gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    struct ObjectEvent *objectEvent = &gObjectEvents[objectEventId];
    u8 spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_HOT_SPRINGS_WATER], 0, 0, 0);
    if (spriteId != MAX_SPRITES)
    {
        struct Sprite *sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gSprites[objectEvent->spriteId].oam.priority;
        sprite->sLocalId = gFieldEffectArguments[0];
        sprite->sMapNum = gFieldEffectArguments[1];
        sprite->sMapGroup = gFieldEffectArguments[2];
        sprite->sPrevX = gSprites[objectEvent->spriteId].x; // Unused
        sprite->sPrevY = gSprites[objectEvent->spriteId].y; // Unused
    }
    return 0;
}

void UpdateHotSpringsWaterFieldEffect(struct Sprite *sprite)
{
    u8 objectEventId;

    if (TryGetObjectEventIdByLocalIdAndMap(sprite->sLocalId, sprite->sMapNum, sprite->sMapGroup, &objectEventId) || !gObjectEvents[objectEventId].inHotSprings)
    {
        FieldEffectStop(sprite, FLDEFF_HOT_SPRINGS_WATER);
    }
    else
    {
        const struct ObjectEventGraphicsInfo *graphicsInfo = GetObjectEventGraphicsInfo(gObjectEvents[objectEventId].graphicsId);
        struct Sprite *linkedSprite = &gSprites[gObjectEvents[objectEventId].spriteId];
        sprite->x = linkedSprite->x;
        sprite->y = (graphicsInfo->height >> 1) + linkedSprite->y - 8;
        sprite->subpriority = linkedSprite->subpriority - 1;
        RaiseFieldEffectSubpriorityBehindObjects(sprite, objectEventId);
        // Hide while the object is behind a cliff, else the water pokes out in front of it.
        UpdateObjectEventSpriteInvisibility(sprite, gObjectEvents[objectEventId].cliffLayer != CLIFF_LAYER_FRONT);
    }
}

#undef sLocalId
#undef sMapNum
#undef sMapGroup
#undef sPrevX
#undef sPrevY

u32 FldEff_UnusedGrass(void)
{
    u8 spriteId;

    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_UNUSED_GRASS], gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    if (spriteId != MAX_SPRITES)
    {
        struct Sprite *sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->sWaitFldEff = FLDEFF_UNUSED_GRASS;
    }
    return 0;
}

u32 FldEff_UnusedGrass2(void)
{
    u8 spriteId;

    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_UNUSED_GRASS_2], gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    if (spriteId != MAX_SPRITES)
    {
        struct Sprite *sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->sWaitFldEff = FLDEFF_UNUSED_GRASS_2;
    }
    return 0;
}

u32 FldEff_UnusedSand(void)
{
    u8 spriteId;

    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_UNUSED_SAND], gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    if (spriteId != MAX_SPRITES)
    {
        struct Sprite *sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->sWaitFldEff = FLDEFF_UNUSED_SAND;
    }
    return 0;
}

u32 FldEff_WaterSurfacing(void)
{
    u8 spriteId;

    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_WATER_SURFACING], gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    if (spriteId != MAX_SPRITES)
    {
        struct Sprite *sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->sWaitFldEff = FLDEFF_WATER_SURFACING;
    }
    return 0;
}

// Sprite data for FLDEFF_ASH
#define sState      data[0]
#define sX          data[1]
#define sY          data[2]
#define sMetatileId data[3]
#define sDelay      data[4]

void StartAshFieldEffect(s16 x, s16 y, u16 metatileId, s16 delay)
{
    gFieldEffectArguments[0] = x;
    gFieldEffectArguments[1] = y;
    gFieldEffectArguments[2] = 82; // subpriority
    gFieldEffectArguments[3] = 1; // priority
    gFieldEffectArguments[4] = metatileId;
    gFieldEffectArguments[5] = delay;
    FieldEffectStart(FLDEFF_ASH);
}

u32 FldEff_Ash(void)
{
    u8 spriteId;

    s16 x = gFieldEffectArguments[0];
    s16 y = gFieldEffectArguments[1];
    SetSpritePosToOffsetMapCoords(&x, &y, 8, 8);
    spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_ASH], x, y, gFieldEffectArguments[2]);
    if (spriteId != MAX_SPRITES)
    {
        struct Sprite *sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->sX = gFieldEffectArguments[0];
        sprite->sY = gFieldEffectArguments[1];
        sprite->sMetatileId = gFieldEffectArguments[4];
        sprite->sDelay = gFieldEffectArguments[5];
    }
    return 0;
}

void (*const gAshFieldEffectFuncs[])(struct Sprite *) = {
    UpdateAshFieldEffect_Wait,
    UpdateAshFieldEffect_Show,
    UpdateAshFieldEffect_End
};

void UpdateAshFieldEffect(struct Sprite *sprite)
{
    gAshFieldEffectFuncs[sprite->sState](sprite);
}

static void UpdateAshFieldEffect_Wait(struct Sprite *sprite)
{
    sprite->invisible = TRUE;
    sprite->animPaused = TRUE;
    if (--sprite->sDelay == 0)
        sprite->sState = 1;
}

static void UpdateAshFieldEffect_Show(struct Sprite *sprite)
{
    sprite->invisible = FALSE;
    sprite->animPaused = FALSE;
    MapGridSetMetatileIdAt(sprite->sX, sprite->sY, sprite->sMetatileId);
    CurrentMapDrawMetatileAt(sprite->sX, sprite->sY);
    gObjectEvents[gPlayerAvatar.objectEventId].triggerGroundEffectsOnMove = TRUE;
    sprite->sState = 2;
}

static void UpdateAshFieldEffect_End(struct Sprite *sprite)
{
    UpdateObjectEventSpriteInvisibility(sprite, FALSE);
    if (sprite->animEnded)
        FieldEffectStop(sprite, FLDEFF_ASH);
}

#undef sState
#undef sX
#undef sY
#undef sMetatileId
#undef sDelay

// Sprite data for FLDEFF_SURF_BLOB
#define sBitfield     data[0]
#define sPlayerOffset data[1]
#define sPlayerObjId  data[2]
#define sVelocity     data[3]
#define sTimer        data[4]
#define sIntervalIdx  data[5]
#define sPrevX        data[6]
#define sPrevY        data[7]

u32 FldEff_SurfBlob(void)
{
    u8 spriteId;

    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    // Behind every FRONT-band sprite (the surfer sits on top of it) but out of the BEHIND band.
    spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_SURF_BLOB], gFieldEffectArguments[0], gFieldEffectArguments[1], OBJ_SUBPRIORITY_SHADOW + 1);
    if (spriteId != MAX_SPRITES)
    {
        struct Sprite *sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        // The blob shares the player's overworld palette; under DOWP that lives in a dynamic slot.
        sprite->oam.paletteNum = gSprites[gObjectEvents[gFieldEffectArguments[2]].spriteId].oam.paletteNum;
        sprite->sPlayerObjId = gFieldEffectArguments[2];
        sprite->sVelocity = -1;
        sprite->sPrevX = -1;
        sprite->sPrevY = -1;
        // Give the blob a reflection tied to its OWN lifetime, not to a one-shot ground-effect edge.
        // Every path that (re)spawns the blob - fresh mount, waterfall, map reload while surfing -
        // runs through here, so each gets a reflection; it then just toggles visibility with the
        // player's own reflection. (Keying off GroundEffect_WaterReflection's hasReflection 0->1 edge
        // missed the reload-while-surfing paths, where hasReflection never re-edges - the reflection
        // was recreated for the player but never for the new blob, hence the intermittent misses.)
        SetUpSurfBlobReflection(spriteId, gFieldEffectArguments[2]);
    }
    FieldEffectActiveListRemove(FLDEFF_SURF_BLOB);
    return spriteId;
}


void SetSurfBlob_BobState(u8 spriteId, u8 state)
{
    gSprites[spriteId].sBitfield = (gSprites[spriteId].sBitfield & ~0xF) | (state & 0xF);
}

void SetSurfBlob_DontSyncAnim(u8 spriteId, bool8 dontSync)
{
    gSprites[spriteId].sBitfield = (gSprites[spriteId].sBitfield & ~0xF0) | ((dontSync & 0xF) << 4);
}

void SetSurfBlob_PlayerOffset(u8 spriteId, bool8 hasOffset, s16 offset)
{
    gSprites[spriteId].sBitfield = (gSprites[spriteId].sBitfield & ~0xF00) | ((hasOffset & 0xF) << 8);
    gSprites[spriteId].sPlayerOffset = offset;
}

static u8 GetSurfBlob_BobState(struct Sprite *sprite)
{
    return sprite->sBitfield & 0xF;
}

// Never TRUE
static u8 GetSurfBlob_DontSyncAnim(struct Sprite *sprite)
{
    return (sprite->sBitfield & 0xF0) >> 4;
}

static u8 GetSurfBlob_HasPlayerOffset(struct Sprite *sprite)
{
    return (sprite->sBitfield & 0xF00) >> 8;
}

// Top 24px of the 32x32 blob: drops the bottom tile row (the 8px overhang below the surfer's feet).
static const struct Subsprite sSurfBlobClippedSubsprites[] = {
    { -16, -16, SPRITE_SHAPE(32x16), SPRITE_SIZE(32x16), 0, 2 },
    { -16,   0, SPRITE_SHAPE(32x8),  SPRITE_SIZE(32x8),  8, 2 },
};
static const struct SubspriteTable sSurfBlobClippedSubspriteTable[] = {
    { ARRAY_COUNT(sSurfBlobClippedSubsprites), sSurfBlobClippedSubsprites },
};

// On the cliff face's bottom row the blob's bottom overhang pokes THROUGH the wall onto the open
// tile in front, where no promotion can cover it — clip it off. The side overhangs stay: past a
// cliff's side edge they render as real pixels, matching the player sprite's own overhang (the
// silhouette window mirrors these subsprites, so it keeps the same shape). Render coords track
// the sprite's real tile mid-step, so the strip returns the moment the blob slides clear.
static void UpdateSurfBlobClipping(struct Sprite *sprite, struct ObjectEvent *playerObj)
{
    s16 x, y;

    GetObjectEventRenderCoords(playerObj, &x, &y);
    if (playerObj->cliffLayer != CLIFF_LAYER_FRONT
     && MapGridGetElevationAt(x, y) > playerObj->baseElevation
     && MapGridGetElevationAt(x, y + 1) <= playerObj->baseElevation)
    {
        sprite->subspriteTables = sSurfBlobClippedSubspriteTable;
        sprite->subspriteTableNum = 0;
        sprite->subspriteMode = SUBSPRITES_IGNORE_PRIORITY; // keep the copied oam.priority
    }
    else
    {
        sprite->subspriteTables = NULL;
        sprite->subspriteMode = SUBSPRITES_OFF;
    }
}

void UpdateSurfBlobFieldEffect(struct Sprite *sprite)
{
    struct ObjectEvent *playerObj = &gObjectEvents[sprite->sPlayerObjId];
    struct Sprite *playerSprite = &gSprites[playerObj->spriteId];
    SynchronizeSurfAnim(playerObj, sprite);
    SynchronizeSurfPosition(playerObj, sprite);
    UpdateBobbingEffect(playerObj, playerSprite, sprite);
    UpdateSurfBlobClipping(sprite, playerObj);
    // Keep mirroring the player's palette in case DOWP re-allocated it (graphics change).
    sprite->oam.paletteNum = playerSprite->oam.paletteNum;
    sprite->oam.priority = playerSprite->oam.priority;
    // Follow the player out of the FRONT band (behind a cliff / drawn on top), tucked just behind them.
    if (GetObjectEventRenderBand(playerObj) == RENDER_BAND_FRONT)
        sprite->subpriority = OBJ_SUBPRIORITY_SHADOW + 1;
    else
        sprite->subpriority = playerSprite->subpriority + 1;
    // Cull off-screen like the player sprite and every other object-anchored field effect; without this
    // the blob keeps drawing while the player is parked off-screen (freecam roaming across map seams),
    // looking like it respawns on each map the camera visits.
    UpdateObjectEventSpriteInvisibility(sprite, playerObj->invisible);
}

static void SynchronizeSurfAnim(struct ObjectEvent *playerObj, struct Sprite *sprite)
{
    // Indexes into sAnimTable_SurfBlob
    u8 surfBlobDirectionAnims[] = {
        [DIR_NONE] = 0,
        [DIR_SOUTH] = 0,
        [DIR_NORTH] = 1,
        [DIR_WEST] = 2,
        [DIR_EAST] = 3,
        [DIR_SOUTHWEST] = 0,
        [DIR_SOUTHEAST] = 0,
        [DIR_NORTHWEST] = 1,
        [DIR_NORTHEAST] = 1,
    };

    if (!GetSurfBlob_DontSyncAnim(sprite))
        StartSpriteAnimIfDifferent(sprite, surfBlobDirectionAnims[playerObj->movementDirection]);
}

void SynchronizeSurfPosition(struct ObjectEvent *playerObj, struct Sprite *sprite)
{
    u8 i;
    s16 x = playerObj->currentCoords.x;
    s16 y = playerObj->currentCoords.y;
    s32 spriteY = sprite->y2;

    if (spriteY == 0 && (x != sprite->sPrevX || y != sprite->sPrevY))
    {
        // Player is moving while surfing, update position.
        sprite->sIntervalIdx = 0;
        sprite->sPrevX = x;
        sprite->sPrevY = y;
        for (i = DIR_SOUTH; i <= DIR_EAST; i++, x = sprite->sPrevX, y = sprite->sPrevY)
        {
            MoveCoords(i, &x, &y);
            if (MapGridGetElevationAt(x, y) == ELEVATION_DEFAULT)
            {
                // While dismounting the surf blob bobs at a slower rate
                sprite->sIntervalIdx++;
                break;
            }
        }
    }
}

static void UpdateBobbingEffect(struct ObjectEvent *playerObj, struct Sprite *playerSprite, struct Sprite *sprite)
{
    // The frame interval at which to update the blob's y movement.
    // Normally every 4th frame, but every 8th frame while dismounting.
    u16 intervals[] = {0x3, 0x7};

    u8 bobState = GetSurfBlob_BobState(sprite);
    if (bobState != BOB_NONE)
    {
        // Update vertical position of surf blob
        if (((u16)(++sprite->sTimer) & intervals[sprite->sIntervalIdx]) == 0)
            sprite->y2 += sprite->sVelocity;

        // Reverse bob direction
        if ((sprite->sTimer & 15) == 0)
            sprite->sVelocity = -sprite->sVelocity;

        if (bobState != BOB_JUST_MON)
        {
            // Update vertical position of player
            if (!GetSurfBlob_HasPlayerOffset(sprite))
                playerSprite->y2 = sprite->y2;
            else
                playerSprite->y2 = sprite->sPlayerOffset + sprite->y2;
            sprite->x = playerSprite->x;
            sprite->y = playerSprite->y + 8;
        }
    }
}

#undef sBitfield
#undef sPlayerOffset
#undef sPlayerObjId
#undef sVelocity
#undef sTimer
#undef sIntervalIdx
#undef sPrevX
#undef sPrevY

// The surf blob is its own sprite (not an object event), so the ground-effect reflection system
// never gives it one. Mirror the player's reflection here: a darkened, priority-3 copy that tracks
// the blob and borrows the darkened palette the player's own reflection already built.
#define sBlobReflSpriteId    data[0]
#define sBlobReflPlayerObjId data[1]

static void SetUpSurfBlobReflection(u8 blobSpriteId, u8 playerObjId)
{
    struct Sprite *blobSprite = &gSprites[blobSpriteId];
    struct Sprite *reflectionSprite;
    u8 reflectionId;

    // Subpriority 153 sits the blob's reflection just behind the player's reflection (created at 152).
    reflectionId = CreateCopySpriteAt(blobSprite, blobSprite->x, blobSprite->y, 153);
    if (reflectionId == MAX_SPRITES)
        return;

    reflectionSprite = &gSprites[reflectionId];
    reflectionSprite->callback = UpdateSurfBlobReflectionSprite;
    reflectionSprite->oam.priority = 3; // as SetUpReflection: over BG3, under BG2
    reflectionSprite->oam.affineMode = ST_OAM_AFFINE_NORMAL; // water (wavy) reflection
    reflectionSprite->oam.matrixNum = 0;
    reflectionSprite->usingSheet = TRUE;
    reflectionSprite->anims = gDummySpriteAnimTable;
    StartSpriteAnim(reflectionSprite, 0);
    reflectionSprite->affineAnims = gDummySpriteAffineAnimTable;
    reflectionSprite->affineAnimBeginning = TRUE;
    reflectionSprite->subspriteMode = SUBSPRITES_OFF;
    reflectionSprite->coordOffsetEnabled = TRUE;
    reflectionSprite->sBlobReflSpriteId = blobSpriteId;
    reflectionSprite->sBlobReflPlayerObjId = playerObjId;
}

static void UpdateSurfBlobReflectionSprite(struct Sprite *reflectionSprite)
{
    struct ObjectEvent *playerObj = &gObjectEvents[reflectionSprite->sBlobReflPlayerObjId];
    struct Sprite *blobSprite = &gSprites[reflectionSprite->sBlobReflSpriteId];
    u8 paletteNum;

    // Die ONLY with the blob (dismount destroys it, and the slot may be reused for another effect) or
    // if the player object goes inactive. Losing the reflection itself (behind a cliff, non-reflective
    // water) just hides us below - the sprite persists for the blob's whole lifetime and reappears
    // when the player's reflection does, so it survives reload-while-surfing without a hasReflection edge.
    if (!playerObj->active || blobSprite->callback != UpdateSurfBlobFieldEffect)
    {
        reflectionSprite->inUse = FALSE;
        return;
    }

    reflectionSprite->oam.shape = blobSprite->oam.shape;
    reflectionSprite->oam.size = blobSprite->oam.size;
    reflectionSprite->oam.tileNum = blobSprite->oam.tileNum;
    // The blob's East frame is its West frame with hFlip (shared tiles), and the reflection is drawn
    // in affine mode where flips live in the matrix - mirror the player reflection: matrix 1 for a
    // flipped source, 0 otherwise, so the reflection faces the same way as the blob.
    reflectionSprite->oam.matrixNum = (blobSprite->oam.matrixNum & ST_OAM_HFLIP) ? 1 : 0;
    reflectionSprite->subspriteTables = blobSprite->subspriteTables;
    reflectionSprite->subspriteTableNum = blobSprite->subspriteTableNum;
    reflectionSprite->subspriteMode = blobSprite->subspriteMode;
    reflectionSprite->invisible = blobSprite->invisible;
    reflectionSprite->x = blobSprite->x;
    // Mirror about the player's waterline: the blob sprite sits 8px (half a tile) below the player
    // sprite, so its reflection is one tile higher than the naive height-2 offset would place it.
    reflectionSprite->y = blobSprite->y + 14;
    reflectionSprite->centerToCornerVecX = blobSprite->centerToCornerVecX;
    reflectionSprite->centerToCornerVecY = blobSprite->centerToCornerVecY;
    reflectionSprite->x2 = blobSprite->x2;
    reflectionSprite->y2 = -blobSprite->y2;
    reflectionSprite->coordOffsetEnabled = blobSprite->coordOffsetEnabled;

    // Borrow the darkened palette the player's own reflection built (source tag + 0x1000). The blob
    // mirrors the player's palette slot each frame, so this stays in sync under DOWP re-allocation.
    // Until the player has a reflection that palette doesn't exist (lookup returns 0xFF); stay hidden.
    paletteNum = IndexOfSpritePaletteTag(GetSpritePaletteTagByPaletteNum(blobSprite->oam.paletteNum) + 0x1000);
    if (paletteNum != 0xFF)
        reflectionSprite->oam.paletteNum = paletteNum;

    // Track the player's own reflection: visible only where it is (reflective water, in front of any
    // cliff), hidden otherwise. hasReflection covers terrain + cliff state in one flag.
    if (!playerObj->hasReflection || paletteNum == 0xFF
     || playerObj->hideReflection == TRUE || ShouldDrawCliffSilhouettes())
        reflectionSprite->invisible = TRUE;
}

#undef sBlobReflSpriteId
#undef sBlobReflPlayerObjId

#define sSpriteId data[0]
#define sBobY     data[1]
#define sTimer    data[2]

u8 StartUnderwaterSurfBlobBobbing(u8 blobSpriteId)
{
    // Create a dummy sprite with its own callback
    // that tracks the actual surf blob sprite and
    // makes it bob up and down underwater
    u8 spriteId = CreateSpriteAtEnd(&gDummySpriteTemplate, 0, 0, -1);
    struct Sprite *sprite = &gSprites[spriteId];
    sprite->callback = SpriteCB_UnderwaterSurfBlob;
    sprite->invisible = TRUE;
    sprite->sSpriteId = blobSpriteId;
    sprite->sBobY = 1;
    return spriteId;
}

static void SpriteCB_UnderwaterSurfBlob(struct Sprite *sprite)
{
    struct Sprite *blobSprite = &gSprites[sprite->sSpriteId];

    // Update vertical position of surf blob
    if (((sprite->sTimer++) & 3) == 0)
        blobSprite->y2 += sprite->sBobY;
    // Reverse direction
    if ((sprite->sTimer & 15) == 0)
        sprite->sBobY = -sprite->sBobY;
}

#undef sSpriteId
#undef sBobY
#undef sTimer

u32 FldEff_Dust(void)
{
    u8 spriteId;

    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 12);
    spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_GROUND_IMPACT_DUST], gFieldEffectArguments[0], gFieldEffectArguments[1], 0);
    if (spriteId != MAX_SPRITES)
    {
        struct Sprite *sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->sJumpElevation = gFieldEffectArguments[2];
        sprite->sJumpFldEff = FLDEFF_DUST;
    }
    return 0;
}

// Sprite data for FLDEFF_SAND_PILE
#define sLocalId  data[0]
#define sMapNum   data[1]
#define sMapGroup data[2]
#define sPrevX    data[3]
#define sPrevY    data[4]

u32 FldEff_SandPile(void)
{
    u8 objectEventId = GetObjectEventIdByLocalIdAndMap(gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    struct ObjectEvent *objectEvent = &gObjectEvents[objectEventId];
    u8 spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_SAND_PILE], 0, 0, 0);
    if (spriteId != MAX_SPRITES)
    {
        const struct ObjectEventGraphicsInfo *graphicsInfo = GetObjectEventGraphicsInfo(objectEvent->graphicsId);
        struct Sprite *sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gSprites[objectEvent->spriteId].oam.priority;
        sprite->sLocalId = gFieldEffectArguments[0];
        sprite->sMapNum = gFieldEffectArguments[1];
        sprite->sMapGroup = gFieldEffectArguments[2];
        sprite->sPrevX = gSprites[objectEvent->spriteId].x;
        sprite->sPrevY = gSprites[objectEvent->spriteId].y;
        sprite->y2 = (graphicsInfo->height >> 1) - 2;
        SeekSpriteAnim(sprite, 2);
    }
    return 0;
}

void UpdateSandPileFieldEffect(struct Sprite *sprite)
{
    u8 objectEventId;

    if (TryGetObjectEventIdByLocalIdAndMap(sprite->sLocalId, sprite->sMapNum, sprite->sMapGroup, &objectEventId) || !gObjectEvents[objectEventId].inSandPile)
    {
        FieldEffectStop(sprite, FLDEFF_SAND_PILE);
    }
    else
    {
        s16 parentY = gSprites[gObjectEvents[objectEventId].spriteId].y;
        s16 parentX = gSprites[gObjectEvents[objectEventId].spriteId].x;
        if (parentX != sprite->sPrevX || parentY != sprite->sPrevY)
        {
            sprite->sPrevX = parentX;
            sprite->sPrevY = parentY;
            if (sprite->animEnded)
                StartSpriteAnim(sprite, 0);
        }
        sprite->x = parentX;
        sprite->y = parentY;
        sprite->subpriority = gSprites[gObjectEvents[objectEventId].spriteId].subpriority;
        RaiseFieldEffectSubpriorityBehindObjects(sprite, objectEventId);
        // Hide while the object is behind a cliff (this sprite follows it).
        UpdateObjectEventSpriteInvisibility(sprite, gObjectEvents[objectEventId].cliffLayer != CLIFF_LAYER_FRONT);
    }
}

#undef sLocalId
#undef sMapNum
#undef sMapGroup
#undef sPrevX
#undef sPrevY

// Free every per-object follower field-effect sprite bound to objectEvent. Followers (short/tall/long
// grass, sand pile, hot springs, flowing-water feet, shadow, reflection) normally self-destruct a frame
// after their parent leaves the terrain. But the paths that reset an object's ground-effect state in
// place — ResetObjectEventFldEffData, run for every object on every camera jump, and object removal —
// clear the gating flags / re-arm the spawn without giving the follower that frame to clean up. The
// orphan then re-adopts the same object while a fresh follower spawns alongside it, leaking one sprite
// per object per event until OAM saturates and the per-scanline OBJ limit starts dropping other sprites.
// Reaping here ties each follower's lifetime to the object. Followers store their parent link in their
// sprite data; the layout differs by effect (see each effect's data macros above).
void RemoveFollowerFieldEffectSprites(struct ObjectEvent *objectEvent)
{
    u8 i;
    u8 objEventId = (u8)(objectEvent - gObjectEvents);
    u8 localId = objectEvent->localId;
    u8 mapNum = objectEvent->mapNum;
    u8 mapGroup = objectEvent->mapGroup;

    for (i = 0; i < MAX_SPRITES; i++)
    {
        struct Sprite *sprite = &gSprites[i];

        if (!sprite->inUse)
            continue;

        // Followers tagged with the parent's (localId, mapNum, mapGroup) in data[0..2].
        if (sprite->callback == UpdateShortGrassFieldEffect
         || sprite->callback == UpdateSandPileFieldEffect
         || sprite->callback == UpdateHotSpringsWaterFieldEffect
         || sprite->callback == UpdateFeetInFlowingWaterFieldEffect
         || sprite->callback == UpdateShadowFieldEffect)
        {
            if ((u8)sprite->data[0] != localId || (u8)sprite->data[1] != mapNum || (u8)sprite->data[2] != mapGroup)
                continue;
            if (sprite->callback == UpdateShortGrassFieldEffect)
                FieldEffectStop(sprite, FLDEFF_SHORT_GRASS);
            else if (sprite->callback == UpdateSandPileFieldEffect)
                FieldEffectStop(sprite, FLDEFF_SAND_PILE);
            else if (sprite->callback == UpdateHotSpringsWaterFieldEffect)
                FieldEffectStop(sprite, FLDEFF_HOT_SPRINGS_WATER);
            else if (sprite->callback == UpdateFeetInFlowingWaterFieldEffect)
                FieldEffectStop(sprite, FLDEFF_FEET_IN_FLOWING_WATER);
            else
                FieldEffectStop(sprite, FLDEFF_SHADOW);
        }
        // Tall/long grass: localId = data[3] >> 8, mapNum = data[3] (low byte), mapGroup = data[4].
        else if (sprite->callback == UpdateTallGrassFieldEffect || sprite->callback == UpdateLongGrassFieldEffect)
        {
            if ((u8)(sprite->data[3] >> 8) != localId || (u8)sprite->data[3] != mapNum || (u8)sprite->data[4] != mapGroup)
                continue;
            FieldEffectStop(sprite, sprite->callback == UpdateTallGrassFieldEffect ? FLDEFF_TALL_GRASS : FLDEFF_LONG_GRASS);
        }
        // Reflection: a copy sprite keyed by the parent's object-event id in data[0]. It owns no field-
        // effect graphics (it copies the parent's), so just release it, mirroring its own self-destruct.
        else if (sprite->callback == UpdateObjectReflectionSprite)
        {
            if ((u8)sprite->data[0] == objEventId)
                sprite->inUse = FALSE;
        }
    }
}

u32 FldEff_Bubbles(void)
{
    u8 spriteId;

    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 0);
    spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_BUBBLES], gFieldEffectArguments[0], gFieldEffectArguments[1], 82);
    if (spriteId != MAX_SPRITES)
    {
        struct Sprite *sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = 1;
    }
    return 0;
}

#define sY data[0]

void UpdateBubblesFieldEffect(struct Sprite *sprite)
{
    // Move up 1 every other frame.
    sprite->sY += ((1 << 8) / 2);
    sprite->sY &= (1 << 8);
    sprite->y -= sprite->sY >> 8;
    UpdateObjectEventSpriteInvisibility(sprite, FALSE);
    if (sprite->invisible || sprite->animEnded)
        FieldEffectStop(sprite, FLDEFF_BUBBLES);
}

#undef sY

u32 FldEff_BerryTreeGrowthSparkle(void)
{
    u8 spriteId;

    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 4);
    LoadFieldEffectPalette(FLDEFFOBJ_SPARKLE);
    spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_SPARKLE], gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    if (spriteId != MAX_SPRITES)
    {
        struct Sprite *sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->sWaitFldEff = FLDEFF_BERRY_TREE_GROWTH_SPARKLE;
    }
    return 0;
}

// Sprite data for FLDEFF_TREE_DISGUISE / FLDEFF_MOUNTAIN_DISGUISE / FLDEFF_SAND_DISGUISE
#define sState      data[0]
#define sFldEff     data[1]
#define sLocalId    data[2]
#define sMapNum     data[3]
#define sMapGroup   data[4]
#define sReadyToEnd data[7]

u32 ShowTreeDisguiseFieldEffect(void)
{
    return ShowDisguiseFieldEffect(FLDEFF_TREE_DISGUISE, FLDEFFOBJ_TREE_DISGUISE, 4);
}

u32 ShowMountainDisguiseFieldEffect(void)
{
    return ShowDisguiseFieldEffect(FLDEFF_MOUNTAIN_DISGUISE, FLDEFFOBJ_MOUNTAIN_DISGUISE, 3);
}

u32 ShowSandDisguiseFieldEffect(void)
{
    return ShowDisguiseFieldEffect(FLDEFF_SAND_DISGUISE, FLDEFFOBJ_SAND_DISGUISE, 2);
}

static u32 ShowDisguiseFieldEffect(u8 fldEff, u8 fldEffObj, u8 paletteNum)
{
    u8 spriteId;

    if (TryGetObjectEventIdByLocalIdAndMap(gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2], &spriteId))
    {
        FieldEffectActiveListRemove(fldEff);
        return MAX_SPRITES;
    }
    LoadFieldEffectPalette(fldEffObj);
    spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[fldEffObj], 0, 0, 0);
    if (spriteId != MAX_SPRITES)
    {
        struct Sprite *sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled ++;
        sprite->sFldEff = fldEff;
        sprite->sLocalId = gFieldEffectArguments[0];
        sprite->sMapNum = gFieldEffectArguments[1];
        sprite->sMapGroup = gFieldEffectArguments[2];
    }
    return spriteId;
}

void UpdateDisguiseFieldEffect(struct Sprite *sprite)
{
    u8 objectEventId;
    const struct ObjectEventGraphicsInfo *graphicsInfo;
    struct Sprite *linkedSprite;

    if (TryGetObjectEventIdByLocalIdAndMap(sprite->sLocalId, sprite->sMapNum, sprite->sMapGroup, &objectEventId))
        FieldEffectStop(sprite, sprite->sFldEff);

    graphicsInfo = GetObjectEventGraphicsInfo(gObjectEvents[objectEventId].graphicsId);
    linkedSprite = &gSprites[gObjectEvents[objectEventId].spriteId];
    sprite->invisible = linkedSprite->invisible;
    sprite->x = linkedSprite->x;
    sprite->y = (graphicsInfo->height >> 1) + linkedSprite->y - 16;
    sprite->subpriority = linkedSprite->subpriority - 1;

    if (sprite->sState == 1)
    {
        sprite->sState++;
        StartSpriteAnim(sprite, 1);
    }

    if (sprite->sState == 2 && sprite->animEnded)
        sprite->sReadyToEnd = TRUE;

    if (sprite->sState == 3)
        FieldEffectStop(sprite, sprite->sFldEff);
}

void StartRevealDisguise(struct ObjectEvent *objectEvent)
{
    if (objectEvent->directionSequenceIndex == 1)
        gSprites[objectEvent->fieldEffectSpriteId].sState++;
}

bool8 UpdateRevealDisguise(struct ObjectEvent *objectEvent)
{
    struct Sprite *sprite;

    if (objectEvent->directionSequenceIndex == 2)
        return TRUE;

    if (objectEvent->directionSequenceIndex == 0)
        return TRUE;

    sprite = &gSprites[objectEvent->fieldEffectSpriteId];
    if (sprite->sReadyToEnd)
    {
        objectEvent->directionSequenceIndex = 2;
        sprite->sState++;
        return TRUE;
    }
    return FALSE;
}

#undef sState
#undef sFldEff
#undef sLocalId
#undef sMapNum
#undef sMapGroup
#undef sReadyToEnd

// Sprite data for FLDEFF_SPARKLE
#define sFinished data[0]
#define sEndTimer data[1]

u32 FldEff_Sparkle(void)
{
    u8 spriteId;

    gFieldEffectArguments[0] += MAP_OFFSET;
    gFieldEffectArguments[1] += MAP_OFFSET;
    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_SMALL_SPARKLE], gFieldEffectArguments[0], gFieldEffectArguments[1], 82);
    if (spriteId != MAX_SPRITES)
    {
        gSprites[spriteId].oam.priority = gFieldEffectArguments[2];
        gSprites[spriteId].coordOffsetEnabled = TRUE;
    }
    return 0;
}

void UpdateSparkleFieldEffect(struct Sprite *sprite)
{
    if (!sprite->sFinished)
    {
        if (sprite->animEnded)
        {
            sprite->invisible = TRUE;
            sprite->sFinished++;
        }
    }

    if (sprite->sFinished && ++sprite->sEndTimer > 34)
        FieldEffectStop(sprite, FLDEFF_SPARKLE);
}

#undef sFinished
#undef sEndTimer

#define sTimer       data[0]
#define sMoveTimer   data[1]
#define sState       data[2]
#define sVelocity    data[3]
#define sStartY      data[4]
#define sCounter     data[5]
#define sAnimCounter data[6]
#define sAnimState   data[7]

// Same as InitSpriteForFigure8Anim
static void InitRayquazaForFigure8Anim(struct Sprite *sprite)
{
    sprite->sAnimCounter = 0;
    sprite->sAnimState = 0;
}

// Only different from AnimateSpriteInFigure8 by the addition of SetGpuReg to move the spotlight
static bool8 AnimateRayquazaInFigure8(struct Sprite *sprite)
{
    bool8 finished = FALSE;

    switch (sprite->sAnimState)
    {
    case 0:
        sprite->x2 += GetFigure8XOffset(sprite->sAnimCounter);
        sprite->y2 += GetFigure8YOffset(sprite->sAnimCounter);
        break;
    case 1:
        sprite->x2 -= GetFigure8XOffset((FIGURE_8_LENGTH - 1) - sprite->sAnimCounter);
        sprite->y2 += GetFigure8YOffset((FIGURE_8_LENGTH - 1) - sprite->sAnimCounter);
        break;
    case 2:
        sprite->x2 -= GetFigure8XOffset(sprite->sAnimCounter);
        sprite->y2 += GetFigure8YOffset(sprite->sAnimCounter);
        break;
    case 3:
        sprite->x2 += GetFigure8XOffset((FIGURE_8_LENGTH - 1) - sprite->sAnimCounter);
        sprite->y2 += GetFigure8YOffset((FIGURE_8_LENGTH - 1) - sprite->sAnimCounter);
        break;
    }

    // Update spotlight to sweep left and right with Rayquaza
    SetGpuReg(REG_OFFSET_BG0HOFS, -sprite->x2);

    if (++sprite->sAnimCounter == FIGURE_8_LENGTH)
    {
        sprite->sAnimCounter = 0;
        sprite->sAnimState++;
    }
    if (sprite->sAnimState == 4)
    {
        sprite->y2 = 0;
        sprite->x2 = 0;
        finished = TRUE;
    }

    return finished;
}

void UpdateRayquazaSpotlightEffect(struct Sprite *sprite)
{
    u8 i, j;

    switch (sprite->sState)
    {
        case 0:
            if (!AreCloudsClearedForEffect()) // hold until the cloud overlay has released the blend registers
                return;
            if (sprite->sTimer == 0) // clouds gone: claim the blend/window registers for the spotlight
            {
                SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT1_BG0 | BLDCNT_EFFECT_BLEND | BLDCNT_TGT2_BG1 | BLDCNT_TGT2_BG2 | BLDCNT_TGT2_BG3 | BLDCNT_TGT2_OBJ | BLDCNT_TGT2_BD);
                SetGpuReg(REG_OFFSET_BLDALPHA, BLDALPHA_BLEND(14, 14));
                SetGpuReg(REG_OFFSET_WININ, WININ_WIN0_BG_ALL | WININ_WIN0_OBJ | WININ_WIN0_CLR | WININ_WIN1_BG_ALL | WININ_WIN1_OBJ | WININ_WIN1_CLR);
            }
            SetGpuReg(REG_OFFSET_BG0VOFS, DISPLAY_WIDTH / 2 - (sprite->sTimer / 3));
            if (sprite->sTimer == 96)
            {
                for (i = 0; i < 3; i++)
                {
                    for (j = 12; j < 18; j++)
                    {
                        ((u16 *)(BG_SCREEN_ADDR(31)))[i * 32 + j] = 0xBFF4 + i * 6 + j + 1;
                    }
                }
            }
            if (sprite->sTimer > 311)
            {
                sprite->sState = 1;
                sprite->sTimer = 0;
            }
            break;
        case 1:
            sprite->y = (gSineTable[sprite->sTimer / 3] >> 2) + sprite->sStartY;
            if (sprite->sTimer == 189)
            {
                sprite->sState = 2;
                sprite->sCounter = 0;
                sprite->sTimer = 0;
            }
            break;
        case 2:
            if (sprite->sTimer == 60)
            {
                sprite->sCounter++;
                sprite->sTimer = 0;
            }
            if (sprite->sCounter == 7)
            {
                sprite->sCounter = 0;
                sprite->sState = 3;
            }
            break;
        case 3:
            if (sprite->y2 == 0)
            {
                sprite->sTimer = 0;
                sprite->sState++;
            }
            if (sprite->sTimer == 5)
            {
                sprite->sTimer = 0;
                if (sprite->y2 > 0)
                    sprite->y2--;
                else
                    sprite->y2++;
            }
            break;
        case 4:
            if (sprite->sTimer == 60)
            {
                sprite->sState = 5;
                sprite->sTimer = 0;
                sprite->sCounter = 0;
            }
            break;
        case 5:
            InitRayquazaForFigure8Anim(sprite);
            sprite->sState = 6;
            sprite->sTimer = 0;
            break;
        case 6:
            if (AnimateRayquazaInFigure8(sprite))
            {
                sprite->sTimer = 0;
                if (++sprite->sCounter <= 2)
                {
                    InitRayquazaForFigure8Anim(sprite);
                }
                else
                {
                    sprite->sCounter = 0;
                    sprite->sState = 7;
                }
            }
            break;
        case 7:
            if (sprite->sTimer == 30)
            {
                sprite->sState = 8;
                sprite->sTimer = 0;
            }
            break;
        case 8:
            for (i = 0; i < 15; i++)
            {
                for (j = 12; j < 18; j++)
                {
                    ((u16 *)(BG_SCREEN_ADDR(31)))[i * 32 + j] = 0;
                }
            }
            SetGpuReg(REG_OFFSET_BG0VOFS, 0);
            SetCloudsExternalPause(FALSE); // scene over, registers freed: let the overlay fade back in
            FieldEffectStop(sprite, FLDEFF_RAYQUAZA_SPOTLIGHT);
            break;
    }

    if (sprite->sState == 1)
    {
        // Update movement
        if ((sprite->sMoveTimer & 7) == 0)
            sprite->y2 += sprite->sVelocity;
        // Reverse direction
        if ((sprite->sMoveTimer & 15) == 0)
            sprite->sVelocity = -sprite->sVelocity;
        sprite->sMoveTimer++;
    }

    sprite->sTimer++;
}

#undef sTimer
#undef sMoveTimer
#undef sState
#undef sStartY
#undef sVelocity
#undef sCounter
#undef sAnimCounter
#undef sAnimState

void UpdateJumpImpactEffect(struct Sprite *sprite)
{
    if (sprite->animEnded)
    {
        FieldEffectStop(sprite, sprite->sJumpFldEff);
    }
    else
    {
        UpdateObjectEventSpriteInvisibility(sprite, FALSE);
        SetSpriteRenderBand(sprite, RENDER_BAND_FRONT, 0);
    }
}

void WaitFieldEffectSpriteAnim(struct Sprite *sprite)
{
    if (sprite->animEnded)
        FieldEffectStop(sprite, sprite->sWaitFldEff);
    else
        UpdateObjectEventSpriteInvisibility(sprite, FALSE);
}

static void UpdateGrassFieldEffectSubpriority(struct Sprite *sprite, u8 subpriorityOffset)
{
    SetSpriteRenderBand(sprite, RENDER_BAND_FRONT, subpriorityOffset);
    // Tile-anchored: no object to skip, so it tucks behind every object standing in the grass.
    RaiseFieldEffectSubpriorityBehindObjects(sprite, OBJECT_EVENTS_COUNT);
}

// Push a field-effect sprite behind every object overlapping its tile that is drawn at least as far
// forward, so the effect never floats on top of a body. Originally this only mattered for the grass
// rustle (which belongs to the tile, skipObjectId == OBJECT_EVENTS_COUNT). Object-anchored overlays
// (short grass, hot springs, sand pile, feet in flowing water) sit just in front of their own object
// and pass their own object event id as skipObjectId so they stay there; the scan only tucks them
// behind a *different* object that has flowed into the same tile (a neighbour's just-vacated coords).
static void RaiseFieldEffectSubpriorityBehindObjects(struct Sprite *sprite, u8 skipObjectId)
{
    u8 i;
    s16 var, xhi, lyhi, yhi, ylo;

    for (i = 0; i < OBJECT_EVENTS_COUNT; i ++)
    {
        struct ObjectEvent *objectEvent = &gObjectEvents[i];
        // Only objects in the FRONT band can push the effect back: a TOP-band object already draws
        // over it by OAM priority, a BEHIND-band one sorts behind it by band, and chasing either's
        // subpriority would drag the effect out of its own band.
        if (objectEvent->active && i != skipObjectId && !objectEvent->fixedPriority
         && GetObjectEventRenderBand(objectEvent) == RENDER_BAND_FRONT)
        {
            struct Sprite *linkedSprite = &gSprites[objectEvent->spriteId];

            xhi = sprite->x + sprite->centerToCornerVecX;
            var = sprite->x - sprite->centerToCornerVecX;
            if (xhi < linkedSprite->x && var > linkedSprite->x)
            {
                lyhi = linkedSprite->y + linkedSprite->centerToCornerVecY;
                var = linkedSprite->y;
                ylo = sprite->y - sprite->centerToCornerVecY;
                yhi = ylo + linkedSprite->centerToCornerVecY;
                // Visit every overlapper, not just the first: the subpriority guard only ever raises
                // the value, so it settles behind the backmost (forwardmost-drawn) overlapping object.
                if ((lyhi < yhi || lyhi < ylo) && var > yhi && sprite->subpriority <= linkedSprite->subpriority)
                    sprite->subpriority = linkedSprite->subpriority + 2;
            }
        }
    }
}

// Unused, duplicates of data in event_object_movement.c
static const s8 sFigure8XOffsets[FIGURE_8_LENGTH] = {
    1, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 1, 2, 2, 1, 2,
    2, 1, 2, 2, 1, 2, 1, 1,
    2, 1, 1, 2, 1, 1, 2, 1,
    1, 2, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    0, 1, 1, 1, 0, 1, 1, 0,
    1, 0, 1, 0, 1, 0, 0, 0,
    0, 1, 0, 0, 0, 0, 0, 0,
};

static const s8 sFigure8YOffsets[FIGURE_8_LENGTH] = {
     0,  0,  1,  0,  0,  1,  0,  0,
     1,  0,  1,  1,  0,  1,  1,  0,
     1,  1,  0,  1,  1,  0,  1,  1,
     0,  0,  1,  0,  0,  1,  0,  0,
     1,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,
     0,  0, -1,  0,  0, -1,  0,  0,
    -1,  0, -1, -1,  0, -1, -1,  0,
    -1, -1, -1, -1, -1, -1, -1, -2,
};
