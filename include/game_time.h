#ifndef GUARD_GAME_TIME_H
#define GUARD_GAME_TIME_H

// Day-of-week index returned by GameClock_GetDayOfWeek (0 = Sunday).
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

void InitGameClock(void);
void GameClock_Update(void);
void GameClock_AdvanceSeconds(u32 seconds);
u32 GameClock_GetDayOfWeek(void);
void GameClock_SetMinutes(u32 minutes);
void GameClock_SetHours(u32 hours);
void GameClock_SetDay(u32 day);
void GameClock_SetMonth(u32 month);
void GameClock_SetYear(u32 year);
void GameClock_SetTimeScale(u32 scale);

#endif // GUARD_GAME_TIME_H
