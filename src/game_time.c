#include "global.h"
#include "game_time.h"
#include "main.h"
#include "overworld.h"
#include "script.h"
#include "siirtc.h" // MONTH_* constants and HOURS_PER_DAY etc.

#define FRAMES_PER_SECOND 60
#define MINUTES_PER_DAY (HOURS_PER_DAY * MINUTES_PER_HOUR)

// Weekday of year 0, Jan 1 (Monday), used as the day-of-week reference point.
#define EPOCH_WEEKDAY WEEKDAY_MON

// Sub-second frame accumulator. Not saved; losing <1s on save/load is harmless.
static u8 sGameClockVBlanks;

static const u8 sDaysPerMonth[MONTH_COUNT] =
{
    [MONTH_JAN - 1] = 31,
    [MONTH_FEB - 1] = 28,
    [MONTH_MAR - 1] = 31,
    [MONTH_APR - 1] = 30,
    [MONTH_MAY - 1] = 31,
    [MONTH_JUN - 1] = 30,
    [MONTH_JUL - 1] = 31,
    [MONTH_AUG - 1] = 31,
    [MONTH_SEP - 1] = 30,
    [MONTH_OCT - 1] = 31,
    [MONTH_NOV - 1] = 30,
    [MONTH_DEC - 1] = 31,
};

static bool32 IsLeapYear(u16 year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static u8 DaysInMonth(u8 month, u16 year)
{
    if (month < MONTH_JAN || month > MONTH_DEC)
        return 30; // defensive: never index out of range on a stale clock
    if (month == MONTH_FEB && IsLeapYear(year))
        return 29;
    return sDaysPerMonth[month - 1];
}

void InitGameClock(void)
{
    struct GameClock *clock = &gSaveBlock1Ptr->gameClock;

    clock->seconds = 0;
    clock->minutes = 30;
    clock->hours = 9;
    clock->day = 15;
    clock->month = MONTH_MAY;
    clock->year = 0;
    clock->timeScale = 20;

    sGameClockVBlanks = 0;
}

// Whole days elapsed since the year 0, Jan 1 epoch.
static u32 DaysSinceEpoch(struct GameClock *clock)
{
    u32 days = clock->day - 1;
    u32 m, y;

    for (m = MONTH_JAN; m < clock->month; m++)
        days += DaysInMonth(m, clock->year);
    for (y = 0; y < clock->year; y++)
        days += IsLeapYear(y) ? 366 : 365;

    return days;
}

// Absolute minute and month counts since the epoch, used to detect interval boundary crossings.
static u32 AbsMinutes(struct GameClock *clock)
{
    return DaysSinceEpoch(clock) * MINUTES_PER_DAY + clock->hours * MINUTES_PER_HOUR + clock->minutes;
}

static u32 AbsMonths(struct GameClock *clock)
{
    return clock->year * MONTH_COUNT + (clock->month - 1);
}

// Returns the current day of the week (WEEKDAY_SUN..WEEKDAY_SAT), counted
// from the number of whole days elapsed since the year 0, Jan 1 epoch.
u32 GameClock_GetDayOfWeek(void)
{
    return (DaysSinceEpoch(&gSaveBlock1Ptr->gameClock) + EPOCH_WEEKDAY) % 7;
}

// ---------------------------------------------------------------------------
// Interval hooks. Each runs when the clock crosses the matching boundary as
// in-game time passes (aligned to the clock: OnHour at :00, OnDay at midnight,
// OnMonth on the 1st, OnYear in January). They are intentionally empty - fill
// in a body here to make something happen every minute, hour, day, etc.
// ---------------------------------------------------------------------------

static void GameClock_OnMinute(void)    {}
static void GameClock_On5Minutes(void)  {}
static void GameClock_On10Minutes(void) {}
static void GameClock_On15Minutes(void) {}
static void GameClock_On20Minutes(void) {}
static void GameClock_On30Minutes(void) {}
static void GameClock_OnHour(void)      {}
static void GameClock_On2Hours(void)    {}
static void GameClock_On3Hours(void)    {}
static void GameClock_On4Hours(void)    {}
static void GameClock_On6Hours(void)    {}
static void GameClock_On12Hours(void)   {}
static void GameClock_OnDay(void)       {}
static void GameClock_On3Days(void)     {}
static void GameClock_On5Days(void)     {}
static void GameClock_OnWeek(void)      {}
static void GameClock_On2Weeks(void)    {}
static void GameClock_OnMonth(void)     {}
static void GameClock_On2Months(void)   {}
static void GameClock_On3Months(void)   {}
static void GameClock_On4Months(void)   {}
static void GameClock_On6Months(void)   {}
static void GameClock_OnYear(void)      {}

// Fires each interval hook whose boundary was crossed between the before/after snapshots. A boundary
// of N units is crossed when before/N != after/N, so this stays correct even if a tick spans several.
static void GameClock_FireIntervals(u32 beforeMin, u32 afterMin, u32 beforeMonth, u32 afterMonth)
{
    u32 beforeDay = beforeMin / MINUTES_PER_DAY;
    u32 afterDay = afterMin / MINUTES_PER_DAY;

    if (afterMin == beforeMin)
        return; // less than a minute passed; no boundary can have been crossed

#define MIN_CROSSED(n)   ((beforeMin / (n)) != (afterMin / (n)))
#define DAY_CROSSED(n)   ((beforeDay / (n)) != (afterDay / (n)))
#define MONTH_CROSSED(n) ((beforeMonth / (n)) != (afterMonth / (n)))

    if (MIN_CROSSED(1))    GameClock_OnMinute();
    if (MIN_CROSSED(5))    GameClock_On5Minutes();
    if (MIN_CROSSED(10))   GameClock_On10Minutes();
    if (MIN_CROSSED(15))   GameClock_On15Minutes();
    if (MIN_CROSSED(20))   GameClock_On20Minutes();
    if (MIN_CROSSED(30))   GameClock_On30Minutes();
    if (MIN_CROSSED(60))   GameClock_OnHour();
    if (MIN_CROSSED(120))  GameClock_On2Hours();
    if (MIN_CROSSED(180))  GameClock_On3Hours();
    if (MIN_CROSSED(240))  GameClock_On4Hours();
    if (MIN_CROSSED(360))  GameClock_On6Hours();
    if (MIN_CROSSED(720))  GameClock_On12Hours();
    if (DAY_CROSSED(1))    GameClock_OnDay();
    if (DAY_CROSSED(3))    GameClock_On3Days();
    if (DAY_CROSSED(5))    GameClock_On5Days();
    if (DAY_CROSSED(7))    GameClock_OnWeek();
    if (DAY_CROSSED(14))   GameClock_On2Weeks();
    if (MONTH_CROSSED(1))  GameClock_OnMonth();
    if (MONTH_CROSSED(2))  GameClock_On2Months();
    if (MONTH_CROSSED(3))  GameClock_On3Months();
    if (MONTH_CROSSED(4))  GameClock_On4Months();
    if (MONTH_CROSSED(6))  GameClock_On6Months();
    if (MONTH_CROSSED(12)) GameClock_OnYear();

#undef MIN_CROSSED
#undef DAY_CROSSED
#undef MONTH_CROSSED
}

// Advances the calendar clock by the given number of seconds, rolling over
// minutes, hours, days, months and years as needed, then runs any interval
// hooks whose boundary was crossed.
void GameClock_AdvanceSeconds(u32 seconds)
{
    struct GameClock *clock = &gSaveBlock1Ptr->gameClock;
    u32 beforeMin = AbsMinutes(clock);
    u32 beforeMonth = AbsMonths(clock);
    u32 carry;

    seconds += clock->seconds;
    clock->seconds = seconds % SECONDS_PER_MINUTE;
    carry = seconds / SECONDS_PER_MINUTE;

    if (carry != 0)
    {
        carry += clock->minutes;
        clock->minutes = carry % MINUTES_PER_HOUR;
        carry /= MINUTES_PER_HOUR;

        if (carry != 0)
        {
            carry += clock->hours;
            clock->hours = carry % HOURS_PER_DAY;
            carry /= HOURS_PER_DAY;

            // Roll over whole days one at a time so month/year lengths stay correct.
            while (carry-- != 0)
            {
                if (++clock->day > DaysInMonth(clock->month, clock->year))
                {
                    clock->day = 1;
                    if (++clock->month > MONTH_DEC)
                    {
                        clock->month = MONTH_JAN;
                        clock->year++;
                    }
                }
            }
        }
    }

    GameClock_FireIntervals(beforeMin, AbsMinutes(clock), beforeMonth, AbsMonths(clock));
}

// The clock only advances while the player is roaming the overworld freely.
// Field controls are locked by scripts, message boxes, NPC/signpost
// interactions and the start/debug menus; a non-overworld callback2 means a
// full-screen menu (bag, party, etc.) is open.
static bool32 ShouldGameClockTick(void)
{
    return gMain.callback2 == CB2_Overworld && !ArePlayerFieldControlsLocked();
}

// Called every frame. Locked at 60 fps, so 60 frames advance the clock by
// timeScale in-game seconds (timeScale = in-game seconds per real second).
void GameClock_Update(void)
{
    if (!ShouldGameClockTick())
        return;

    if (++sGameClockVBlanks < FRAMES_PER_SECOND)
        return;

    sGameClockVBlanks = 0;
    GameClock_AdvanceSeconds(gSaveBlock1Ptr->gameClock.timeScale);
}

// Clamps the day into the current month's valid range; call after changing month/year, since a
// shorter month (or a non-leap February) can leave the stored day past the end of the month.
static void ClampDay(struct GameClock *clock)
{
    u8 max = DaysInMonth(clock->month, clock->year);

    if (clock->day < 1)
        clock->day = 1;
    else if (clock->day > max)
        clock->day = max;
}

void GameClock_SetMinutes(u32 minutes)
{
    gSaveBlock1Ptr->gameClock.minutes = minutes % MINUTES_PER_HOUR;
}

void GameClock_SetHours(u32 hours)
{
    gSaveBlock1Ptr->gameClock.hours = hours % HOURS_PER_DAY;
}

void GameClock_SetDay(u32 day)
{
    struct GameClock *clock = &gSaveBlock1Ptr->gameClock;
    u8 max = DaysInMonth(clock->month, clock->year);

    if (day < 1)
        day = 1;
    else if (day > max)
        day = max;
    clock->day = day;
}

void GameClock_SetMonth(u32 month)
{
    struct GameClock *clock = &gSaveBlock1Ptr->gameClock;

    if (month < MONTH_JAN)
        month = MONTH_JAN;
    else if (month > MONTH_DEC)
        month = MONTH_DEC;
    clock->month = month;
    ClampDay(clock);
}

void GameClock_SetYear(u32 year)
{
    struct GameClock *clock = &gSaveBlock1Ptr->gameClock;

    clock->year = year;
    ClampDay(clock); // a leap/non-leap change can shorten February
}

void GameClock_SetTimeScale(u32 scale)
{
    gSaveBlock1Ptr->gameClock.timeScale = scale; // in-game seconds per real second (0 pauses)
}
