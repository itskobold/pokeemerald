#ifndef GUARD_CLOCK_H
#define GUARD_CLOCK_H

#define HOURS_PER_DAY       24
#define MINUTES_PER_HOUR    60
#define SECONDS_PER_MINUTE  60

enum
{
    MONTH_JAN = 1,
    MONTH_FEB,
    MONTH_MAR,
    MONTH_APR,
    MONTH_MAY,
    MONTH_JUN,
    MONTH_JUL,
    MONTH_AUG,
    MONTH_SEP,
    MONTH_OCT,
    MONTH_NOV,
    MONTH_DEC,
    MONTH_COUNT = MONTH_DEC
};

// Day-of-week index returned by Clock_GetDayOfWeek (0 = Sunday).
enum
{
    WEEKDAY_SUN,
    WEEKDAY_MON,
    WEEKDAY_TUE,
    WEEKDAY_WED,
    WEEKDAY_THU,
    WEEKDAY_FRI,
    WEEKDAY_SAT,
};

// Coarse time-of-day band, returned by Clock_GetTimeOfDay.
enum
{
    TIME_OF_DAY_MIDNIGHT, // 12:00am - 12:30am
    TIME_OF_DAY_NIGHT,    // 12:30am - dawn, and dusk - midnight
    TIME_OF_DAY_DAWN,     // sunrise twilight
    TIME_OF_DAY_DAY,      // end of dawn - noon, and 12:30pm - dusk
    TIME_OF_DAY_MIDDAY,   // 12:00pm - 12:30pm
    TIME_OF_DAY_DUSK,     // sunset twilight
};

// Season, returned by Clock_GetSeason. Boundaries fall on the 1st of the month.
enum
{
    SEASON_SPRING, // Mar 1 - Jun 1
    SEASON_SUMMER, // Jun 1 - Sep 1
    SEASON_AUTUMN, // Sep 1 - Dec 1
    SEASON_WINTER, // Dec 1 - Mar 1
};

// Phase within a season, returned by Clock_GetSeasonPhase: the first two weeks
// (early), the last two weeks (late), or the stretch between (mid). Late winter
// gains a day in leap years so its window keeps the same start date.
enum
{
    SEASON_PHASE_EARLY,
    SEASON_PHASE_MID,
    SEASON_PHASE_LATE,
};

void Clock_Init(void);
void Clock_Update(void);
void Clock_AdvanceSeconds(u32 seconds);
u32 Clock_GetDayOfWeek(void);
u32 Clock_GetDayCount(void);
u32 Clock_GetTotalMinutes(void);
u32 Clock_GetTwilight(void);
u32 Clock_GetTimeOfDay(void);
u8 *Clock_GetTimeOfDayString(u8 *dest);
u32 Clock_GetSeason(void);
u8 *Clock_GetSeasonString(u8 *dest, bool32 abbreviate);
u8 *Clock_GetFullSeasonString(u8 *dest, bool32 abbreviate);
u32 Clock_GetSeasonPhase(void);
bool32 IsSeason(u32 season);
bool32 IsEarlySeason(u32 season);
bool32 IsMidSeason(u32 season);
bool32 IsLateSeason(u32 season);
bool32 IsDay(void);
bool32 IsNight(void);
bool32 IsMidday(void);
bool32 IsMidnight(void);
bool32 IsDawn(void);
bool32 IsDusk(void);
bool32 IsTwilight(void);
bool32 IsMorning(void);
bool32 IsAfternoon(void);
u8 *Clock_GetTimeString(u8 *dest);
u8 *Clock_GetDayString(u8 *dest, bool32 abbreviate);
u8 *Clock_GetMonthString(u8 *dest, bool32 abbreviate);
void Clock_SetMinutes(u32 minutes);
void Clock_SetHours(u32 hours);
void Clock_SetDay(u32 day);
void Clock_SetMonth(u32 month);
void Clock_SetYear(u32 year);
void Clock_SetTimeScale(u32 scale);

#endif // GUARD_CLOCK_H
