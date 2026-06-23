#include "global.h"
#include "debug.h"
#include "event_data.h"
#include "event_object_movement.h"
#include "field_camera.h"
#include "field_player_avatar.h"
#include "field_screen_effect.h"
#include "fieldmap.h"
#include "field_weather.h"
#include "clock.h"
#include "overworld.h"
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
    DEBUG_MENU_ITEM_CLOCK,
    DEBUG_MENU_ITEM_CAMERA,
    DEBUG_MENU_ITEM_WEATHER,
    DEBUG_MENU_ITEM_CANCEL,
};

// Clock submenu items
enum
{
    DEBUG_CLOCK_ITEM_SET_HOURS,
    DEBUG_CLOCK_ITEM_SET_MINUTES,
    DEBUG_CLOCK_ITEM_SET_DAY,
    DEBUG_CLOCK_ITEM_SET_MONTH,
    DEBUG_CLOCK_ITEM_SET_YEAR,
    DEBUG_CLOCK_ITEM_SET_TIMESCALE,
    DEBUG_CLOCK_ITEM_CANCEL,
};

// Which clock field the shared "set value" scroll box is editing.
enum
{
    CLOCK_FIELD_HOURS,
    CLOCK_FIELD_MINUTES,
    CLOCK_FIELD_DAY,
    CLOCK_FIELD_MONTH,
    CLOCK_FIELD_YEAR,
    CLOCK_FIELD_TIMESCALE,
};

// Weather submenu items
enum
{
    DEBUG_WEATHER_ITEM_SET_CLOUD_COVER,
    DEBUG_WEATHER_ITEM_SET_CLOUD_BRIGHTNESS,
    DEBUG_WEATHER_ITEM_SET_WIND_DIRECTION,
    DEBUG_WEATHER_ITEM_SET_WIND_SPEED,
    DEBUG_WEATHER_ITEM_CANCEL,
};

// Camera submenu items
enum
{
    DEBUG_CAMERA_ITEM_TOGGLE_FREECAM,
    DEBUG_CAMERA_ITEM_TRACK_ENTITY,
    DEBUG_CAMERA_ITEM_PAN_TILE,
    DEBUG_CAMERA_ITEM_PAN_ENTITY,
    DEBUG_CAMERA_ITEM_WARP_TILE,
    DEBUG_CAMERA_ITEM_RESET,
    DEBUG_CAMERA_ITEM_CANCEL,
};

// "Track Entity" / "Pan to Entity" share one numeric scroll box; the mode decides whether choosing
// an object jumps the camera live (track) or arms a smooth pan committed with A (pan).
enum
{
    DEBUG_ENTITY_BOX_MODE_TRACK,
    DEBUG_ENTITY_BOX_MODE_PAN,
};

// Target duration (frames) for camera pans started from the debug menu, ~0.4s at 60fps.
#define DEBUG_PAN_FRAMES 24

#define tMenuTaskId  data[0]
#define tWindowId    data[1]

#define TAG_TRACK_ENTITY_SCROLL_ARROW 2110
#define TAG_CLOUD_COVER_SCROLL_ARROW 2111
#define TAG_CLOUD_BRIGHTNESS_SCROLL_ARROW 2112
#define TAG_WIND_DIRECTION_SCROLL_ARROW 2113
#define TAG_WIND_DIRECTION_LR_SCROLL_ARROW 2114
#define TAG_WIND_SPEED_SCROLL_ARROW 2115
#define TAG_CLOCK_SET_SCROLL_ARROW 2116

static void Debug_ShowMenu(const struct ListMenuItem *items, u16 numItems, void (*handleInput)(u8));
static void Debug_DestroyMenu(u8 taskId);
static void Debug_CloseMenu(u8 taskId);
static void Debug_OpenMainMenu(void);
static void Debug_DrawClockWindow(void);
static void Debug_RemoveClockWindow(void);
static void Debug_ShowCameraMenu(void);
static void Debug_ShowWeatherMenu(void);
static void Debug_ShowClockMenu(void);
static void Debug_OpenClockSetBox(u8 field);
static bool32 Debug_ClockFieldHasCoarseStep(u8 field);
static void Debug_DrawClockSetBox(void);
static void Debug_TearDownClockSetBox(u8 taskId);
static void Debug_CommitClockSetBox(u8 taskId);
static void Debug_CancelClockSetBox(u8 taskId);
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
static void Debug_OpenWindDirectionBox(void);
static void Debug_DrawWindDirectionBox(void);
static void Debug_TearDownWindDirectionBox(u8 taskId);
static void Debug_CommitWindDirectionBox(u8 taskId);
static void Debug_CancelWindDirectionBox(u8 taskId);
static void Debug_OpenWindSpeedBox(void);
static void Debug_DrawWindSpeedBox(void);
static void Debug_TearDownWindSpeedBox(u8 taskId);
static void Debug_CommitWindSpeedBox(u8 taskId);
static void Debug_CancelWindSpeedBox(u8 taskId);
static void Debug_EnableFreecam(u8 taskId);
static void Debug_ReturnCameraToPlayer(u8 taskId);
static void Debug_OpenEntityBox(u8 mode);
static void Debug_TearDownEntityBox(u8 taskId);
static void Debug_CloseTrackEntityBox(u8 taskId);
static void Debug_CommitPanEntityBox(u8 taskId);
static void Debug_CancelPanEntityBox(u8 taskId);
static void Debug_DrawTrackEntityBox(void);
static u8 Debug_TrackedObjectCount(void);
static u8 Debug_LocalIdForOrder(u8 order);
static u8 Debug_OrderForLocalId(u8 localId);

static void DebugTask_HandleMenuInput_Main(u8 taskId);
static void DebugTask_HandleMenuInput_Camera(u8 taskId);
static void DebugTask_HandleMenuInput_Weather(u8 taskId);
static void DebugTask_HandleMenuInput_Clock(u8 taskId);
static void DebugTask_ClockSetInput(u8 taskId);
static void DebugTask_TrackEntityInput(u8 taskId);
static void DebugTask_CloudCoverInput(u8 taskId);
static void DebugTask_CloudBrightnessInput(u8 taskId);
static void DebugTask_WindDirectionInput(u8 taskId);
static void DebugTask_WindSpeedInput(u8 taskId);

static void DebugAction_OpenCameraMenu(u8 taskId);
static void DebugAction_OpenWeatherMenu(u8 taskId);
static void DebugAction_OpenClockMenu(u8 taskId);
static void DebugAction_Clock_SetHours(u8 taskId);
static void DebugAction_Clock_SetMinutes(u8 taskId);
static void DebugAction_Clock_SetDay(u8 taskId);
static void DebugAction_Clock_SetMonth(u8 taskId);
static void DebugAction_Clock_SetYear(u8 taskId);
static void DebugAction_Clock_SetTimeScale(u8 taskId);
static void DebugAction_Clock_Cancel(u8 taskId);
static void DebugAction_Weather_SetCloudCover(u8 taskId);
static void DebugAction_Weather_SetCloudBrightness(u8 taskId);
static void DebugAction_Weather_SetWindDirection(u8 taskId);
static void DebugAction_Weather_SetWindSpeed(u8 taskId);
static void DebugAction_Weather_Cancel(u8 taskId);
static void DebugAction_Cancel(u8 taskId);
static void DebugAction_Camera_ToggleFreecam(u8 taskId);
static void DebugAction_Camera_TrackEntity(u8 taskId);
static void DebugAction_Camera_PanTile(u8 taskId);
static void DebugAction_Camera_PanEntity(u8 taskId);
static void DebugAction_Camera_WarpTile(u8 taskId);
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

// Read-only clock readout drawn in the bottom-right while the main menu is open.
static u8 sClockWindowId;

// Shared "set clock field" numeric scroll box. One field is edited at a time (see CLOCK_FIELD_*);
// up/down adjust the value by 1 within [sClockSetMin, sClockSetMax], A commits, B returns to the menu.
// The wide-range fields (year, time scale) also take left/right for a coarse +/-10 step, shown as
// arrow glyphs flanking the value.
static u8 sClockSetWindowId;
static u8 sClockSetArrowTaskId;
static u16 sClockSetValue;
static u8 sClockSetField;
static u16 sClockSetMin;
static u16 sClockSetMax;

#define CLOCK_SET_COARSE_STEP 10

// "Track Entity" numeric scroll box state. sTrackEntityOrder walks the map's object-event
// list: 0 = player, 1..N = the N object event templates in order. This covers every
// object event in the map (spawned or culled) with a contiguous, stable index.
static u8 sTrackEntityWindowId;
static u8 sTrackEntityArrowTaskId;
static u16 sTrackEntityOrder;  // position in the player+templates list
static u8 sTrackEntityMaxOrder; // == object event count, for the scroll arrows
static u8 sTrackEntityBoxMode; // DEBUG_ENTITY_BOX_MODE_*: track (live jump) vs pan (commit with A)

// "Pan to tile" coordinate box state. The D-pad edits the destination tile (left/right = X,
// up/down = Y); A commits the pan, B cancels back to the camera menu.
static u8 sPanTileWindowId;
static s16 sPanTileX;
static s16 sPanTileY;

// "Set cloud cover" numeric scroll box state. Up/down adjust the value in [0, CLOUD_COVER_SETS-1];
// A commits it via SetCloudCover, B backs out to the weather menu.
static u8 sCloudCoverWindowId;
static u8 sCloudCoverArrowTaskId;
static u16 sCloudCoverValue;

// "Set cloud brightness" numeric scroll box state. The value is an index into the non-zero
// brightness scale {-2,-1,+1,+2,+3} (stage 0 is skipped); A commits via SetCloudBrightness.
static u8 sCloudBrightnessWindowId;
static u8 sCloudBrightnessArrowTaskId;
static u16 sCloudBrightnessValue; // index in [0, NUM_CLOUD_BRIGHTNESS_LEVELS - 1]

// "Set wind direction" numeric scroll box state. The value is a BAM angle that wraps [0, 255];
// A commits it via SetWindDirection.
static u8 sWindDirectionWindowId;
static u8 sWindDirectionArrowTaskId;   // up/down: fine 1-unit adjust
static u8 sWindDirectionLRArrowTaskId; // left/right: jump between defined directions
static u16 sWindDirectionValue;

// "Set wind speed" numeric scroll box state. Up/down adjust the value in [0, WIND_SPEED_MAX];
// A commits it via SetWindSpeed.
static u8 sWindSpeedWindowId;
static u8 sWindSpeedArrowTaskId;
static u16 sWindSpeedValue;

// Writable copy of the camera submenu items, refilled from the const template each time the menu is
// shown, so the freecam label can be swapped (debug.o's .data is discarded by the linker).
static struct ListMenuItem sCameraMenuItems[DEBUG_CAMERA_ITEM_CANCEL + 1];

static const u8 sDebugText_Camera[] = _("Camera");
static const u8 sDebugText_Weather[] = _("Weather");
static const u8 sDebugText_Weather_SetCloudCover[] = _("Set cloud cover");
static const u8 sDebugText_CloudCover_Label[] = _("Cover ");
static const u8 sDebugText_Weather_SetCloudBrightness[] = _("Set cloud brightness");
static const u8 sDebugText_CloudBrightness_Label[] = _("Bright ");
static const u8 sDebugText_Weather_SetWindDirection[] = _("Set wind direction");
static const u8 sDebugText_WindDirection_Label[] = _("Dir ");
static const u8 sDebugText_WindDir_Sep[] = _(" ");
static const u8 sDebugText_WindDir_Below[] = _("< "); // prefix: value sits just above the name (step down to reach it)
static const u8 sDebugText_WindDir_Above[] = _(" >"); // suffix: value sits just below the name (step up to reach it)
static const u8 sDebugText_Weather_SetWindSpeed[] = _("Set wind speed");
static const u8 sDebugText_WindSpeed_Label[] = _("Speed ");

// Clock readout pieces, e.g. "9:30AM, MON.\nSEPT. 15, YR. 0".
static const u8 sDebugText_Clock_Colon[] = _(":");
static const u8 sDebugText_Clock_AM[] = _("AM");
static const u8 sDebugText_Clock_PM[] = _("PM");
static const u8 sDebugText_Clock_Comma[] = _(", ");
static const u8 sDebugText_Clock_Space[] = _(" ");
static const u8 sDebugText_Clock_YearSep[] = _(", YR. ");
static const u8 sDebugText_Clock_Newline[] = _("\n");
static const u8 sDebugText_Clock_ScaleLabel[] = _("SCALE: ");
static const u8 sDebugText_Clock_ScaleSuffix[] = _("x");
static const u8 sDebugText_Clock_SunriseLabel[] = _("SUNRISE: ");
static const u8 sDebugText_Clock_SunsetLabel[] = _("SUNSET: ");

// Clock submenu labels.
static const u8 sDebugText_Clock[] = _("Clock");
static const u8 sDebugText_Clock_SetHours[] = _("Set hours");
static const u8 sDebugText_Clock_SetMinutes[] = _("Set minutes");
static const u8 sDebugText_Clock_SetDay[] = _("Set day");
static const u8 sDebugText_Clock_SetMonth[] = _("Set month");
static const u8 sDebugText_Clock_SetYear[] = _("Set year");
static const u8 sDebugText_Clock_SetTimeScale[] = _("Set time scale");
static const u8 sDebugText_ClockSet_Hours[] = _("Hours ");
static const u8 sDebugText_ClockSet_Minutes[] = _("Minutes ");
static const u8 sDebugText_ClockSet_Day[] = _("Day ");
static const u8 sDebugText_ClockSet_Month[] = _("Month ");
static const u8 sDebugText_ClockSet_Year[] = _("Year ");
static const u8 sDebugText_ClockSet_TimeScale[] = _("Scale ");
static const u8 sDebugText_ClockSet_ArrowL[] = _("{LEFT_ARROW}");  // left/right = coarse +/-10 step
static const u8 sDebugText_ClockSet_ArrowR[] = _("{RIGHT_ARROW}");

static const u8 sDebugText_Weekday_Sun[] = _("SUN.");
static const u8 sDebugText_Weekday_Mon[] = _("MON.");
static const u8 sDebugText_Weekday_Tue[] = _("TUE.");
static const u8 sDebugText_Weekday_Wed[] = _("WED.");
static const u8 sDebugText_Weekday_Thu[] = _("THU.");
static const u8 sDebugText_Weekday_Fri[] = _("FRI.");
static const u8 sDebugText_Weekday_Sat[] = _("SAT.");

static const u8 *const sDebugClockWeekdays[] =
{
    [WEEKDAY_SUN] = sDebugText_Weekday_Sun,
    [WEEKDAY_MON] = sDebugText_Weekday_Mon,
    [WEEKDAY_TUE] = sDebugText_Weekday_Tue,
    [WEEKDAY_WED] = sDebugText_Weekday_Wed,
    [WEEKDAY_THU] = sDebugText_Weekday_Thu,
    [WEEKDAY_FRI] = sDebugText_Weekday_Fri,
    [WEEKDAY_SAT] = sDebugText_Weekday_Sat,
};

static const u8 sDebugText_Month_Jan[] = _("JAN.");
static const u8 sDebugText_Month_Feb[] = _("FEB.");
static const u8 sDebugText_Month_Mar[] = _("MAR.");
static const u8 sDebugText_Month_Apr[] = _("APR.");
static const u8 sDebugText_Month_May[] = _("MAY");
static const u8 sDebugText_Month_Jun[] = _("JUN.");
static const u8 sDebugText_Month_Jul[] = _("JUL.");
static const u8 sDebugText_Month_Aug[] = _("AUG.");
static const u8 sDebugText_Month_Sep[] = _("SEPT.");
static const u8 sDebugText_Month_Oct[] = _("OCT.");
static const u8 sDebugText_Month_Nov[] = _("NOV.");
static const u8 sDebugText_Month_Dec[] = _("DEC.");

static const u8 *const sDebugClockMonths[MONTH_COUNT] =
{
    [MONTH_JAN - 1] = sDebugText_Month_Jan,
    [MONTH_FEB - 1] = sDebugText_Month_Feb,
    [MONTH_MAR - 1] = sDebugText_Month_Mar,
    [MONTH_APR - 1] = sDebugText_Month_Apr,
    [MONTH_MAY - 1] = sDebugText_Month_May,
    [MONTH_JUN - 1] = sDebugText_Month_Jun,
    [MONTH_JUL - 1] = sDebugText_Month_Jul,
    [MONTH_AUG - 1] = sDebugText_Month_Aug,
    [MONTH_SEP - 1] = sDebugText_Month_Sep,
    [MONTH_OCT - 1] = sDebugText_Month_Oct,
    [MONTH_NOV - 1] = sDebugText_Month_Nov,
    [MONTH_DEC - 1] = sDebugText_Month_Dec,
};

// Box labels for the shared clock-field setter, indexed by CLOCK_FIELD_*.
static const u8 *const sClockSetLabels[] =
{
    [CLOCK_FIELD_HOURS]   = sDebugText_ClockSet_Hours,
    [CLOCK_FIELD_MINUTES] = sDebugText_ClockSet_Minutes,
    [CLOCK_FIELD_DAY]     = sDebugText_ClockSet_Day,
    [CLOCK_FIELD_MONTH]   = sDebugText_ClockSet_Month,
    [CLOCK_FIELD_YEAR]    = sDebugText_ClockSet_Year,
    [CLOCK_FIELD_TIMESCALE] = sDebugText_ClockSet_TimeScale,
};

// Names for the 16 defined wind directions, indexed by BAM angle >> 4 (see the WIND_DIR_* constants).
static const u8 sDebugText_WindDir_N[]   = _("NORTH");
static const u8 sDebugText_WindDir_NNE[] = _("NNE");
static const u8 sDebugText_WindDir_NE[]  = _("NORTHEAST");
static const u8 sDebugText_WindDir_ENE[] = _("ENE");
static const u8 sDebugText_WindDir_E[]   = _("EAST");
static const u8 sDebugText_WindDir_ESE[] = _("ESE");
static const u8 sDebugText_WindDir_SE[]  = _("SOUTHEAST");
static const u8 sDebugText_WindDir_SSE[] = _("SSE");
static const u8 sDebugText_WindDir_S[]   = _("SOUTH");
static const u8 sDebugText_WindDir_SSW[] = _("SSW");
static const u8 sDebugText_WindDir_SW[]  = _("SOUTHWEST");
static const u8 sDebugText_WindDir_WSW[] = _("WSW");
static const u8 sDebugText_WindDir_W[]   = _("WEST");
static const u8 sDebugText_WindDir_WNW[] = _("WNW");
static const u8 sDebugText_WindDir_NW[]  = _("NORTHWEST");
static const u8 sDebugText_WindDir_NNW[] = _("NNW");
static const u8 *const sWindDirectionNames[16] =
{
    sDebugText_WindDir_N,   sDebugText_WindDir_NNE, sDebugText_WindDir_NE,  sDebugText_WindDir_ENE,
    sDebugText_WindDir_E,   sDebugText_WindDir_ESE, sDebugText_WindDir_SE,  sDebugText_WindDir_SSE,
    sDebugText_WindDir_S,   sDebugText_WindDir_SSW, sDebugText_WindDir_SW,  sDebugText_WindDir_WSW,
    sDebugText_WindDir_W,   sDebugText_WindDir_WNW, sDebugText_WindDir_NW,  sDebugText_WindDir_NNW,
};
static const u8 sDebugText_Minus[] = _("-");
static const u8 sDebugText_Cancel[] = _("Cancel");
static const u8 sDebugText_Camera_EnableFreecam[] = _("Enable freecam");
static const u8 sDebugText_Camera_DisableFreecam[] = _("Disable freecam");
static const u8 sDebugText_Camera_TrackEntity[] = _("Track entity");
static const u8 sDebugText_Camera_PanEntity[] = _("Pan to entity");
static const u8 sDebugText_Camera_PanTile[] = _("Pan to tile");
static const u8 sDebugText_Camera_WarpTile[] = _("Warp to tile");
static const u8 sDebugText_Camera_Reset[] = _("Reset camera");
static const u8 sDebugText_TrackEntity_Label[] = _("Track ");
static const u8 sDebugText_PanEntity_Label[] = _("Pan ");
static const u8 sDebugText_PanTile_X[] = _("X: ");
static const u8 sDebugText_PanTile_Y[] = _(", Y: ");

static const struct ListMenuItem sDebugMenuItems_Main[] =
{
    [DEBUG_MENU_ITEM_CAMERA]  = {sDebugText_Camera, DEBUG_MENU_ITEM_CAMERA},
    [DEBUG_MENU_ITEM_WEATHER] = {sDebugText_Weather, DEBUG_MENU_ITEM_WEATHER},
    [DEBUG_MENU_ITEM_CLOCK]   = {sDebugText_Clock, DEBUG_MENU_ITEM_CLOCK},
    [DEBUG_MENU_ITEM_CANCEL]  = {sDebugText_Cancel, DEBUG_MENU_ITEM_CANCEL},
};

static const struct ListMenuItem sDebugMenuItems_Clock[] =
{
    [DEBUG_CLOCK_ITEM_SET_HOURS]   = {sDebugText_Clock_SetHours, DEBUG_CLOCK_ITEM_SET_HOURS},
    [DEBUG_CLOCK_ITEM_SET_MINUTES] = {sDebugText_Clock_SetMinutes, DEBUG_CLOCK_ITEM_SET_MINUTES},
    [DEBUG_CLOCK_ITEM_SET_DAY]     = {sDebugText_Clock_SetDay, DEBUG_CLOCK_ITEM_SET_DAY},
    [DEBUG_CLOCK_ITEM_SET_MONTH]   = {sDebugText_Clock_SetMonth, DEBUG_CLOCK_ITEM_SET_MONTH},
    [DEBUG_CLOCK_ITEM_SET_YEAR]    = {sDebugText_Clock_SetYear, DEBUG_CLOCK_ITEM_SET_YEAR},
    [DEBUG_CLOCK_ITEM_SET_TIMESCALE] = {sDebugText_Clock_SetTimeScale, DEBUG_CLOCK_ITEM_SET_TIMESCALE},
    [DEBUG_CLOCK_ITEM_CANCEL]      = {sDebugText_Cancel, DEBUG_CLOCK_ITEM_CANCEL},
};

static const struct ListMenuItem sDebugMenuItems_Weather[] =
{
    [DEBUG_WEATHER_ITEM_SET_CLOUD_COVER]      = {sDebugText_Weather_SetCloudCover, DEBUG_WEATHER_ITEM_SET_CLOUD_COVER},
    [DEBUG_WEATHER_ITEM_SET_CLOUD_BRIGHTNESS] = {sDebugText_Weather_SetCloudBrightness, DEBUG_WEATHER_ITEM_SET_CLOUD_BRIGHTNESS},
    [DEBUG_WEATHER_ITEM_SET_WIND_DIRECTION]   = {sDebugText_Weather_SetWindDirection, DEBUG_WEATHER_ITEM_SET_WIND_DIRECTION},
    [DEBUG_WEATHER_ITEM_SET_WIND_SPEED]       = {sDebugText_Weather_SetWindSpeed, DEBUG_WEATHER_ITEM_SET_WIND_SPEED},
    [DEBUG_WEATHER_ITEM_CANCEL]               = {sDebugText_Cancel, DEBUG_WEATHER_ITEM_CANCEL},
};

static const struct ListMenuItem sDebugMenuItems_Camera[] =
{
    [DEBUG_CAMERA_ITEM_TOGGLE_FREECAM] = {sDebugText_Camera_EnableFreecam, DEBUG_CAMERA_ITEM_TOGGLE_FREECAM},
    [DEBUG_CAMERA_ITEM_TRACK_ENTITY]      = {sDebugText_Camera_TrackEntity, DEBUG_CAMERA_ITEM_TRACK_ENTITY},
    [DEBUG_CAMERA_ITEM_PAN_TILE]       = {sDebugText_Camera_PanTile, DEBUG_CAMERA_ITEM_PAN_TILE},
    [DEBUG_CAMERA_ITEM_PAN_ENTITY]        = {sDebugText_Camera_PanEntity, DEBUG_CAMERA_ITEM_PAN_ENTITY},
    [DEBUG_CAMERA_ITEM_WARP_TILE]      = {sDebugText_Camera_WarpTile, DEBUG_CAMERA_ITEM_WARP_TILE},
    [DEBUG_CAMERA_ITEM_RESET]          = {sDebugText_Camera_Reset, DEBUG_CAMERA_ITEM_RESET},
    [DEBUG_CAMERA_ITEM_CANCEL]         = {sDebugText_Cancel, DEBUG_CAMERA_ITEM_CANCEL},
};

static void (*const sDebugMenuActions_Main[])(u8) =
{
    [DEBUG_MENU_ITEM_CAMERA]  = DebugAction_OpenCameraMenu,
    [DEBUG_MENU_ITEM_WEATHER] = DebugAction_OpenWeatherMenu,
    [DEBUG_MENU_ITEM_CLOCK]   = DebugAction_OpenClockMenu,
    [DEBUG_MENU_ITEM_CANCEL]  = DebugAction_Cancel,
};

static void (*const sDebugMenuActions_Clock[])(u8) =
{
    [DEBUG_CLOCK_ITEM_SET_HOURS]   = DebugAction_Clock_SetHours,
    [DEBUG_CLOCK_ITEM_SET_MINUTES] = DebugAction_Clock_SetMinutes,
    [DEBUG_CLOCK_ITEM_SET_DAY]     = DebugAction_Clock_SetDay,
    [DEBUG_CLOCK_ITEM_SET_MONTH]   = DebugAction_Clock_SetMonth,
    [DEBUG_CLOCK_ITEM_SET_YEAR]    = DebugAction_Clock_SetYear,
    [DEBUG_CLOCK_ITEM_SET_TIMESCALE] = DebugAction_Clock_SetTimeScale,
    [DEBUG_CLOCK_ITEM_CANCEL]      = DebugAction_Clock_Cancel,
};

static void (*const sDebugMenuActions_Weather[])(u8) =
{
    [DEBUG_WEATHER_ITEM_SET_CLOUD_COVER]      = DebugAction_Weather_SetCloudCover,
    [DEBUG_WEATHER_ITEM_SET_CLOUD_BRIGHTNESS] = DebugAction_Weather_SetCloudBrightness,
    [DEBUG_WEATHER_ITEM_SET_WIND_DIRECTION]   = DebugAction_Weather_SetWindDirection,
    [DEBUG_WEATHER_ITEM_SET_WIND_SPEED]       = DebugAction_Weather_SetWindSpeed,
    [DEBUG_WEATHER_ITEM_CANCEL]               = DebugAction_Weather_Cancel,
};

static void (*const sDebugMenuActions_Camera[])(u8) =
{
    [DEBUG_CAMERA_ITEM_TOGGLE_FREECAM] = DebugAction_Camera_ToggleFreecam,
    [DEBUG_CAMERA_ITEM_TRACK_ENTITY]      = DebugAction_Camera_TrackEntity,
    [DEBUG_CAMERA_ITEM_PAN_TILE]       = DebugAction_Camera_PanTile,
    [DEBUG_CAMERA_ITEM_PAN_ENTITY]        = DebugAction_Camera_PanEntity,
    [DEBUG_CAMERA_ITEM_WARP_TILE]      = DebugAction_Camera_WarpTile,
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
    Debug_DrawClockWindow();
}

// The clock readout has its own baseBlock so its tiles don't collide with the main menu's
// list window (CreateWindowFromRect hardcodes baseBlock 100). Bottom-right of the screen.
#define CLOCK_WIN_BASE_BLOCK 0xC0
#define CLOCK_WIN_HEIGHT     9 // six FONT_SMALL lines (12px line height)

// Appends a minutes-past-midnight value to dest as "7:04AM". Returns the end.
static u8 *Debug_AppendTimeOfDay(u8 *dest, u16 minutes)
{
    u8 hour = (minutes / 60) % 24;
    u8 minute = minutes % 60;
    bool32 isPm = hour >= 12;
    u8 hour12 = hour % 12;

    if (hour12 == 0)
        hour12 = 12;

    ConvertIntToDecimalStringN(gStringVar1, hour12, STR_CONV_MODE_LEFT_ALIGN, 2);
    dest = StringAppend(dest, gStringVar1);
    dest = StringAppend(dest, sDebugText_Clock_Colon);
    ConvertIntToDecimalStringN(gStringVar1, minute, STR_CONV_MODE_LEADING_ZEROS, 2);
    dest = StringAppend(dest, gStringVar1);
    return StringAppend(dest, isPm ? sDebugText_Clock_PM : sDebugText_Clock_AM);
}

static void Debug_DrawClockWindow(void)
{
    struct Clock *clock = &gSaveBlock1Ptr->gameClock;
    struct WindowTemplate template;
    u8 line1[24];
    u8 line2[24];
    u8 lineTimeOfDay[24];
    u8 lineSunrise[24];
    u8 lineSunset[24];
    u8 line3[24];
    u8 text[144];
    s32 width, lineWidth;
    u8 tileWidth;
    u8 hour = clock->hours % 12;
    u8 month = clock->month;

    if (hour == 0)
        hour = 12;

    // Guard the month-name lookup: a save predating the gameClock field has an
    // uninitialised (often 0) month, which would index out of bounds.
    if (month < MONTH_JAN || month > MONTH_DEC)
        month = MONTH_JAN;

    // Line 1: "9:30AM, MON."
    ConvertIntToDecimalStringN(gStringVar1, hour, STR_CONV_MODE_LEFT_ALIGN, 2);
    StringCopy(line1, gStringVar1);
    StringAppend(line1, sDebugText_Clock_Colon);
    ConvertIntToDecimalStringN(gStringVar1, clock->minutes, STR_CONV_MODE_LEADING_ZEROS, 2);
    StringAppend(line1, gStringVar1);
    StringAppend(line1, clock->hours < 12 ? sDebugText_Clock_AM : sDebugText_Clock_PM);
    StringAppend(line1, sDebugText_Clock_Comma);
    StringAppend(line1, sDebugClockWeekdays[Clock_GetDayOfWeek()]);

    // Line 2: "SEPT. 15, YR. 0"
    StringCopy(line2, sDebugClockMonths[month - 1]);
    StringAppend(line2, sDebugText_Clock_Space);
    ConvertIntToDecimalStringN(gStringVar1, clock->day, STR_CONV_MODE_LEFT_ALIGN, 2);
    StringAppend(line2, gStringVar1);
    StringAppend(line2, sDebugText_Clock_YearSep);
    ConvertIntToDecimalStringN(gStringVar1, clock->year, STR_CONV_MODE_LEFT_ALIGN, 5);
    StringAppend(line2, gStringVar1);

    // "MIDDAY"
    Clock_GetTimeOfDayString(lineTimeOfDay);

    // "SUNRISE: 7:04AM" / "SUNSET: 5:30PM"
    StringCopy(lineSunrise, sDebugText_Clock_SunriseLabel);
    Debug_AppendTimeOfDay(lineSunrise, clock->sunriseTime);
    StringCopy(lineSunset, sDebugText_Clock_SunsetLabel);
    Debug_AppendTimeOfDay(lineSunset, clock->sunsetTime);

    // Line 3: "SCALE: 20x"
    StringCopy(line3, sDebugText_Clock_ScaleLabel);
    ConvertIntToDecimalStringN(gStringVar1, clock->timeScale, STR_CONV_MODE_LEFT_ALIGN, 3);
    StringAppend(line3, gStringVar1);
    StringAppend(line3, sDebugText_Clock_ScaleSuffix);

    // Size the window to the widest line so it hugs the text.
    width = GetStringWidth(FONT_SMALL, line1, 0);
    lineWidth = GetStringWidth(FONT_SMALL, line2, 0);
    if (lineWidth > width)
        width = lineWidth;
    lineWidth = GetStringWidth(FONT_SMALL, lineTimeOfDay, 0);
    if (lineWidth > width)
        width = lineWidth;
    lineWidth = GetStringWidth(FONT_SMALL, lineSunrise, 0);
    if (lineWidth > width)
        width = lineWidth;
    lineWidth = GetStringWidth(FONT_SMALL, lineSunset, 0);
    if (lineWidth > width)
        width = lineWidth;
    lineWidth = GetStringWidth(FONT_SMALL, line3, 0);
    if (lineWidth > width)
        width = lineWidth;
    tileWidth = ConvertPixelWidthToTileWidth(width);

    template = CreateWindowTemplate(0, 29 - tileWidth, 19 - CLOCK_WIN_HEIGHT,
                                    tileWidth, CLOCK_WIN_HEIGHT, 15, CLOCK_WIN_BASE_BLOCK);
    sClockWindowId = AddWindow(&template);
    PutWindowTilemap(sClockWindowId);
    DrawStdWindowFrame(sClockWindowId, FALSE);

    StringCopy(text, line1);
    StringAppend(text, sDebugText_Clock_Newline);
    StringAppend(text, line2);
    StringAppend(text, sDebugText_Clock_Newline);
    StringAppend(text, lineTimeOfDay);
    StringAppend(text, sDebugText_Clock_Newline);
    StringAppend(text, lineSunrise);
    StringAppend(text, sDebugText_Clock_Newline);
    StringAppend(text, lineSunset);
    StringAppend(text, sDebugText_Clock_Newline);
    StringAppend(text, line3);

    FillWindowPixelBuffer(sClockWindowId, PIXEL_FILL(1));
    AddTextPrinterParameterized(sClockWindowId, FONT_SMALL, text, 2, 0, TEXT_SKIP_DRAW, NULL);
    CopyWindowToVram(sClockWindowId, COPYWIN_FULL);
}

static void Debug_RemoveClockWindow(void)
{
    if (sClockWindowId == WINDOW_NONE)
        return;

    ClearStdWindowAndFrameToTransparent(sClockWindowId, TRUE);
    RemoveWindow(sClockWindowId);
    sClockWindowId = WINDOW_NONE;
}

static void Debug_ShowMenu(const struct ListMenuItem *items, u16 numItems, void (*handleInput)(u8))
{
    struct ListMenuTemplate listTemplate;
    u8 windowId;
    u8 menuTaskId;
    u8 inputTaskId;
    int width = 0;
    u32 i;

    LoadMessageBoxAndBorderGfx();
    // Size the window to its widest item so each menu fits its own text exactly.
    for (i = 0; i < numItems; i++)
        width = DisplayTextAndGetWidth(items[i].name, width);
    windowId = CreateWindowFromRect(0, 0, ConvertPixelWidthToTileWidth(width), numItems * 2);
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
    Debug_RemoveClockWindow(); // no-op unless the main menu drew it
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
    s32 input;

    if (ListMenuTryWrapSelection(gTasks[taskId].tMenuTaskId))
        return;

    input = ListMenu_ProcessInput(gTasks[taskId].tMenuTaskId);

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
    s32 input;

    if (ListMenuTryWrapSelection(gTasks[taskId].tMenuTaskId))
        return;

    input = ListMenu_ProcessInput(gTasks[taskId].tMenuTaskId);

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

// Detaches the camera from the player and lets the D-pad drive it. When opening from Entity tracking
// the camera is still anchored to that Entity, so drop the anchor here or the D-pad can't move it.
static void Debug_EnableFreecam(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    if (GetCameraTrackedLocalId() != 0)
        StopCameraObjectTracking();
    SetFreecamActive(TRUE);
    UnlockPlayerFieldControls();
}

// Returns the camera to the player from any detached state (freecam, Entity tracking, or a pan/tile
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
static void DebugAction_Camera_TrackEntity(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    Debug_OpenEntityBox(DEBUG_ENTITY_BOX_MODE_TRACK);
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
static void DebugAction_Camera_PanEntity(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    Debug_OpenEntityBox(DEBUG_ENTITY_BOX_MODE_PAN);
}

// Warps the player to the tile the camera is centered on. The camera view anchor names the map the
// focus tile currently sits on (tracked across any number of connection hops) plus that map's origin
// offset in the home frame, so the focus resolves to a local tile no matter how far the camera has
// roamed. A full map warp is always fired, as if walking through a seam: the map loads fresh and
// ResetCameraTracking re-anchors the camera on the player (so a same-map warp re-centers too, rather
// than the player snapping in place).
static void DebugAction_Camera_WarpTile(u8 taskId)
{
    struct ViewMap anchor;
    const struct MapHeader *header;
    s16 x, y;
    int width, height;

    GetCameraViewAnchor(&anchor);
    header = Overworld_GetMapHeaderByGroupAndId(anchor.mapGroup, anchor.mapNum);
    if (header == NULL || header->mapLayout == NULL)
    {
        // Shouldn't happen; fall back to treating the focus as a home-map tile.
        anchor.mapGroup = gSaveBlock1Ptr->location.mapGroup;
        anchor.mapNum   = gSaveBlock1Ptr->location.mapNum;
        anchor.dx = anchor.dy = 0;
        header = &gMapHeader;
    }

    // Focus tile, local to the anchor map (anchor.dx/dy is that map's origin in the home frame).
    x = gCameraPos.x - anchor.dx;
    y = gCameraPos.y - anchor.dy;
    width = header->mapLayout->width;
    height = header->mapLayout->height;

    // Clamp into the map in case the focus drifted just past an edge into a border region.
    if (x < 0)
        x = 0;
    else if (x > width - 1)
        x = width - 1;
    if (y < 0)
        y = 0;
    else if (y > height - 1)
        y = height - 1;

    // Preserve the player's avatar state (bike/surf/etc.) across the warp, like an ordinary transition.
    StoreInitialPlayerAvatarState();
    SetWarpDestination(anchor.mapGroup, anchor.mapNum, WARP_ID_NONE, x, y);
    DoWarp();
    Debug_DestroyMenu(taskId);
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

static void Debug_DrawTrackEntityBox(void)
{
    u8 text[16];

    StringCopy(text, sTrackEntityBoxMode == DEBUG_ENTITY_BOX_MODE_PAN ? sDebugText_PanEntity_Label : sDebugText_TrackEntity_Label);
    ConvertIntToDecimalStringN(gStringVar1, Debug_LocalIdForOrder(sTrackEntityOrder), STR_CONV_MODE_LEADING_ZEROS, 3);
    StringAppend(text, gStringVar1);

    FillWindowPixelBuffer(sTrackEntityWindowId, PIXEL_FILL(1));
    AddTextPrinterParameterized(sTrackEntityWindowId, FONT_NORMAL, text, 4, 1, TEXT_SKIP_DRAW, NULL);
    CopyWindowToVram(sTrackEntityWindowId, COPYWIN_GFX);
}

static void Debug_OpenEntityBox(u8 mode)
{
    sTrackEntityBoxMode = mode;
    // Start on whatever the camera is already tracking (the player by default).
    sTrackEntityOrder = Debug_OrderForLocalId(GetCameraTrackedLocalId());
    sTrackEntityMaxOrder = Debug_TrackedObjectCount();

    LoadMessageBoxAndBorderGfx();
    sTrackEntityWindowId = CreateWindowFromRect(0, 1, 9, 2);
    DrawStdWindowFrame(sTrackEntityWindowId, FALSE);
    Debug_DrawTrackEntityBox();
    CopyWindowToVram(sTrackEntityWindowId, COPYWIN_FULL);

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
        arrows.fullyUpThreshold = sTrackEntityMaxOrder;
        arrows.fullyDownThreshold = 0;
        arrows.tileTag = TAG_TRACK_ENTITY_SCROLL_ARROW;
        arrows.palTag = TAG_TRACK_ENTITY_SCROLL_ARROW;
        arrows.palNum = 0;
        sTrackEntityArrowTaskId = AddScrollIndicatorArrowPair(&arrows, &sTrackEntityOrder);
    }

    CreateTask(DebugTask_TrackEntityInput, 3);
    sDebugMenuOpen = TRUE;
    // Track mode jumps the camera live as you scroll; pan mode only previews and waits for A.
    if (mode == DEBUG_ENTITY_BOX_MODE_TRACK)
        Debug_SetCameraTrack(Debug_LocalIdForOrder(sTrackEntityOrder));
}

// Tears down the box's window, scroll arrows and input task. Shared by every box exit path;
// the caller decides what field/camera state to restore afterwards.
static void Debug_TearDownEntityBox(u8 taskId)
{
    RemoveScrollIndicatorArrowPair(sTrackEntityArrowTaskId);
    ClearStdWindowAndFrameToTransparent(sTrackEntityWindowId, TRUE);
    RemoveWindow(sTrackEntityWindowId);
    DestroyTask(taskId);
    sDebugMenuOpen = FALSE;
}

// Closes the box but leaves the camera tracking the selected object, handing input back. When
// tracking the player (id 0) this returns normal control; when tracking an Entity the overworld keeps
// the player parked while L+R+Select still reopens the box to change or stop tracking.
static void Debug_CloseTrackEntityBox(u8 taskId)
{
    Debug_TearDownEntityBox(taskId);
    UnlockPlayerFieldControls();
}

// Commits the pan box (A): starts the camera gliding to the selected object and hands input back so
// the world runs while it travels. PanCameraToLocalId chases the target's live position and locks
// on at arrival, after which the overworld keeps the player parked just as ordinary tracking does.
static void Debug_CommitPanEntityBox(u8 taskId)
{
    u8 localId = Debug_LocalIdForOrder(sTrackEntityOrder);

    Debug_TearDownEntityBox(taskId);
    PanCameraToLocalId(localId, DEBUG_PAN_FRAMES);
    UnlockPlayerFieldControls();
}

// Backs out of the pan box (B) without moving the camera, returning to the camera submenu.
static void Debug_CancelPanEntityBox(u8 taskId)
{
    Debug_TearDownEntityBox(taskId);
    Debug_ShowCameraMenu();
}

static void DebugTask_TrackEntityInput(u8 taskId)
{
    bool8 trackMode = (sTrackEntityBoxMode == DEBUG_ENTITY_BOX_MODE_TRACK);

    if (JOY_REPEAT(DPAD_UP) && sTrackEntityOrder < sTrackEntityMaxOrder)
    {
        sTrackEntityOrder++;
        PlaySE(SE_SELECT);
        Debug_DrawTrackEntityBox();
        if (trackMode)
            Debug_SetCameraTrack(Debug_LocalIdForOrder(sTrackEntityOrder));
    }
    else if (JOY_REPEAT(DPAD_DOWN) && sTrackEntityOrder > 0)
    {
        sTrackEntityOrder--;
        PlaySE(SE_SELECT);
        Debug_DrawTrackEntityBox();
        if (trackMode)
            Debug_SetCameraTrack(Debug_LocalIdForOrder(sTrackEntityOrder));
    }
    else if (!trackMode && JOY_NEW(A_BUTTON))
    {
        // Pan mode commits the selection: glide the camera to the chosen object.
        PlaySE(SE_SELECT);
        Debug_CommitPanEntityBox(taskId);
    }
    else if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        // Track mode keeps its live selection and returns to the field; pan mode discards the
        // (uncommitted) selection and steps back to the camera menu.
        if (trackMode)
            Debug_CloseTrackEntityBox(taskId);
        else
            Debug_CancelPanEntityBox(taskId);
    }
}

static void DebugTask_HandleMenuInput_Weather(u8 taskId)
{
    void (*func)(u8);
    s32 input;

    if (ListMenuTryWrapSelection(gTasks[taskId].tMenuTaskId))
        return;

    input = ListMenu_ProcessInput(gTasks[taskId].tMenuTaskId);

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
    ConvertIntToDecimalStringN(gStringVar1, sCloudCoverValue, STR_CONV_MODE_LEADING_ZEROS, 2);
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

// Opens the numeric scroll box used to pick a target wind direction.
static void DebugAction_Weather_SetWindDirection(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    Debug_OpenWindDirectionBox();
}

// Redraws the box as "Dir n NAME" from the current sWindDirectionValue (BAM angle 0..255). NAME is the
// nearest defined direction; when the value is off that direction, a marker shows which side it sits on
// (e.g. 1 -> "< NORTH", 255 -> "NORTH >", 0 -> "NORTH").
static void Debug_DrawWindDirectionBox(void)
{
    u8 text[32];
    s8 offset;
    u8 nearest = GetClosestWindDirection(sWindDirectionValue, &offset);

    StringCopy(text, sDebugText_WindDirection_Label);
    ConvertIntToDecimalStringN(gStringVar1, sWindDirectionValue, STR_CONV_MODE_LEADING_ZEROS, 3);
    StringAppend(text, gStringVar1);
    StringAppend(text, sDebugText_WindDir_Sep);
    if (offset > 0)
        StringAppend(text, sDebugText_WindDir_Below);
    StringAppend(text, sWindDirectionNames[nearest >> 4]);
    if (offset < 0)
        StringAppend(text, sDebugText_WindDir_Above);

    FillWindowPixelBuffer(sWindDirectionWindowId, PIXEL_FILL(1));
    AddTextPrinterParameterized(sWindDirectionWindowId, FONT_NORMAL, text, 4, 1, TEXT_SKIP_DRAW, NULL);
    CopyWindowToVram(sWindDirectionWindowId, COPYWIN_GFX);
}

// Opens the wind direction box, starting on the current target so reopening lands where you left off.
static void Debug_OpenWindDirectionBox(void)
{
    sWindDirectionValue = gWeatherPtr->targetWindDirection;

    LoadMessageBoxAndBorderGfx();
    sWindDirectionWindowId = CreateWindowFromRect(0, 1, 15, 2); // snug fit for "Dir 255 < NORTHEAST"
    DrawStdWindowFrame(sWindDirectionWindowId, FALSE);
    Debug_DrawWindDirectionBox();
    CopyWindowToVram(sWindDirectionWindowId, COPYWIN_FULL);

    {
        struct ScrollArrowsTemplate arrows;

        // Up/down flank the box vertically (fine 1-unit adjust).
        arrows.firstArrowType = SCROLL_ARROW_UP;
        arrows.firstX = 44;
        arrows.firstY = 12;
        arrows.secondArrowType = SCROLL_ARROW_DOWN;
        arrows.secondX = 44;
        arrows.secondY = 36;
        arrows.fullyUpThreshold = 0xFFFF;   // the angle wraps, so no arrow ever hides
        arrows.fullyDownThreshold = 0xFFFF;
        arrows.tileTag = TAG_WIND_DIRECTION_SCROLL_ARROW;
        arrows.palTag = TAG_WIND_DIRECTION_SCROLL_ARROW;
        arrows.palNum = 0;
        sWindDirectionArrowTaskId = AddScrollIndicatorArrowPair(&arrows, &sWindDirectionValue);

        // Left/right flank the box horizontally (jump between defined directions).
        arrows.firstArrowType = SCROLL_ARROW_LEFT;
        arrows.firstX = 4;
        arrows.firstY = 24;
        arrows.secondArrowType = SCROLL_ARROW_RIGHT;
        arrows.secondX = 132;
        arrows.secondY = 24;
        arrows.fullyUpThreshold = 0xFFFF;
        arrows.fullyDownThreshold = 0xFFFF;
        arrows.tileTag = TAG_WIND_DIRECTION_LR_SCROLL_ARROW;
        arrows.palTag = TAG_WIND_DIRECTION_LR_SCROLL_ARROW;
        arrows.palNum = 0;
        sWindDirectionLRArrowTaskId = AddScrollIndicatorArrowPair(&arrows, &sWindDirectionValue);
    }

    CreateTask(DebugTask_WindDirectionInput, 3);
    sDebugMenuOpen = TRUE;
}

// Tears down the box's window, scroll arrows and input task. The caller decides what to do next.
static void Debug_TearDownWindDirectionBox(u8 taskId)
{
    RemoveScrollIndicatorArrowPair(sWindDirectionArrowTaskId);
    RemoveScrollIndicatorArrowPair(sWindDirectionLRArrowTaskId);
    ClearStdWindowAndFrameToTransparent(sWindDirectionWindowId, TRUE);
    RemoveWindow(sWindDirectionWindowId);
    DestroyTask(taskId);
    sDebugMenuOpen = FALSE;
}

// Commits the box (A): sets the target wind direction and hands input back to the field.
static void Debug_CommitWindDirectionBox(u8 taskId)
{
    u8 value = sWindDirectionValue;

    Debug_TearDownWindDirectionBox(taskId);
    SetWindDirection(value);
    UnlockPlayerFieldControls();
}

// Backs out of the box (B) without changing anything, returning to the weather menu.
static void Debug_CancelWindDirectionBox(u8 taskId)
{
    Debug_TearDownWindDirectionBox(taskId);
    Debug_ShowWeatherMenu();
}

// Up/down fine-step the angle by 1 (held to repeat); left/right jump between the 16 defined directions.
// Everything wraps around the circle. A commits, B cancels.
static void DebugTask_WindDirectionInput(u8 taskId)
{
    u16 index = sWindDirectionValue >> 4;

    // Skip the frame the box opens on: with the extra left/right arrow task this input task can be placed
    // later in the frame's task list than the menu that spawned it, which would re-read the selecting A
    // press here as an immediate commit. data[0] is zeroed by CreateTask, so the first run just arms it.
    if (gTasks[taskId].data[0] == 0)
    {
        gTasks[taskId].data[0] = 1;
        return;
    }

    if (JOY_REPEAT(DPAD_UP))
    {
        sWindDirectionValue = (sWindDirectionValue + 1) & 0xFF;
        PlaySE(SE_SELECT);
        Debug_DrawWindDirectionBox();
    }
    else if (JOY_REPEAT(DPAD_DOWN))
    {
        sWindDirectionValue = (sWindDirectionValue - 1) & 0xFF;
        PlaySE(SE_SELECT);
        Debug_DrawWindDirectionBox();
    }
    else if (JOY_NEW(DPAD_RIGHT))
    {
        sWindDirectionValue = ((index + 1) & 15) << 4; // next defined direction clockwise
        PlaySE(SE_SELECT);
        Debug_DrawWindDirectionBox();
    }
    else if (JOY_NEW(DPAD_LEFT))
    {
        // Snap down to the current direction if between two; otherwise step to the previous one.
        if (sWindDirectionValue & 15)
            sWindDirectionValue = index << 4;
        else
            sWindDirectionValue = ((index - 1) & 15) << 4;
        PlaySE(SE_SELECT);
        Debug_DrawWindDirectionBox();
    }
    else if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        Debug_CommitWindDirectionBox(taskId);
    }
    else if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        Debug_CancelWindDirectionBox(taskId);
    }
}

// Opens the numeric scroll box used to pick a target wind speed.
static void DebugAction_Weather_SetWindSpeed(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    Debug_OpenWindSpeedBox();
}

// Redraws the box as "Speed n" from the current sWindSpeedValue.
static void Debug_DrawWindSpeedBox(void)
{
    u8 text[16];

    StringCopy(text, sDebugText_WindSpeed_Label);
    ConvertIntToDecimalStringN(gStringVar1, sWindSpeedValue, STR_CONV_MODE_LEADING_ZEROS, 2);
    StringAppend(text, gStringVar1);

    FillWindowPixelBuffer(sWindSpeedWindowId, PIXEL_FILL(1));
    AddTextPrinterParameterized(sWindSpeedWindowId, FONT_NORMAL, text, 4, 1, TEXT_SKIP_DRAW, NULL);
    CopyWindowToVram(sWindSpeedWindowId, COPYWIN_GFX);
}

// Opens the wind speed box, starting on the current target so reopening lands where you left off.
static void Debug_OpenWindSpeedBox(void)
{
    sWindSpeedValue = gWeatherPtr->targetWindSpeed;

    LoadMessageBoxAndBorderGfx();
    sWindSpeedWindowId = CreateWindowFromRect(0, 1, 9, 2);
    DrawStdWindowFrame(sWindSpeedWindowId, FALSE);
    Debug_DrawWindSpeedBox();
    CopyWindowToVram(sWindSpeedWindowId, COPYWIN_FULL);

    {
        struct ScrollArrowsTemplate arrows;

        arrows.firstArrowType = SCROLL_ARROW_UP;
        arrows.firstX = 44;
        arrows.firstY = 12;
        arrows.secondArrowType = SCROLL_ARROW_DOWN;
        arrows.secondX = 44;
        arrows.secondY = 36;
        arrows.fullyUpThreshold = WIND_SPEED_MAX;
        arrows.fullyDownThreshold = 0;
        arrows.tileTag = TAG_WIND_SPEED_SCROLL_ARROW;
        arrows.palTag = TAG_WIND_SPEED_SCROLL_ARROW;
        arrows.palNum = 0;
        sWindSpeedArrowTaskId = AddScrollIndicatorArrowPair(&arrows, &sWindSpeedValue);
    }

    CreateTask(DebugTask_WindSpeedInput, 3);
    sDebugMenuOpen = TRUE;
}

// Tears down the box's window, scroll arrows and input task. The caller decides what to do next.
static void Debug_TearDownWindSpeedBox(u8 taskId)
{
    RemoveScrollIndicatorArrowPair(sWindSpeedArrowTaskId);
    ClearStdWindowAndFrameToTransparent(sWindSpeedWindowId, TRUE);
    RemoveWindow(sWindSpeedWindowId);
    DestroyTask(taskId);
    sDebugMenuOpen = FALSE;
}

// Commits the box (A): sets the target wind speed and hands input back to the field.
static void Debug_CommitWindSpeedBox(u8 taskId)
{
    u8 value = sWindSpeedValue;

    Debug_TearDownWindSpeedBox(taskId);
    SetWindSpeed(value);
    UnlockPlayerFieldControls();
}

// Backs out of the box (B) without changing anything, returning to the weather menu.
static void Debug_CancelWindSpeedBox(u8 taskId)
{
    Debug_TearDownWindSpeedBox(taskId);
    Debug_ShowWeatherMenu();
}

// Up/down adjust the value (held to repeat), clamped to [0, WIND_SPEED_MAX]. A commits, B cancels.
static void DebugTask_WindSpeedInput(u8 taskId)
{
    if (JOY_REPEAT(DPAD_UP) && sWindSpeedValue < WIND_SPEED_MAX)
    {
        sWindSpeedValue++;
        PlaySE(SE_SELECT);
        Debug_DrawWindSpeedBox();
    }
    else if (JOY_REPEAT(DPAD_DOWN) && sWindSpeedValue > 0)
    {
        sWindSpeedValue--;
        PlaySE(SE_SELECT);
        Debug_DrawWindSpeedBox();
    }
    else if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        Debug_CommitWindSpeedBox(taskId);
    }
    else if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        Debug_CancelWindSpeedBox(taskId);
    }
}

// -------------------- Clock submenu --------------------

static void Debug_ShowClockMenu(void)
{
    Debug_ShowMenu(sDebugMenuItems_Clock, ARRAY_COUNT(sDebugMenuItems_Clock), DebugTask_HandleMenuInput_Clock);
}

static void DebugAction_OpenClockMenu(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    Debug_ShowClockMenu();
}

static void DebugAction_Clock_Cancel(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    Debug_OpenMainMenu();
}

static void DebugTask_HandleMenuInput_Clock(u8 taskId)
{
    void (*func)(u8);
    s32 input;

    if (ListMenuTryWrapSelection(gTasks[taskId].tMenuTaskId))
        return;

    input = ListMenu_ProcessInput(gTasks[taskId].tMenuTaskId);

    if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        if ((func = sDebugMenuActions_Clock[input]) != NULL)
            func(taskId);
    }
    else if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        DebugAction_Clock_Cancel(taskId);
    }
}

static void DebugAction_Clock_SetHours(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    Debug_OpenClockSetBox(CLOCK_FIELD_HOURS);
}

static void DebugAction_Clock_SetMinutes(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    Debug_OpenClockSetBox(CLOCK_FIELD_MINUTES);
}

static void DebugAction_Clock_SetDay(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    Debug_OpenClockSetBox(CLOCK_FIELD_DAY);
}

static void DebugAction_Clock_SetMonth(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    Debug_OpenClockSetBox(CLOCK_FIELD_MONTH);
}

static void DebugAction_Clock_SetYear(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    Debug_OpenClockSetBox(CLOCK_FIELD_YEAR);
}

static void DebugAction_Clock_SetTimeScale(u8 taskId)
{
    Debug_DestroyMenu(taskId);
    Debug_OpenClockSetBox(CLOCK_FIELD_TIMESCALE);
}

// Redraws the box as e.g. "Hours 9", the month name for the month field, or value flanked by
// arrow glyphs for the coarse-step fields, e.g. "Year {<}1995{>}".
static void Debug_DrawClockSetBox(void)
{
    bool32 coarse = Debug_ClockFieldHasCoarseStep(sClockSetField);
    u8 text[24];

    StringCopy(text, sClockSetLabels[sClockSetField]);
    if (coarse)
        StringAppend(text, sDebugText_ClockSet_ArrowL);

    if (sClockSetField == CLOCK_FIELD_MONTH)
    {
        StringAppend(text, sDebugClockMonths[sClockSetValue - 1]);
    }
    else
    {
        ConvertIntToDecimalStringN(gStringVar1, sClockSetValue, STR_CONV_MODE_LEFT_ALIGN, 5);
        StringAppend(text, gStringVar1);
    }

    if (coarse)
        StringAppend(text, sDebugText_ClockSet_ArrowR);

    FillWindowPixelBuffer(sClockSetWindowId, PIXEL_FILL(1));
    AddTextPrinterParameterized(sClockSetWindowId, FONT_NORMAL, text, 4, 1, TEXT_SKIP_DRAW, NULL);
    CopyWindowToVram(sClockSetWindowId, COPYWIN_GFX);
}

// The wide-range fields get left/right arrows for a coarse +/-10 step.
static bool32 Debug_ClockFieldHasCoarseStep(u8 field)
{
    return field == CLOCK_FIELD_YEAR || field == CLOCK_FIELD_TIMESCALE;
}

// Opens the shared clock-field box on the given CLOCK_FIELD_*, seeded with the current value.
static void Debug_OpenClockSetBox(u8 field)
{
    struct Clock *clock = &gSaveBlock1Ptr->gameClock;

    sClockSetField = field;
    switch (field)
    {
    case CLOCK_FIELD_HOURS:
        sClockSetValue = clock->hours;
        sClockSetMin = 0;
        sClockSetMax = HOURS_PER_DAY - 1;
        break;
    case CLOCK_FIELD_MINUTES:
        sClockSetValue = clock->minutes;
        sClockSetMin = 0;
        sClockSetMax = MINUTES_PER_HOUR - 1;
        break;
    case CLOCK_FIELD_DAY:
        sClockSetValue = clock->day;
        sClockSetMin = 1;
        sClockSetMax = 31;
        break;
    case CLOCK_FIELD_MONTH:
        sClockSetValue = clock->month;
        sClockSetMin = MONTH_JAN;
        sClockSetMax = MONTH_DEC;
        break;
    case CLOCK_FIELD_YEAR:
        sClockSetValue = clock->year;
        sClockSetMin = 0;
        sClockSetMax = 9999;
        break;
    case CLOCK_FIELD_TIMESCALE:
        sClockSetValue = clock->timeScale;
        sClockSetMin = 0;
        sClockSetMax = 255; // timeScale is a u8
        break;
    }

    LoadMessageBoxAndBorderGfx();
    sClockSetWindowId = CreateWindowFromRect(0, 1, 12, 2);
    DrawStdWindowFrame(sClockSetWindowId, FALSE);
    Debug_DrawClockSetBox();
    CopyWindowToVram(sClockSetWindowId, COPYWIN_FULL);

    {
        struct ScrollArrowsTemplate arrows;

        arrows.firstArrowType = SCROLL_ARROW_UP;
        arrows.firstX = 44;
        arrows.firstY = 12;
        arrows.secondArrowType = SCROLL_ARROW_DOWN;
        arrows.secondX = 44;
        arrows.secondY = 36;
        arrows.fullyUpThreshold = sClockSetMax;
        arrows.fullyDownThreshold = sClockSetMin;
        arrows.tileTag = TAG_CLOCK_SET_SCROLL_ARROW;
        arrows.palTag = TAG_CLOCK_SET_SCROLL_ARROW;
        arrows.palNum = 0;
        sClockSetArrowTaskId = AddScrollIndicatorArrowPair(&arrows, &sClockSetValue);
    }

    CreateTask(DebugTask_ClockSetInput, 3);
    sDebugMenuOpen = TRUE;
}

// Tears down the box's window, scroll arrows and input task. The caller decides what to do next.
static void Debug_TearDownClockSetBox(u8 taskId)
{
    RemoveScrollIndicatorArrowPair(sClockSetArrowTaskId);
    ClearStdWindowAndFrameToTransparent(sClockSetWindowId, TRUE);
    RemoveWindow(sClockSetWindowId);
    DestroyTask(taskId);
    sDebugMenuOpen = FALSE;
}

// Commits the box (A): writes the edited field through its clock setter (which clamps the date),
// then returns to the clock menu so several fields can be set in a row.
static void Debug_CommitClockSetBox(u8 taskId)
{
    u8 field = sClockSetField;
    u16 value = sClockSetValue;

    Debug_TearDownClockSetBox(taskId);
    switch (field)
    {
    case CLOCK_FIELD_HOURS:   Clock_SetHours(value);   break;
    case CLOCK_FIELD_MINUTES: Clock_SetMinutes(value); break;
    case CLOCK_FIELD_DAY:     Clock_SetDay(value);     break;
    case CLOCK_FIELD_MONTH:   Clock_SetMonth(value);   break;
    case CLOCK_FIELD_YEAR:    Clock_SetYear(value);    break;
    case CLOCK_FIELD_TIMESCALE: Clock_SetTimeScale(value); break;
    }
    Debug_ShowClockMenu();
}

// Backs out of the box (B) without changing anything, returning to the clock menu.
static void Debug_CancelClockSetBox(u8 taskId)
{
    Debug_TearDownClockSetBox(taskId);
    Debug_ShowClockMenu();
}

// Up/down adjust the value by 1 (held to repeat); on wide-range fields left/right step by 10. All
// clamped to [sClockSetMin, sClockSetMax]. A commits, B cancels; both return to the clock menu.
static void DebugTask_ClockSetInput(u8 taskId)
{
    bool32 coarse = Debug_ClockFieldHasCoarseStep(sClockSetField);

    if (JOY_REPEAT(DPAD_UP) && sClockSetValue < sClockSetMax)
    {
        sClockSetValue++;
        PlaySE(SE_SELECT);
        Debug_DrawClockSetBox();
    }
    else if (JOY_REPEAT(DPAD_DOWN) && sClockSetValue > sClockSetMin)
    {
        sClockSetValue--;
        PlaySE(SE_SELECT);
        Debug_DrawClockSetBox();
    }
    else if (coarse && JOY_REPEAT(DPAD_RIGHT) && sClockSetValue < sClockSetMax)
    {
        if (sClockSetMax - sClockSetValue < CLOCK_SET_COARSE_STEP)
            sClockSetValue = sClockSetMax;
        else
            sClockSetValue += CLOCK_SET_COARSE_STEP;
        PlaySE(SE_SELECT);
        Debug_DrawClockSetBox();
    }
    else if (coarse && JOY_REPEAT(DPAD_LEFT) && sClockSetValue > sClockSetMin)
    {
        if (sClockSetValue - sClockSetMin < CLOCK_SET_COARSE_STEP)
            sClockSetValue = sClockSetMin;
        else
            sClockSetValue -= CLOCK_SET_COARSE_STEP;
        PlaySE(SE_SELECT);
        Debug_DrawClockSetBox();
    }
    else if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        Debug_CommitClockSetBox(taskId);
    }
    else if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        Debug_CancelClockSetBox(taskId);
    }
}

#undef tMenuTaskId
#undef tWindowId
