#include "global.h"
#include "debug.h"
#include "event_data.h"
#include "field_camera.h"
#include "field_player_avatar.h"
#include "field_weather.h"
#include "item.h"
#include "list_menu.h"
#include "main.h"
#include "malloc.h"
#include "map_name_popup.h"
#include "menu.h"
#include "pokedex.h"
#include "script.h"
#include "script_menu.h"
#include "sound.h"
#include "string_util.h"
#include "task.h"
#include "window.h"
#include "constants/field_weather.h"
#include "constants/flags.h"
#include "constants/moves.h"
#include "constants/songs.h"

void SetDebugNewGameFlags()
{
    struct Pokemon *mon;
    int nationalDexNum;

    FlagSet(FLAG_SYS_B_DASH);
    FlagSet(FLAG_SYS_POKEMON_GET);
    FlagSet(FLAG_SYS_POKENAV_GET);
    FlagSet(FLAG_SYS_POKEDEX_GET);
    FlagSet(FLAG_SYS_NATIONAL_DEX);
    FlagSet(FLAG_ADVENTURE_STARTED);
    FlagSet(FLAG_SYS_GAME_CLEAR);
    FlagSet(FLAG_BADGE01_GET);
    FlagSet(FLAG_BADGE02_GET);
    FlagSet(FLAG_BADGE03_GET);
    FlagSet(FLAG_BADGE04_GET);
    FlagSet(FLAG_BADGE05_GET);
    FlagSet(FLAG_BADGE06_GET);
    FlagSet(FLAG_BADGE07_GET);
    FlagSet(FLAG_BADGE08_GET);
    FlagSet(FLAG_VISITED_LITTLEROOT_TOWN);
    FlagSet(FLAG_VISITED_OLDALE_TOWN);
    FlagSet(FLAG_VISITED_DEWFORD_TOWN);
    FlagSet(FLAG_VISITED_LAVARIDGE_TOWN);
    FlagSet(FLAG_VISITED_FALLARBOR_TOWN);
    FlagSet(FLAG_VISITED_VERDANTURF_TOWN);
    FlagSet(FLAG_VISITED_PACIFIDLOG_TOWN);
    FlagSet(FLAG_VISITED_PETALBURG_CITY);
    FlagSet(FLAG_VISITED_SLATEPORT_CITY);
    FlagSet(FLAG_VISITED_MAUVILLE_CITY);
    FlagSet(FLAG_VISITED_RUSTBORO_CITY);
    FlagSet(FLAG_VISITED_FORTREE_CITY);
    FlagSet(FLAG_VISITED_LILYCOVE_CITY);
    FlagSet(FLAG_VISITED_MOSSDEEP_CITY);
    FlagSet(FLAG_VISITED_SOOTOPOLIS_CITY);
    FlagSet(FLAG_VISITED_EVER_GRANDE_CITY);
    FlagSet(FLAG_LANDMARK_BATTLE_FRONTIER);
    FlagSet(FLAG_SYS_FRONTIER_PASS);

    mon = AllocZeroed(sizeof(struct Pokemon));

    CreateMon(mon, SPECIES_RAYQUAZA, 100, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);
    SetMonMoveSlot(mon, MOVE_FLY, 0);
    GiveMonToPlayer(mon);

    CreateMon(mon, SPECIES_KYOGRE, 100, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);
    SetMonMoveSlot(mon, MOVE_SURF, 0);
    GiveMonToPlayer(mon);

    Free(mon);

    nationalDexNum = SpeciesToNationalPokedexNum(SPECIES_RAYQUAZA);
    GetSetPokedexFlag(nationalDexNum, FLAG_SET_SEEN);
    GetSetPokedexFlag(nationalDexNum, FLAG_SET_CAUGHT);

    AddBagItem(ITEM_MASTER_BALL, 99);
    AddBagItem(ITEM_POKE_BALL, 99);
    AddBagItem(ITEM_FULL_RESTORE, 99);
    AddBagItem(ITEM_FULL_HEAL, 99);
    AddBagItem(ITEM_MAX_POTION, 99);
    AddBagItem(ITEM_SUPER_POTION, 99);
    AddBagItem(ITEM_MAX_REVIVE, 99);
    AddBagItem(ITEM_REVIVE, 99);
    AddBagItem(ITEM_MAX_ETHER, 99);
    AddBagItem(ITEM_ETHER, 99);
    AddBagItem(ITEM_MAX_REPEL, 99);
    AddBagItem(ITEM_ACRO_BIKE, 1);
    AddBagItem(ITEM_MACH_BIKE, 1);
    AddBagItem(ITEM_SUPER_ROD, 1);
}

// Shell debug menu, opened in the field by pressing L + R + Select together.
// The main menu is a list of categories, each opening its own submenu.

// Main menu items
enum
{
    DEBUG_MENU_ITEM_CAMERA,
    DEBUG_MENU_ITEM_WEATHER,
    DEBUG_MENU_ITEM_CANCEL,
};

// Weather submenu items
enum
{
    DEBUG_WEATHER_ITEM_SET_CLOUD_COVER,
    DEBUG_WEATHER_ITEM_SET_CLOUD_BRIGHTNESS,
    DEBUG_WEATHER_ITEM_CANCEL,
};

// Camera submenu items
enum
{
    DEBUG_CAMERA_ITEM_TOGGLE_FREECAM,
    DEBUG_CAMERA_ITEM_TRACK_NPC,
    DEBUG_CAMERA_ITEM_PAN_TILE,
    DEBUG_CAMERA_ITEM_PAN_NPC,
    DEBUG_CAMERA_ITEM_RESET,
    DEBUG_CAMERA_ITEM_CANCEL,
};

// "Track NPC" / "Pan to NPC" share one numeric scroll box; the mode decides whether choosing
// an object jumps the camera live (track) or arms a smooth pan committed with A (pan).
enum
{
    DEBUG_NPC_BOX_MODE_TRACK,
    DEBUG_NPC_BOX_MODE_PAN,
};

// Target duration (frames) for camera pans started from the debug menu, ~0.4s at 60fps.
#define DEBUG_PAN_FRAMES 24

#define tMenuTaskId  data[0]
#define tWindowId    data[1]

#define TAG_TRACK_NPC_SCROLL_ARROW 2110
#define TAG_CLOUD_COVER_SCROLL_ARROW 2111
#define TAG_CLOUD_BRIGHTNESS_SCROLL_ARROW 2112

static void Debug_ShowMenu(const struct ListMenuItem *items, u16 numItems, void (*handleInput)(u8));
static void Debug_DestroyMenu(u8 taskId);
static void Debug_CloseMenu(u8 taskId);
static void Debug_OpenMainMenu(void);
static void Debug_ShowCameraMenu(void);
static void Debug_ShowWeatherMenu(void);
static void Debug_OpenCloudCoverBox(void);
static void Debug_DrawCloudCoverBox(void);
static void Debug_TearDownCloudCoverBox(u8 taskId);
static void Debug_CommitCloudCoverBox(u8 taskId);
static void Debug_CancelCloudCoverBox(u8 taskId);
static void Debug_OpenCloudBrightnessBox(void);
static void Debug_DrawCloudBrightnessBox(void);
static void Debug_TearDownCloudBrightnessBox(u8 taskId);
static void Debug_CommitCloudBrightnessBox(u8 taskId);
static void Debug_CancelCloudBrightnessBox(u8 taskId);
static void Debug_EnableFreecam(u8 taskId);
static void Debug_ReturnCameraToPlayer(u8 taskId);
static void Debug_OpenNpcBox(u8 mode);
static void Debug_TearDownNpcBox(u8 taskId);
static void Debug_CloseTrackNpcBox(u8 taskId);
static void Debug_CommitPanNpcBox(u8 taskId);
static void Debug_CancelPanNpcBox(u8 taskId);
static void Debug_DrawTrackNpcBox(void);
static u8 Debug_TrackedObjectCount(void);
static u8 Debug_LocalIdForOrder(u8 order);
static u8 Debug_OrderForLocalId(u8 localId);

static void DebugTask_HandleMenuInput_Main(u8 taskId);
static void DebugTask_HandleMenuInput_Camera(u8 taskId);
static void DebugTask_HandleMenuInput_Weather(u8 taskId);
static void DebugTask_TrackNpcInput(u8 taskId);
static void DebugTask_CloudCoverInput(u8 taskId);
static void DebugTask_CloudBrightnessInput(u8 taskId);

static void DebugAction_OpenCameraMenu(u8 taskId);
static void DebugAction_OpenWeatherMenu(u8 taskId);
static void DebugAction_Weather_SetCloudCover(u8 taskId);
static void DebugAction_Weather_SetCloudBrightness(u8 taskId);
static void DebugAction_Weather_Cancel(u8 taskId);
static void DebugAction_Cancel(u8 taskId);
static void DebugAction_Camera_ToggleFreecam(u8 taskId);
static void DebugAction_Camera_TrackNPC(u8 taskId);
static void DebugAction_Camera_PanTile(u8 taskId);
static void DebugAction_Camera_PanNPC(u8 taskId);
static void DebugAction_Camera_Reset(u8 taskId);
static void DebugAction_Camera_Cancel(u8 taskId);
static void Debug_OpenPanTileBox(void);
static void Debug_DrawPanTileBox(void);
static void Debug_TearDownPanTileBox(u8 taskId);
static void Debug_CommitPanTileBox(u8 taskId);
static void Debug_CancelPanTileBox(u8 taskId);
static void DebugTask_PanTileInput(u8 taskId);
static void DebugTask_WaitPanToTile(u8 taskId);

// Tracks whether a debug menu window is currently displayed. While a debug menu/box is
// open the player's field controls are locked so the D-pad drives the menu (and freecam
// scrolling pauses); closing it hands input back.
static bool8 sDebugMenuOpen;

// "Track NPC" numeric scroll box state. sTrackNpcOrder walks the map's object-event
// list: 0 = player, 1..N = the N object event templates in order. This covers every
// object event in the map (spawned or culled) with a contiguous, stable index.
static u8 sTrackNpcWindowId;
static u8 sTrackNpcArrowTaskId;
static u16 sTrackNpcOrder;  // position in the player+templates list
static u8 sTrackNpcMaxOrder; // == object event count, for the scroll arrows
static u8 sTrackNpcBoxMode; // DEBUG_NPC_BOX_MODE_*: track (live jump) vs pan (commit with A)

// "Pan to tile" coordinate box state. The D-pad edits the destination tile (left/right = X,
// up/down = Y); A commits the pan, B cancels back to the camera menu.
static u8 sPanTileWindowId;
static s16 sPanTileX;
static s16 sPanTileY;

// "Set Cloud Cover" numeric scroll box state. Up/down adjust the value in [0, CLOUD_COVER_SETS-1];
// A commits it via SetCloudCover, B backs out to the weather menu.
static u8 sCloudCoverWindowId;
static u8 sCloudCoverArrowTaskId;
static u16 sCloudCoverValue;

// "Set Cloud Brightness" numeric scroll box state. The value is an index into the non-zero
// brightness scale {-2,-1,+1,+2,+3} (stage 0 is skipped); A commits via SetCloudBrightness.
static u8 sCloudBrightnessWindowId;
static u8 sCloudBrightnessArrowTaskId;
static u16 sCloudBrightnessValue; // index in [0, NUM_CLOUD_BRIGHTNESS_LEVELS - 1]

// Writable copy of the camera submenu items, refilled from the const template each time the menu is
// shown, so the freecam label can be swapped (debug.o's .data is discarded by the linker).
static struct ListMenuItem sCameraMenuItems[DEBUG_CAMERA_ITEM_CANCEL + 1];

static const u8 sDebugText_Camera[] = _("Camera");
static const u8 sDebugText_Weather[] = _("Weather");
static const u8 sDebugText_Weather_SetCloudCover[] = _("Set Cloud Cover");
static const u8 sDebugText_CloudCover_Label[] = _("Cover ");
static const u8 sDebugText_Weather_SetCloudBrightness[] = _("Set Cloud Brightness");
static const u8 sDebugText_CloudBrightness_Label[] = _("Bright ");
static const u8 sDebugText_Minus[] = _("-");
static const u8 sDebugText_Cancel[] = _("Cancel");
static const u8 sDebugText_Camera_EnableFreecam[] = _("Enable freecam");
static const u8 sDebugText_Camera_DisableFreecam[] = _("Disable freecam");
static const u8 sDebugText_Camera_TrackNPC[] = _("Track NPC");
static const u8 sDebugText_Camera_PanNPC[] = _("Pan to NPC");
static const u8 sDebugText_Camera_PanTile[] = _("Pan to tile");
static const u8 sDebugText_Camera_Reset[] = _("Reset camera");
static const u8 sDebugText_TrackNpc_Label[] = _("Track ");
static const u8 sDebugText_PanNpc_Label[] = _("Pan ");
static const u8 sDebugText_PanTile_X[] = _("X: ");
static const u8 sDebugText_PanTile_Y[] = _(", Y: ");

static const struct ListMenuItem sDebugMenuItems_Main[] =
{
    [DEBUG_MENU_ITEM_CAMERA]  = {sDebugText_Camera, DEBUG_MENU_ITEM_CAMERA},
    [DEBUG_MENU_ITEM_WEATHER] = {sDebugText_Weather, DEBUG_MENU_ITEM_WEATHER},
    [DEBUG_MENU_ITEM_CANCEL]  = {sDebugText_Cancel, DEBUG_MENU_ITEM_CANCEL},
};

static const struct ListMenuItem sDebugMenuItems_Weather[] =
{
    [DEBUG_WEATHER_ITEM_SET_CLOUD_COVER]      = {sDebugText_Weather_SetCloudCover, DEBUG_WEATHER_ITEM_SET_CLOUD_COVER},
    [DEBUG_WEATHER_ITEM_SET_CLOUD_BRIGHTNESS] = {sDebugText_Weather_SetCloudBrightness, DEBUG_WEATHER_ITEM_SET_CLOUD_BRIGHTNESS},
    [DEBUG_WEATHER_ITEM_CANCEL]               = {sDebugText_Cancel, DEBUG_WEATHER_ITEM_CANCEL},
};

static const struct ListMenuItem sDebugMenuItems_Camera[] =
{
    [DEBUG_CAMERA_ITEM_TOGGLE_FREECAM] = {sDebugText_Camera_EnableFreecam, DEBUG_CAMERA_ITEM_TOGGLE_FREECAM},
    [DEBUG_CAMERA_ITEM_TRACK_NPC]      = {sDebugText_Camera_TrackNPC, DEBUG_CAMERA_ITEM_TRACK_NPC},
    [DEBUG_CAMERA_ITEM_PAN_TILE]       = {sDebugText_Camera_PanTile, DEBUG_CAMERA_ITEM_PAN_TILE},
    [DEBUG_CAMERA_ITEM_PAN_NPC]        = {sDebugText_Camera_PanNPC, DEBUG_CAMERA_ITEM_PAN_NPC},
    [DEBUG_CAMERA_ITEM_RESET]          = {sDebugText_Camera_Reset, DEBUG_CAMERA_ITEM_RESET},
    [DEBUG_CAMERA_ITEM_CANCEL]         = {sDebugText_Cancel, DEBUG_CAMERA_ITEM_CANCEL},
};

static void (*const sDebugMenuActions_Main[])(u8) =
{
    [DEBUG_MENU_ITEM_CAMERA]  = DebugAction_OpenCameraMenu,
    [DEBUG_MENU_ITEM_WEATHER] = DebugAction_OpenWeatherMenu,
    [DEBUG_MENU_ITEM_CANCEL]  = DebugAction_Cancel,
};

static void (*const sDebugMenuActions_Weather[])(u8) =
{
    [DEBUG_WEATHER_ITEM_SET_CLOUD_COVER]      = DebugAction_Weather_SetCloudCover,
    [DEBUG_WEATHER_ITEM_SET_CLOUD_BRIGHTNESS] = DebugAction_Weather_SetCloudBrightness,
    [DEBUG_WEATHER_ITEM_CANCEL]               = DebugAction_Weather_Cancel,
};

static void (*const sDebugMenuActions_Camera[])(u8) =
{
    [DEBUG_CAMERA_ITEM_TOGGLE_FREECAM] = DebugAction_Camera_ToggleFreecam,
    [DEBUG_CAMERA_ITEM_TRACK_NPC]      = DebugAction_Camera_TrackNPC,
    [DEBUG_CAMERA_ITEM_PAN_TILE]       = DebugAction_Camera_PanTile,
    [DEBUG_CAMERA_ITEM_PAN_NPC]        = DebugAction_Camera_PanNPC,
    [DEBUG_CAMERA_ITEM_RESET]          = DebugAction_Camera_Reset,
    [DEBUG_CAMERA_ITEM_CANCEL]         = DebugAction_Camera_Cancel,
};

static const struct ListMenuTemplate sDebugMenuListTemplate =
{
    .items = NULL,
    .moveCursorFunc = ListMenuDefaultCursorMoveFunc,
    .itemPrintFunc = NULL,
    .totalItems = 0,
    .maxShowed = 0,
    .windowId = 0,
    .header_X = 0,
    .item_X = 8,
    .cursor_X = 0,
    .upText_Y = 1,
    .cursorPal = 2,
    .fillValue = 1,
    .cursorShadowPal = 3,
    .lettersSpacing = 1,
    .itemVerticalPadding = 0,
    .scrollMultiple = LIST_NO_MULTIPLE_SCROLL,
    .fontId = FONT_NORMAL,
    .cursorKind = CURSOR_BLACK_ARROW,
};

void Debug_ShowMainMenu(void)
{
    // Free any active map-name popup's window/BG data before the menu draws its own window.
    HideMapNamePopUpWindow();

    // When opening from normal play, freeze the player and field. When detached (freecam/tracking)
    // the player is already parked, so leave the world be. Either way lock controls so the D-pad
    // drives the menu and freecam scrolling pauses.
    if (!IsCameraDetachedFromPlayer())
    {
        PlayerFreeze();
        StopPlayerAvatar();
    }
    LockPlayerFieldControls();
    Debug_OpenMainMenu();
}

static void Debug_OpenMainMenu(void)
{
    Debug_ShowMenu(sDebugMenuItems_Main, ARRAY_COUNT(sDebugMenuItems_Main), DebugTask_HandleMenuInput_Main);
}

static void Debug_ShowMenu(const struct ListMenuItem *items, u16 numItems, void (*handleInput)(u8))
{
    struct ListMenuTemplate listTemplate;
    u8 windowId;
    u8 menuTaskId;
    u8 inputTaskId;

    LoadMessageBoxAndBorderGfx();
    windowId = CreateWindowFromRect(0, 0, 14, numItems * 2);
    DrawStdWindowFrame(windowId, FALSE);

    listTemplate = sDebugMenuListTemplate;
    listTemplate.items = items;
    listTemplate.totalItems = numItems;
    listTemplate.maxShowed = numItems;
    listTemplate.windowId = windowId;

    menuTaskId = ListMenuInit(&listTemplate, 0, 0);
    CopyWindowToVram(windowId, COPYWIN_FULL);

    inputTaskId = CreateTask(handleInput, 3);
    gTasks[inputTaskId].tMenuTaskId = menuTaskId;
    gTasks[inputTaskId].tWindowId = windowId;
    sDebugMenuOpen = TRUE;
}

// Tears down the current menu's list, window and input task, but leaves the field locked
// so a different submenu (or a terminal close action) can take over.
static void Debug_DestroyMenu(u8 taskId)
{
    DestroyListMenuTask(gTasks[taskId].tMenuTaskId, NULL, NULL);
    ClearStdWindowAndFrameToTransparent(gTasks[taskId].tWindowId, TRUE);
    RemoveWindow(gTasks[taskId].tWindowId);
    DestroyTask(taskId);
    sDebugMenuOpen = FALSE;
}

// Closes the menu entirely and hands input back. Controls are always unlocked; while the camera is
// still detached the overworld keeps the player parked, but the start menu and L+R+Select stay
// reachable.
static void Debug_CloseMenu(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    UnlockPlayerFieldControls();
}

static void DebugTask_HandleMenuInput_Main(u8 taskId)
{
    void (*func)(u8);
    s32 input = ListMenu_ProcessInput(gTasks[taskId].tMenuTaskId);

    if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        if ((func = sDebugMenuActions_Main[input]) != NULL)
            func(taskId);
    }
    else if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        Debug_CloseMenu(taskId);
    }
}

static void DebugTask_HandleMenuInput_Camera(u8 taskId)
{
    void (*func)(u8);
    s32 input = ListMenu_ProcessInput(gTasks[taskId].tMenuTaskId);

    if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        if ((func = sDebugMenuActions_Camera[input]) != NULL)
            func(taskId);
    }
    else if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        DebugAction_Camera_Cancel(taskId);
    }
}

// Shows the camera submenu, first setting the freecam entry's label to "Disable freecam" while
// freecam is running, "Enable freecam" otherwise.
static void Debug_ShowCameraMenu(void)
{
    u32 i;

    for (i = 0; i < ARRAY_COUNT(sDebugMenuItems_Camera); i++)
        sCameraMenuItems[i] = sDebugMenuItems_Camera[i];
    sCameraMenuItems[DEBUG_CAMERA_ITEM_TOGGLE_FREECAM].name =
        IsFreecamActive() ? sDebugText_Camera_DisableFreecam : sDebugText_Camera_EnableFreecam;
    Debug_ShowMenu(sCameraMenuItems, ARRAY_COUNT(sCameraMenuItems), DebugTask_HandleMenuInput_Camera);
}

static void DebugAction_OpenCameraMenu(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    Debug_ShowCameraMenu();
}

static void DebugAction_Cancel(u8 taskId)
{
    Debug_CloseMenu(taskId);
}

static void DebugAction_Camera_ToggleFreecam(u8 taskId)
{
    if (IsFreecamActive())
        Debug_ReturnCameraToPlayer(taskId);
    else
        Debug_EnableFreecam(taskId);
}

// Detaches the camera from the player and lets the D-pad drive it. When opening from NPC tracking
// the camera is still anchored to that NPC, so drop the anchor here or the D-pad can't move it.
static void Debug_EnableFreecam(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    if (GetCameraTrackedLocalId() != 0)
        StopCameraObjectTracking();
    SetFreecamActive(TRUE);
    UnlockPlayerFieldControls();
}

// Returns the camera to the player from any detached state (freecam, NPC tracking, or a pan/tile
// rest) and hands control back. RecenterCameraOnPlayer runs before clearing the freecam flag so the
// location's graphics swap stays silent. Shared by the freecam toggle and the Reset action.
static void Debug_ReturnCameraToPlayer(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    RecenterCameraOnPlayer();
    SetFreecamActive(FALSE);
    UnlockPlayerFieldControls();
}

// Opens the numeric scroll box used to pick which object event the camera tracks.
static void DebugAction_Camera_TrackNPC(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    Debug_OpenNpcBox(DEBUG_NPC_BOX_MODE_TRACK);
}

// Tests PanCameraToTile: opens a box to enter a destination tile, then glides the camera there.
// The box edits the X/Y tile with the D-pad and commits with A (see DebugTask_PanTileInput).
static void DebugAction_Camera_PanTile(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    Debug_OpenPanTileBox();
}

// Redraws the coordinate box as "Xnnn Ynnn" from the current sPanTileX/sPanTileY.
static void Debug_DrawPanTileBox(void)
{
    u8 text[20];

    StringCopy(text, sDebugText_PanTile_X);
    ConvertIntToDecimalStringN(gStringVar1, sPanTileX, STR_CONV_MODE_LEFT_ALIGN, 3);
    StringAppend(text, gStringVar1);
    StringAppend(text, sDebugText_PanTile_Y);
    ConvertIntToDecimalStringN(gStringVar1, sPanTileY, STR_CONV_MODE_LEFT_ALIGN, 3);
    StringAppend(text, gStringVar1);

    FillWindowPixelBuffer(sPanTileWindowId, PIXEL_FILL(1));
    AddTextPrinterParameterized(sPanTileWindowId, FONT_NORMAL, text, 4, 1, TEXT_SKIP_DRAW, NULL);
    CopyWindowToVram(sPanTileWindowId, COPYWIN_GFX);
}

// Opens the "Pan to tile" coordinate box, starting on the camera's current focus tile.
static void Debug_OpenPanTileBox(void)
{
    sPanTileX = gCameraPos.x;
    sPanTileY = gCameraPos.y;

    LoadMessageBoxAndBorderGfx();
    sPanTileWindowId = CreateWindowFromRect(0, 1, 14, 2);
    DrawStdWindowFrame(sPanTileWindowId, FALSE);
    Debug_DrawPanTileBox();
    CopyWindowToVram(sPanTileWindowId, COPYWIN_FULL);

    CreateTask(DebugTask_PanTileInput, 3);
    sDebugMenuOpen = TRUE;
}

// Tears down the coordinate box's window and input task. The caller decides what to do next.
static void Debug_TearDownPanTileBox(u8 taskId)
{
    ClearStdWindowAndFrameToTransparent(sPanTileWindowId, TRUE);
    RemoveWindow(sPanTileWindowId);
    DestroyTask(taskId);
    sDebugMenuOpen = FALSE;
}

// Commits the box (A): closes it and glides the camera to the chosen tile, then hands input back so
// the camera travels while the world runs. DebugTask_WaitPanToTile drops into freecam once the
// camera settles, so the result can be driven around.
static void Debug_CommitPanTileBox(u8 taskId)
{
    s16 x = sPanTileX;
    s16 y = sPanTileY;

    Debug_TearDownPanTileBox(taskId);
    PanCameraToTile(x, y, DEBUG_PAN_FRAMES);
    UnlockPlayerFieldControls();
    CreateTask(DebugTask_WaitPanToTile, 3);
}

// Backs out of the box (B) without moving the camera, returning to the camera submenu.
static void Debug_CancelPanTileBox(u8 taskId)
{
    Debug_TearDownPanTileBox(taskId);
    Debug_ShowCameraMenu();
}

// Drives the coordinate box: left/right edit the X tile, up/down edit the Y tile (held to repeat),
// each clamped to the map bounds. A commits the pan, B cancels back to the camera menu.
static void DebugTask_PanTileInput(u8 taskId)
{
    int width = gMapHeader.mapLayout->width;
    int height = gMapHeader.mapLayout->height;
    bool8 changed = FALSE;

    if (JOY_REPEAT(DPAD_RIGHT) && sPanTileX < width - 1)
    {
        sPanTileX++;
        changed = TRUE;
    }
    else if (JOY_REPEAT(DPAD_LEFT) && sPanTileX > 0)
    {
        sPanTileX--;
        changed = TRUE;
    }
    if (JOY_REPEAT(DPAD_DOWN) && sPanTileY < height - 1)
    {
        sPanTileY++;
        changed = TRUE;
    }
    else if (JOY_REPEAT(DPAD_UP) && sPanTileY > 0)
    {
        sPanTileY--;
        changed = TRUE;
    }

    if (changed)
    {
        PlaySE(SE_SELECT);
        Debug_DrawPanTileBox();
    }
    else if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        Debug_CommitPanTileBox(taskId);
    }
    else if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        Debug_CancelPanTileBox(taskId);
    }
}

// Polls the tile pan started by Debug_CommitPanTileBox. Once the camera settles, switch to freecam
// so the D-pad drives it. If the pan is superseded before it arrives, just stop waiting.
static void DebugTask_WaitPanToTile(u8 taskId)
{
    if (!IsCameraPanActive())
        DestroyTask(taskId);
    else if (HasCameraPanArrived())
    {
        SetFreecamActive(TRUE);
        DestroyTask(taskId);
    }
}

// Opens the same selection box in "pan" mode: scrolling only previews the choice; A smoothly pans
// the camera to the picked object, B steps back to the camera menu without moving it.
static void DebugAction_Camera_PanNPC(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    Debug_OpenNpcBox(DEBUG_NPC_BOX_MODE_PAN);
}

static void DebugAction_Camera_Reset(u8 taskId)
{
    Debug_ReturnCameraToPlayer(taskId);
}

static void DebugAction_Camera_Cancel(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    Debug_OpenMainMenu();
}

// A template is trackable only if the map currently has it spawned (its flag isn't set), matching
// the engine's own spawn test, so hidden/despawned object events are skipped.
static bool8 Debug_TemplateIsTrackable(const struct ObjectEventTemplate *template)
{
    return !FlagGet(template->flagId);
}

// Number of trackable (spawned) object event templates in the current map.
static u8 Debug_TrackedObjectCount(void)
{
    u8 i, count = 0;

    if (gMapHeader.events == NULL)
        return 0;
    for (i = 0; i < gMapHeader.events->objectEventCount; i++)
    {
        if (Debug_TemplateIsTrackable(&gSaveBlock1Ptr->objectEventTemplates[i]))
            count++;
    }
    return count;
}

// Maps a list position to its track id: 0 -> player; k -> the k-th trackable template's
// local id (hidden/despawned templates are skipped over).
static u8 Debug_LocalIdForOrder(u8 order)
{
    u8 i, count = 0;

    if (order == 0)
        return 0; // player
    if (gMapHeader.events == NULL)
        return 0;
    for (i = 0; i < gMapHeader.events->objectEventCount; i++)
    {
        if (Debug_TemplateIsTrackable(&gSaveBlock1Ptr->objectEventTemplates[i]) && ++count == order)
            return gSaveBlock1Ptr->objectEventTemplates[i].localId;
    }
    return 0;
}

// Inverse of Debug_LocalIdForOrder, so reopening the box lands on the tracked object.
static u8 Debug_OrderForLocalId(u8 localId)
{
    u8 i, count = 0;

    if (localId == 0)
        return 0;
    if (gMapHeader.events == NULL)
        return 0;
    for (i = 0; i < gMapHeader.events->objectEventCount; i++)
    {
        if (Debug_TemplateIsTrackable(&gSaveBlock1Ptr->objectEventTemplates[i]))
        {
            count++;
            if (gSaveBlock1Ptr->objectEventTemplates[i].localId == localId)
                return count;
        }
    }
    return 0;
}

// Points the camera at the chosen track id, taking over from any running freecam. SetCameraTrackedLocalId
// runs first, while freecam is still flagged active, so the location swap stays silent.
static void Debug_SetCameraTrack(u8 localId)
{
    SetCameraTrackedLocalId(localId);
    if (IsFreecamActive())
        SetFreecamActive(FALSE);
}

static void Debug_DrawTrackNpcBox(void)
{
    u8 text[16];

    StringCopy(text, sTrackNpcBoxMode == DEBUG_NPC_BOX_MODE_PAN ? sDebugText_PanNpc_Label : sDebugText_TrackNpc_Label);
    ConvertIntToDecimalStringN(gStringVar1, Debug_LocalIdForOrder(sTrackNpcOrder), STR_CONV_MODE_LEADING_ZEROS, 3);
    StringAppend(text, gStringVar1);

    FillWindowPixelBuffer(sTrackNpcWindowId, PIXEL_FILL(1));
    AddTextPrinterParameterized(sTrackNpcWindowId, FONT_NORMAL, text, 4, 1, TEXT_SKIP_DRAW, NULL);
    CopyWindowToVram(sTrackNpcWindowId, COPYWIN_GFX);
}

static void Debug_OpenNpcBox(u8 mode)
{
    sTrackNpcBoxMode = mode;
    // Start on whatever the camera is already tracking (the player by default).
    sTrackNpcOrder = Debug_OrderForLocalId(GetCameraTrackedLocalId());
    sTrackNpcMaxOrder = Debug_TrackedObjectCount();

    LoadMessageBoxAndBorderGfx();
    sTrackNpcWindowId = CreateWindowFromRect(0, 1, 9, 2);
    DrawStdWindowFrame(sTrackNpcWindowId, FALSE);
    Debug_DrawTrackNpcBox();
    CopyWindowToVram(sTrackNpcWindowId, COPYWIN_FULL);

    // Build the arrow pair by hand so the hide-at-bounds thresholds match the reversed controls (up
    // increases the value, down decreases it): the up arrow hides at the max, the down arrow at 0.
    {
        struct ScrollArrowsTemplate arrows;

        arrows.firstArrowType = SCROLL_ARROW_UP;
        arrows.firstX = 44;
        arrows.firstY = 12;
        arrows.secondArrowType = SCROLL_ARROW_DOWN;
        arrows.secondX = 44;
        arrows.secondY = 36;
        arrows.fullyUpThreshold = sTrackNpcMaxOrder;
        arrows.fullyDownThreshold = 0;
        arrows.tileTag = TAG_TRACK_NPC_SCROLL_ARROW;
        arrows.palTag = TAG_TRACK_NPC_SCROLL_ARROW;
        arrows.palNum = 0;
        sTrackNpcArrowTaskId = AddScrollIndicatorArrowPair(&arrows, &sTrackNpcOrder);
    }

    CreateTask(DebugTask_TrackNpcInput, 3);
    sDebugMenuOpen = TRUE;
    // Track mode jumps the camera live as you scroll; pan mode only previews and waits for A.
    if (mode == DEBUG_NPC_BOX_MODE_TRACK)
        Debug_SetCameraTrack(Debug_LocalIdForOrder(sTrackNpcOrder));
}

// Tears down the box's window, scroll arrows and input task. Shared by every box exit path;
// the caller decides what field/camera state to restore afterwards.
static void Debug_TearDownNpcBox(u8 taskId)
{
    RemoveScrollIndicatorArrowPair(sTrackNpcArrowTaskId);
    ClearStdWindowAndFrameToTransparent(sTrackNpcWindowId, TRUE);
    RemoveWindow(sTrackNpcWindowId);
    DestroyTask(taskId);
    sDebugMenuOpen = FALSE;
}

// Closes the box but leaves the camera tracking the selected object, handing input back. When
// tracking the player (id 0) this returns normal control; when tracking an NPC the overworld keeps
// the player parked while L+R+Select still reopens the box to change or stop tracking.
static void Debug_CloseTrackNpcBox(u8 taskId)
{
    Debug_TearDownNpcBox(taskId);
    UnlockPlayerFieldControls();
}

// Commits the pan box (A): starts the camera gliding to the selected object and hands input back so
// the world runs while it travels. PanCameraToLocalId chases the target's live position and locks
// on at arrival, after which the overworld keeps the player parked just as ordinary tracking does.
static void Debug_CommitPanNpcBox(u8 taskId)
{
    u8 localId = Debug_LocalIdForOrder(sTrackNpcOrder);

    Debug_TearDownNpcBox(taskId);
    PanCameraToLocalId(localId, DEBUG_PAN_FRAMES);
    UnlockPlayerFieldControls();
}

// Backs out of the pan box (B) without moving the camera, returning to the camera submenu.
static void Debug_CancelPanNpcBox(u8 taskId)
{
    Debug_TearDownNpcBox(taskId);
    Debug_ShowCameraMenu();
}

static void DebugTask_TrackNpcInput(u8 taskId)
{
    bool8 trackMode = (sTrackNpcBoxMode == DEBUG_NPC_BOX_MODE_TRACK);

    if (JOY_REPEAT(DPAD_UP) && sTrackNpcOrder < sTrackNpcMaxOrder)
    {
        sTrackNpcOrder++;
        PlaySE(SE_SELECT);
        Debug_DrawTrackNpcBox();
        if (trackMode)
            Debug_SetCameraTrack(Debug_LocalIdForOrder(sTrackNpcOrder));
    }
    else if (JOY_REPEAT(DPAD_DOWN) && sTrackNpcOrder > 0)
    {
        sTrackNpcOrder--;
        PlaySE(SE_SELECT);
        Debug_DrawTrackNpcBox();
        if (trackMode)
            Debug_SetCameraTrack(Debug_LocalIdForOrder(sTrackNpcOrder));
    }
    else if (!trackMode && JOY_NEW(A_BUTTON))
    {
        // Pan mode commits the selection: glide the camera to the chosen object.
        PlaySE(SE_SELECT);
        Debug_CommitPanNpcBox(taskId);
    }
    else if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        // Track mode keeps its live selection and returns to the field; pan mode discards the
        // (uncommitted) selection and steps back to the camera menu.
        if (trackMode)
            Debug_CloseTrackNpcBox(taskId);
        else
            Debug_CancelPanNpcBox(taskId);
    }
}

static void DebugTask_HandleMenuInput_Weather(u8 taskId)
{
    void (*func)(u8);
    s32 input = ListMenu_ProcessInput(gTasks[taskId].tMenuTaskId);

    if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        if ((func = sDebugMenuActions_Weather[input]) != NULL)
            func(taskId);
    }
    else if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        DebugAction_Weather_Cancel(taskId);
    }
}

static void Debug_ShowWeatherMenu(void)
{
    Debug_ShowMenu(sDebugMenuItems_Weather, ARRAY_COUNT(sDebugMenuItems_Weather), DebugTask_HandleMenuInput_Weather);
}

static void DebugAction_OpenWeatherMenu(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    Debug_ShowWeatherMenu();
}

static void DebugAction_Weather_Cancel(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    Debug_OpenMainMenu();
}

// Opens the numeric scroll box used to pick a target cloud cover.
static void DebugAction_Weather_SetCloudCover(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    Debug_OpenCloudCoverBox();
}

// Redraws the box as "Cover n" from the current sCloudCoverValue.
static void Debug_DrawCloudCoverBox(void)
{
    u8 text[16];

    StringCopy(text, sDebugText_CloudCover_Label);
    ConvertIntToDecimalStringN(gStringVar1, sCloudCoverValue, STR_CONV_MODE_LEADING_ZEROS, 1);
    StringAppend(text, gStringVar1);

    FillWindowPixelBuffer(sCloudCoverWindowId, PIXEL_FILL(1));
    AddTextPrinterParameterized(sCloudCoverWindowId, FONT_NORMAL, text, 4, 1, TEXT_SKIP_DRAW, NULL);
    CopyWindowToVram(sCloudCoverWindowId, COPYWIN_GFX);
}

// Opens the cloud cover box, starting on the current target so reopening lands where you left off.
static void Debug_OpenCloudCoverBox(void)
{
    sCloudCoverValue = gWeatherPtr->targetCloudCover;

    LoadMessageBoxAndBorderGfx();
    sCloudCoverWindowId = CreateWindowFromRect(0, 1, 9, 2);
    DrawStdWindowFrame(sCloudCoverWindowId, FALSE);
    Debug_DrawCloudCoverBox();
    CopyWindowToVram(sCloudCoverWindowId, COPYWIN_FULL);

    {
        struct ScrollArrowsTemplate arrows;

        arrows.firstArrowType = SCROLL_ARROW_UP;
        arrows.firstX = 44;
        arrows.firstY = 12;
        arrows.secondArrowType = SCROLL_ARROW_DOWN;
        arrows.secondX = 44;
        arrows.secondY = 36;
        arrows.fullyUpThreshold = CLOUD_COVER_SETS - 1;
        arrows.fullyDownThreshold = 0;
        arrows.tileTag = TAG_CLOUD_COVER_SCROLL_ARROW;
        arrows.palTag = TAG_CLOUD_COVER_SCROLL_ARROW;
        arrows.palNum = 0;
        sCloudCoverArrowTaskId = AddScrollIndicatorArrowPair(&arrows, &sCloudCoverValue);
    }

    CreateTask(DebugTask_CloudCoverInput, 3);
    sDebugMenuOpen = TRUE;
}

// Tears down the box's window, scroll arrows and input task. The caller decides what to do next.
static void Debug_TearDownCloudCoverBox(u8 taskId)
{
    RemoveScrollIndicatorArrowPair(sCloudCoverArrowTaskId);
    ClearStdWindowAndFrameToTransparent(sCloudCoverWindowId, TRUE);
    RemoveWindow(sCloudCoverWindowId);
    DestroyTask(taskId);
    sDebugMenuOpen = FALSE;
}

// Commits the box (A): sets the target cloud cover and hands input back to the field.
static void Debug_CommitCloudCoverBox(u8 taskId)
{
    u8 value = sCloudCoverValue;

    Debug_TearDownCloudCoverBox(taskId);
    SetCloudCover(value);
    UnlockPlayerFieldControls();
}

// Backs out of the box (B) without changing anything, returning to the weather menu.
static void Debug_CancelCloudCoverBox(u8 taskId)
{
    Debug_TearDownCloudCoverBox(taskId);
    Debug_ShowWeatherMenu();
}

// Up/down adjust the value (held to repeat), clamped to [0, CLOUD_COVER_SETS-1]. A commits, B cancels.
static void DebugTask_CloudCoverInput(u8 taskId)
{
    if (JOY_REPEAT(DPAD_UP) && sCloudCoverValue < CLOUD_COVER_SETS - 1)
    {
        sCloudCoverValue++;
        PlaySE(SE_SELECT);
        Debug_DrawCloudCoverBox();
    }
    else if (JOY_REPEAT(DPAD_DOWN) && sCloudCoverValue > 0)
    {
        sCloudCoverValue--;
        PlaySE(SE_SELECT);
        Debug_DrawCloudCoverBox();
    }
    else if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        Debug_CommitCloudCoverBox(taskId);
    }
    else if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        Debug_CancelCloudCoverBox(taskId);
    }
}

// Number of selectable brightness stages: the scale {-2,-1,+1,+2,+3} with stage 0 skipped.
#define NUM_CLOUD_BRIGHTNESS_LEVELS (CLOUD_BRIGHTNESS_MAX - CLOUD_BRIGHTNESS_MIN)

// Map a box index [0, NUM_CLOUD_BRIGHTNESS_LEVELS-1] to its brightness value, skipping stage 0.
static s8 Debug_CloudBrightnessFromIndex(u16 index)
{
    s8 value = CLOUD_BRIGHTNESS_MIN + index;
    if (value >= 0)
        value++;
    return value;
}

// Inverse of Debug_CloudBrightnessFromIndex.
static u16 Debug_CloudBrightnessToIndex(s8 value)
{
    u16 index = value - CLOUD_BRIGHTNESS_MIN;
    if (value > 0)
        index--;
    return index;
}

// Opens the numeric scroll box used to pick a target cloud brightness.
static void DebugAction_Weather_SetCloudBrightness(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    Debug_OpenCloudBrightnessBox();
}

// Redraws the box as "Bright n" from the current index, showing the signed value.
static void Debug_DrawCloudBrightnessBox(void)
{
    u8 text[16];
    s8 value = Debug_CloudBrightnessFromIndex(sCloudBrightnessValue);

    StringCopy(text, sDebugText_CloudBrightness_Label);
    if (value < 0)
    {
        StringAppend(text, sDebugText_Minus);
        value = -value;
    }
    ConvertIntToDecimalStringN(gStringVar1, value, STR_CONV_MODE_LEADING_ZEROS, 1);
    StringAppend(text, gStringVar1);

    FillWindowPixelBuffer(sCloudBrightnessWindowId, PIXEL_FILL(1));
    AddTextPrinterParameterized(sCloudBrightnessWindowId, FONT_NORMAL, text, 4, 1, TEXT_SKIP_DRAW, NULL);
    CopyWindowToVram(sCloudBrightnessWindowId, COPYWIN_GFX);
}

// Opens the brightness box, starting on the current target so reopening lands where you left off.
static void Debug_OpenCloudBrightnessBox(void)
{
    sCloudBrightnessValue = Debug_CloudBrightnessToIndex(gWeatherPtr->targetCloudBrightness);

    LoadMessageBoxAndBorderGfx();
    sCloudBrightnessWindowId = CreateWindowFromRect(0, 1, 10, 2);
    DrawStdWindowFrame(sCloudBrightnessWindowId, FALSE);
    Debug_DrawCloudBrightnessBox();
    CopyWindowToVram(sCloudBrightnessWindowId, COPYWIN_FULL);

    {
        struct ScrollArrowsTemplate arrows;

        arrows.firstArrowType = SCROLL_ARROW_UP;
        arrows.firstX = 44;
        arrows.firstY = 12;
        arrows.secondArrowType = SCROLL_ARROW_DOWN;
        arrows.secondX = 44;
        arrows.secondY = 36;
        arrows.fullyUpThreshold = NUM_CLOUD_BRIGHTNESS_LEVELS - 1;
        arrows.fullyDownThreshold = 0;
        arrows.tileTag = TAG_CLOUD_BRIGHTNESS_SCROLL_ARROW;
        arrows.palTag = TAG_CLOUD_BRIGHTNESS_SCROLL_ARROW;
        arrows.palNum = 0;
        sCloudBrightnessArrowTaskId = AddScrollIndicatorArrowPair(&arrows, &sCloudBrightnessValue);
    }

    CreateTask(DebugTask_CloudBrightnessInput, 3);
    sDebugMenuOpen = TRUE;
}

// Tears down the box's window, scroll arrows and input task. The caller decides what to do next.
static void Debug_TearDownCloudBrightnessBox(u8 taskId)
{
    RemoveScrollIndicatorArrowPair(sCloudBrightnessArrowTaskId);
    ClearStdWindowAndFrameToTransparent(sCloudBrightnessWindowId, TRUE);
    RemoveWindow(sCloudBrightnessWindowId);
    DestroyTask(taskId);
    sDebugMenuOpen = FALSE;
}

// Commits the box (A): sets the target cloud brightness and hands input back to the field.
static void Debug_CommitCloudBrightnessBox(u8 taskId)
{
    s8 value = Debug_CloudBrightnessFromIndex(sCloudBrightnessValue);

    Debug_TearDownCloudBrightnessBox(taskId);
    SetCloudBrightness(value);
    UnlockPlayerFieldControls();
}

// Backs out of the box (B) without changing anything, returning to the weather menu.
static void Debug_CancelCloudBrightnessBox(u8 taskId)
{
    Debug_TearDownCloudBrightnessBox(taskId);
    Debug_ShowWeatherMenu();
}

// Up/down step the index (held to repeat); the value scale skips stage 0. A commits, B cancels.
static void DebugTask_CloudBrightnessInput(u8 taskId)
{
    if (JOY_REPEAT(DPAD_UP) && sCloudBrightnessValue < NUM_CLOUD_BRIGHTNESS_LEVELS - 1)
    {
        sCloudBrightnessValue++;
        PlaySE(SE_SELECT);
        Debug_DrawCloudBrightnessBox();
    }
    else if (JOY_REPEAT(DPAD_DOWN) && sCloudBrightnessValue > 0)
    {
        sCloudBrightnessValue--;
        PlaySE(SE_SELECT);
        Debug_DrawCloudBrightnessBox();
    }
    else if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        Debug_CommitCloudBrightnessBox(taskId);
    }
    else if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        Debug_CancelCloudBrightnessBox(taskId);
    }
}

#undef tMenuTaskId
#undef tWindowId
