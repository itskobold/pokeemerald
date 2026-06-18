#include "global.h"
#include "debug.h"
#include "event_data.h"
#include "event_object_lock.h"
#include "event_object_movement.h"
#include "field_camera.h"
#include "field_player_avatar.h"
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
}

// Shell debug menu, opened in the field by pressing Start + Select together.
// The main menu is a list of categories, each opening its own submenu.

// Main menu items
enum
{
    DEBUG_MENU_ITEM_CAMERA,
    DEBUG_MENU_ITEM_CANCEL,
};

// Camera submenu items
enum
{
    DEBUG_CAMERA_ITEM_TOGGLE_FREECAM,
    DEBUG_CAMERA_ITEM_TRACK_NPC,
    DEBUG_CAMERA_ITEM_CANCEL,
};

#define tMenuTaskId  data[0]
#define tWindowId    data[1]

#define TAG_TRACK_NPC_SCROLL_ARROW 2110

static void Debug_ShowMenu(const struct ListMenuItem *items, u16 numItems, void (*handleInput)(u8));
static void Debug_DestroyMenu(u8 taskId);
static void Debug_CloseMenu(u8 taskId);
static void Debug_OpenMainMenu(void);
static bool8 Debug_IsCameraDetached(void);
static void Debug_StartDetachedInput(void);
static void Debug_StopDetachedInput(void);
static void Debug_EnableFreecam(u8 taskId);
static void Debug_DisableFreecam(u8 taskId);
static void Debug_OpenTrackNpcBox(void);
static void Debug_CloseTrackNpcBox(u8 taskId);
static void Debug_DrawTrackNpcBox(void);
static u8 Debug_TrackedObjectCount(void);
static u8 Debug_LocalIdForOrder(u8 order);
static u8 Debug_OrderForLocalId(u8 localId);

static void DebugTask_HandleMenuInput_Main(u8 taskId);
static void DebugTask_HandleMenuInput_Camera(u8 taskId);
static void DebugTask_DetachedInput(u8 taskId);
static void DebugTask_TrackNpcInput(u8 taskId);

static void DebugAction_OpenCameraMenu(u8 taskId);
static void DebugAction_Cancel(u8 taskId);
static void DebugAction_Camera_ToggleFreecam(u8 taskId);
static void DebugAction_Camera_TrackNPC(u8 taskId);
static void DebugAction_Camera_Cancel(u8 taskId);

// Tracks whether a debug menu window is currently displayed, so the detached-camera
// input task knows not to open a second one while it sits behind an open menu.
static bool8 sDebugMenuOpen;

// "Track NPC" numeric scroll box state. sTrackNpcOrder walks the map's object-event
// list: 0 = player, 1..N = the N object event templates in order. This covers every
// object event in the map (spawned or culled) with a contiguous, stable index.
static u8 sTrackNpcWindowId;
static u8 sTrackNpcArrowTaskId;
static u16 sTrackNpcOrder;  // position in the player+templates list
static u8 sTrackNpcMaxOrder; // == object event count, for the scroll arrows

static const u8 sDebugText_Camera[] = _("Camera");
static const u8 sDebugText_Cancel[] = _("Cancel");
static const u8 sDebugText_Camera_ToggleFreecam[] = _("Toggle freecam");
static const u8 sDebugText_Camera_TrackNPC[] = _("Track NPC");
static const u8 sDebugText_TrackNpc_Label[] = _("Track ");

static const struct ListMenuItem sDebugMenuItems_Main[] =
{
    [DEBUG_MENU_ITEM_CAMERA] = {sDebugText_Camera, DEBUG_MENU_ITEM_CAMERA},
    [DEBUG_MENU_ITEM_CANCEL] = {sDebugText_Cancel, DEBUG_MENU_ITEM_CANCEL},
};

static const struct ListMenuItem sDebugMenuItems_Camera[] =
{
    [DEBUG_CAMERA_ITEM_TOGGLE_FREECAM] = {sDebugText_Camera_ToggleFreecam, DEBUG_CAMERA_ITEM_TOGGLE_FREECAM},
    [DEBUG_CAMERA_ITEM_TRACK_NPC]      = {sDebugText_Camera_TrackNPC, DEBUG_CAMERA_ITEM_TRACK_NPC},
    [DEBUG_CAMERA_ITEM_CANCEL]         = {sDebugText_Cancel, DEBUG_CAMERA_ITEM_CANCEL},
};

static void (*const sDebugMenuActions_Main[])(u8) =
{
    [DEBUG_MENU_ITEM_CAMERA] = DebugAction_OpenCameraMenu,
    [DEBUG_MENU_ITEM_CANCEL] = DebugAction_Cancel,
};

static void (*const sDebugMenuActions_Camera[])(u8) =
{
    [DEBUG_CAMERA_ITEM_TOGGLE_FREECAM] = DebugAction_Camera_ToggleFreecam,
    [DEBUG_CAMERA_ITEM_TRACK_NPC]      = DebugAction_Camera_TrackNPC,
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

// The camera is "detached" from the player while freecam is running or while tracking
// any object other than the player (local id 0). In those modes the player's controls
// stay locked so they can't walk off, and a background task watches for Start + Select.
static bool8 Debug_IsCameraDetached(void)
{
    return IsFreecamActive() || GetCameraTrackedLocalId() != 0;
}

void Debug_ShowMainMenu(void)
{
    // Tear down any active map-name popup so its window/BG data is freed before the
    // menu draws its own window.
    HideMapNamePopUpWindow();

    if (Debug_IsCameraDetached())
    {
        // The field is already locked; just halt freecam scrolling so the D-pad drives
        // the menu instead of the camera.
        if (IsFreecamActive())
            SetFreecamPaused(TRUE);
    }
    else
    {
        FreezeObjectEvents();
        PlayerFreeze();
        StopPlayerAvatar();
        LockPlayerFieldControls();
    }
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

// Tears down the current menu's list, window and input task, but leaves the
// field locked so a different submenu (or freecam) can take over.
static void Debug_DestroyMenu(u8 taskId)
{
    DestroyListMenuTask(gTasks[taskId].tMenuTaskId, NULL, NULL);
    ClearStdWindowAndFrameToTransparent(gTasks[taskId].tWindowId, TRUE);
    RemoveWindow(gTasks[taskId].tWindowId);
    DestroyTask(taskId);
    sDebugMenuOpen = FALSE;
}

// Closes the menu entirely. If the camera is detached (freecam, or tracking an NPC)
// the field stays locked and the camera mode resumes; otherwise control returns to
// the player.
static void Debug_CloseMenu(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    if (Debug_IsCameraDetached())
    {
        if (IsFreecamActive())
            SetFreecamPaused(FALSE);
    }
    else
    {
        ScriptUnfreezeObjectEvents();
        UnlockPlayerFieldControls();
    }
}

// Starts/stops the background task that lets Start + Select reopen the menu while the
// camera is detached and the player's field controls are locked.
static void Debug_StartDetachedInput(void)
{
    if (!FuncIsActiveTask(DebugTask_DetachedInput))
        CreateTask(DebugTask_DetachedInput, 3);
}

static void Debug_StopDetachedInput(void)
{
    u8 taskId = FindTaskIdByFunc(DebugTask_DetachedInput);

    if (taskId != TASK_NONE)
        DestroyTask(taskId);
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

// Runs while the camera is detached (freecam or tracking an NPC) so Start + Select can
// reopen the menu, since the player's field input handler is disabled in those modes.
static void DebugTask_DetachedInput(u8 taskId)
{
    if (sDebugMenuOpen)
        return;

    if (JOY_HELD(START_BUTTON) && JOY_HELD(SELECT_BUTTON) && JOY_NEW(START_BUTTON | SELECT_BUTTON))
    {
        PlaySE(SE_WIN_OPEN);
        Debug_ShowMainMenu();
    }
}

static void DebugAction_OpenCameraMenu(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    Debug_ShowMenu(sDebugMenuItems_Camera, ARRAY_COUNT(sDebugMenuItems_Camera), DebugTask_HandleMenuInput_Camera);
}

static void DebugAction_Cancel(u8 taskId)
{
    Debug_CloseMenu(taskId);
}

static void DebugAction_Camera_ToggleFreecam(u8 taskId)
{
    if (IsFreecamActive())
        Debug_DisableFreecam(taskId);
    else
        Debug_EnableFreecam(taskId);
}

// Detaches the camera from the player. The field is already frozen and the
// player's controls locked (the menu did this when it opened with freecam off),
// which is exactly the state freecam needs, so we just close the menu, flag the
// camera as free and spawn the task that listens for the menu being reopened.
static void Debug_EnableFreecam(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    SetFreecamActive(TRUE);
    Debug_StartDetachedInput();
}

// Reattaches the camera to the player and hands control back. RecenterCameraOnPlayer
// runs before clearing the freecam flag so the location's graphics are restored
// without the music/popup the player would otherwise see (see RecenterCameraOnPlayer).
static void Debug_DisableFreecam(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    RecenterCameraOnPlayer();
    SetFreecamActive(FALSE);
    Debug_StopDetachedInput();
    ScriptUnfreezeObjectEvents();
    UnlockPlayerFieldControls();
}

// Opens the numeric scroll box used to pick which object event the camera tracks.
// The menu already froze object events and locked controls when it opened, and we
// keep that state while scrolling (the D-pad drives the box, not the player). On
// close the camera keeps tracking the chosen object.
static void DebugAction_Camera_TrackNPC(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    Debug_OpenTrackNpcBox();
}

static void DebugAction_Camera_Cancel(u8 taskId)
{
    // Step back up to the main menu.
    Debug_DestroyMenu(taskId);
    Debug_OpenMainMenu();
}

// A template is trackable only if the map currently has it spawned, i.e. its flag isn't
// set. This matches the engine's own spawn test (see TrySpawnObjectEvents) and skips
// object events a script has hidden/despawned.
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

static void Debug_DrawTrackNpcBox(void)
{
    u8 text[16];

    StringCopy(text, sDebugText_TrackNpc_Label);
    ConvertIntToDecimalStringN(gStringVar1, Debug_LocalIdForOrder(sTrackNpcOrder), STR_CONV_MODE_LEADING_ZEROS, 3);
    StringAppend(text, gStringVar1);

    FillWindowPixelBuffer(sTrackNpcWindowId, PIXEL_FILL(1));
    AddTextPrinterParameterized(sTrackNpcWindowId, FONT_NORMAL, text, 4, 1, TEXT_SKIP_DRAW, NULL);
    CopyWindowToVram(sTrackNpcWindowId, COPYWIN_GFX);
}

static void Debug_OpenTrackNpcBox(void)
{
    // Start on whatever the camera is already tracking (the player by default).
    sTrackNpcOrder = Debug_OrderForLocalId(GetCameraTrackedLocalId());
    sTrackNpcMaxOrder = Debug_TrackedObjectCount();

    LoadMessageBoxAndBorderGfx();
    sTrackNpcWindowId = CreateWindowFromRect(0, 1, 9, 2);
    DrawStdWindowFrame(sTrackNpcWindowId, FALSE);
    Debug_DrawTrackNpcBox();
    CopyWindowToVram(sTrackNpcWindowId, COPYWIN_FULL);

    sTrackNpcArrowTaskId = AddScrollIndicatorArrowPairParameterized(
        SCROLL_ARROW_UP, 44, 12, 36, sTrackNpcMaxOrder,
        TAG_TRACK_NPC_SCROLL_ARROW, TAG_TRACK_NPC_SCROLL_ARROW, &sTrackNpcOrder);

    CreateTask(DebugTask_TrackNpcInput, 3);
    sDebugMenuOpen = TRUE;
    SetCameraTrackedLocalId(Debug_LocalIdForOrder(sTrackNpcOrder));
}

// Closes the box but leaves the camera tracking the selected object. Object events are
// unfrozen so the tracked NPC moves and the camera follows it. When tracking the player
// (id 0) the player regains control; when tracking an NPC the player's controls stay
// locked (so they can't walk off-camera) and Start + Select still reopens the box to
// change or stop tracking.
static void Debug_CloseTrackNpcBox(u8 taskId)
{
    RemoveScrollIndicatorArrowPair(sTrackNpcArrowTaskId);
    ClearStdWindowAndFrameToTransparent(sTrackNpcWindowId, TRUE);
    RemoveWindow(sTrackNpcWindowId);
    DestroyTask(taskId);
    sDebugMenuOpen = FALSE;
    ScriptUnfreezeObjectEvents();

    if (Debug_IsCameraDetached())
    {
        Debug_StartDetachedInput();
    }
    else
    {
        Debug_StopDetachedInput();
        UnlockPlayerFieldControls();
    }
}

static void DebugTask_TrackNpcInput(u8 taskId)
{
    if (JOY_REPEAT(DPAD_DOWN) && sTrackNpcOrder < sTrackNpcMaxOrder)
    {
        sTrackNpcOrder++;
        PlaySE(SE_SELECT);
        Debug_DrawTrackNpcBox();
        SetCameraTrackedLocalId(Debug_LocalIdForOrder(sTrackNpcOrder));
    }
    else if (JOY_REPEAT(DPAD_UP) && sTrackNpcOrder > 0)
    {
        sTrackNpcOrder--;
        PlaySE(SE_SELECT);
        Debug_DrawTrackNpcBox();
        SetCameraTrackedLocalId(Debug_LocalIdForOrder(sTrackNpcOrder));
    }
    else if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        Debug_CloseTrackNpcBox(taskId);
    }
}

#undef tMenuTaskId
#undef tWindowId
