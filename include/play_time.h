#ifndef GUARD_PLAY_TIME_H
#define GUARD_PLAY_TIME_H

#define TIME_MINUTE_0			0
#define TIME_MINUTE_1			1
#define TIME_MINUTE_2			2

#define TIME_HOUR_MIDNIGHT		0
#define TIME_HOUR_1AM			1
#define TIME_HOUR_2AM			2
#define TIME_HOUR_3AM			3
#define TIME_HOUR_4AM			4
#define TIME_HOUR_5AM			5
#define TIME_HOUR_6AM			6
#define TIME_HOUR_7AM			7
#define TIME_HOUR_8AM			8
#define TIME_HOUR_9AM			9
#define TIME_HOUR_10AM			10
#define TIME_HOUR_11AM			11
#define TIME_HOUR_MIDDAY		12
#define TIME_HOUR_1PM			13
#define TIME_HOUR_2PM			14
#define TIME_HOUR_3PM			15
#define TIME_HOUR_4PM			16
#define TIME_HOUR_5PM			17
#define TIME_HOUR_6PM			18
#define TIME_HOUR_7PM			19
#define TIME_HOUR_8PM			20
#define TIME_HOUR_9PM			21
#define TIME_HOUR_10PM			22
#define TIME_HOUR_11PM			23

#define TIME_DAY_MONDAY 		0
#define TIME_DAY_TUESDAY 		1
#define TIME_DAY_WEDNESDAY 		2
#define TIME_DAY_THURSDAY 		3
#define TIME_DAY_FRIDAY 		4
#define TIME_DAY_SATURDAY 		5
#define TIME_DAY_SUNDAY 		6

#define TIME_WEEK_0				0
#define TIME_WEEK_1				1

#define TIME_SEASON_SPRING 		0
#define TIME_SEASON_SUMMER 		1
#define TIME_SEASON_FALL 		2
#define TIME_SEASON_WINTER 		3

#define TIME_YEAR_0				0
#define TIME_YEAR_1				1
#define TIME_YEAR_2				2
#define TIME_YEAR_3				3
#define TIME_YEAR_4				4
#define TIME_YEAR_5				5
#define TIME_YEAR_6				6
#define TIME_YEAR_7				7

#define TIME_DAWN				0
#define TIME_DAY				1
#define TIME_DUSK				2
#define TIME_NIGHT				3

#define TIME_DAY_ANY			((TIME_DAWN | TIME_DAY))
#define TIME_NIGHT_ANY			((TIME_DUSK | TIME_NIGHT))

void PlayTimeCounter_Reset(void);
void PlayTimeCounter_Start(void);
void PlayTimeCounter_Stop(void);
void PlayTimeCounter_Update(void);
void PlayTimeCounter_SetToMax(void);

void IncrementClockSecond(bool8 runRoutines);
void IncrementClockMinute(bool8 runRoutines);
void IncrementClockHour(bool8 runRoutines);
void IncrementClockDay(bool8 runRoutines);
void IncrementClockWeek(bool8 runRoutines);
void IncrementClockSeason(bool8 runRoutines);
void IncrementClockYear(bool8 runRoutines);
void UpdateDayNightStatus(void);
u8 CalculateSubSeason(void);

//hour routines
void TryIncrementWaitTime(void);

#endif // GUARD_PLAY_TIME_H
