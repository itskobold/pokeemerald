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

void Clock_Init(void);
void Clock_Update(void);
void Clock_AdvanceSeconds(u32 seconds);
u32 Clock_GetDayOfWeek(void);
u32 Clock_GetDayCount(void);
u32 Clock_GetTotalMinutes(void);
void Clock_SetMinutes(u32 minutes);
void Clock_SetHours(u32 hours);
void Clock_SetDay(u32 day);
void Clock_SetMonth(u32 month);
void Clock_SetYear(u32 year);
void Clock_SetTimeScale(u32 scale);

#endif // GUARD_CLOCK_H
