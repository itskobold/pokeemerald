#include "global.h"
#include "menu.h"
#include "main.h"
#include "play_time.h"
#include "start_menu.h"

const u8 gSunriseTimes[8] = {TIME_HOUR_6AM, TIME_HOUR_5AM, TIME_HOUR_4AM, TIME_HOUR_5AM, TIME_HOUR_6AM, TIME_HOUR_7AM, TIME_HOUR_8AM, TIME_HOUR_7AM};
const u8 gSunsetTimes[8] = {TIME_HOUR_7PM, TIME_HOUR_8PM, TIME_HOUR_9PM, TIME_HOUR_8PM, TIME_HOUR_7PM, TIME_HOUR_6PM, TIME_HOUR_5PM, TIME_HOUR_6PM};

enum
{
    STOPPED,
    RUNNING,
    MAXED_OUT
};

static u8 sPlayTimeCounterState;

void PlayTimeCounter_Reset(void)
{
    sPlayTimeCounterState = STOPPED;

    gSaveBlock2Ptr->playTimeHours = 0;
    gSaveBlock2Ptr->playTimeMinutes = 0;
    gSaveBlock2Ptr->playTimeSeconds = 0;
    gSaveBlock2Ptr->playTimeVBlanks = 0;
}

void PlayTimeCounter_Loop()
{
    gSaveBlock2Ptr->playTimeHours = 0;
    gSaveBlock2Ptr->playTimeMinutes = 0;
    gSaveBlock2Ptr->playTimeSeconds = 0;
    gSaveBlock2Ptr->playTimeVBlanks = 0;
}

void PlayTimeCounter_Start(void)
{
    sPlayTimeCounterState = RUNNING;

    if (gSaveBlock2Ptr->playTimeHours > 0xFFFF)
        PlayTimeCounter_Loop();
}

void PlayTimeCounter_Stop(void)
{
    sPlayTimeCounterState = STOPPED;
}

void PlayTimeCounter_Update()
{
    if (sPlayTimeCounterState == !STOPPED)
    {		
        gSaveBlock2Ptr->playTimeVBlanks++;

        if (gSaveBlock2Ptr->playTimeVBlanks > 59)
        {
            gSaveBlock2Ptr->playTimeVBlanks = 0;
            gSaveBlock2Ptr->playTimeSeconds++;
			
			if (!gMain.stopClockUpdating && !gMain.inBattle) //stops clock updating in battle & in start menu/submenus such as bag
				IncrementClockSecond(TRUE);

            if (gSaveBlock2Ptr->playTimeSeconds > 59)
            {
                gSaveBlock2Ptr->playTimeSeconds = 0;
                gSaveBlock2Ptr->playTimeMinutes++;

                if (gSaveBlock2Ptr->playTimeMinutes > 59)
                {
                    gSaveBlock2Ptr->playTimeMinutes = 0;
                    gSaveBlock2Ptr->playTimeHours++;

                    if (gSaveBlock2Ptr->playTimeHours > 65535)
                        PlayTimeCounter_Loop();
                }
            }
        }
    }
}

void PlayTimeCounter_SetToMax(void)
{
    sPlayTimeCounterState = MAXED_OUT;

    gSaveBlock2Ptr->playTimeHours = 999;
    gSaveBlock2Ptr->playTimeMinutes = 59;
    gSaveBlock2Ptr->playTimeSeconds = 59;
    gSaveBlock2Ptr->playTimeVBlanks = 59;
}

static void RunSecondRoutines(void) //called every second outside of battles/menus
{
}

static void RunMinuteRoutines(void) //called every minute outside of battles/menus
{
}

static void RunHourRoutines(void) //called every hour outside of battles/menus
{
	TryIncrementWaitTime();
}

static void RunDayRoutines(void) //called every day outside of battles/menus
{
}

static void RunWeekRoutines(void) //called every week outside of battles/menus
{
}

static void RunSeasonRoutines(void) //called every season outside of battles/menus
{
}

static void RunYearRoutines(void) //called every year outside of battles/menus
{
}

void IncrementClockSecond(bool8 runRoutines) //adds 1 second to game time
{
	if (gSaveBlock2Ptr->timeSeconds + 1 > 59)
	{
		gSaveBlock2Ptr->timeSeconds = 0;
		IncrementClockMinute(runRoutines);
		return;
	}
	
	gSaveBlock2Ptr->timeSeconds++;
	
	if (runRoutines)
		RunSecondRoutines();
}

void IncrementClockMinute(bool8 runRoutines) //adds 1 minute to game time
{
	if (gSaveBlock2Ptr->timeMinute + 1 > TIME_MINUTE_2)
	{
		gSaveBlock2Ptr->timeMinute = TIME_MINUTE_0;
		IncrementClockHour(runRoutines);
		return;
	}
	
	gSaveBlock2Ptr->timeMinute++;
	
	if (runRoutines)
		RunMinuteRoutines();
}

void IncrementClockHour(bool8 runRoutines) //adds 1 hour to game time
{
	if (gSaveBlock2Ptr->timeHour + 1 > TIME_HOUR_11PM)
	{
		gSaveBlock2Ptr->timeHour = TIME_HOUR_MIDNIGHT;
		IncrementClockDay(runRoutines);
		return;
	}
	gSaveBlock2Ptr->timeHour++;
	
	UpdateDayNightStatus(); //called seperately from hour routines as it's always needed
	
	if (runRoutines)
		RunHourRoutines();
}

void IncrementClockDay(bool8 runRoutines) //adds 1 day to game time
{
	if (gSaveBlock2Ptr->timeDay + 1 > TIME_DAY_SUNDAY)
	{
		gSaveBlock2Ptr->timeDay = TIME_DAY_MONDAY;
		IncrementClockWeek(runRoutines);
		return;
	}
	gSaveBlock2Ptr->timeDay++;
	
	if (runRoutines)
		RunDayRoutines();
}

void IncrementClockWeek(bool8 runRoutines) //adds 1 week to game time
{
	if (gSaveBlock2Ptr->timeWeek + 1 > TIME_WEEK_1)
	{
		gSaveBlock2Ptr->timeWeek = TIME_WEEK_0;
		IncrementClockSeason(runRoutines);
		return;
	}
	gSaveBlock2Ptr->timeWeek++;
	
	if (runRoutines)
		RunWeekRoutines();
}

void IncrementClockSeason(bool8 runRoutines) //adds 1 season to game time
{
	if (gSaveBlock2Ptr->timeSeason + 1 > TIME_SEASON_WINTER)
	{
		gSaveBlock2Ptr->timeSeason = TIME_SEASON_SPRING;
		IncrementClockYear(TRUE);
		return;
	}
	gSaveBlock2Ptr->timeSeason++;
	
	if (runRoutines)
		RunSeasonRoutines();
}

void IncrementClockYear(bool8 runRoutines) //adds 1 year to game time
{
	if (gSaveBlock2Ptr->timeYear + 1 > TIME_YEAR_7)
	{
		gSaveBlock2Ptr->timeYear = TIME_YEAR_0;
		return;
	}
	gSaveBlock2Ptr->timeYear++;
	
	if (runRoutines)
		RunYearRoutines();
}

u8 CalculateSubSeason(void)
{
	int i = 1;
	u8 month = (gSaveBlock2Ptr->timeSeason * 3);
	
	if (gSaveBlock2Ptr->timeWeek == TIME_WEEK_1 && gSaveBlock2Ptr->timeDay > TIME_DAY_THURSDAY)
		i++;
	else if (gSaveBlock2Ptr->timeWeek == TIME_WEEK_0 && gSaveBlock2Ptr->timeDay < TIME_DAY_THURSDAY)
		i--;
	
	month += i;
	
	return month;
}

void UpdateDayNightStatus(void)
{
	u8 season = gSaveBlock2Ptr->timeSeason * 2;
	
	if (gSaveBlock2Ptr->timeWeek == TIME_WEEK_1)
		season++;
	
	if (gSaveBlock2Ptr->timeHour == gSunriseTimes[season] || gSaveBlock2Ptr->timeHour == (gSunriseTimes[season] + 1)) //if 1 hour before/after sunrise time, it is dawn
		gSaveBlock2Ptr->dayNightStatus = TIME_DAWN;
	else if (gSaveBlock2Ptr->timeHour == gSunsetTimes[season] || gSaveBlock2Ptr->timeHour == (gSunsetTimes[season] - 1)) //if 1 hour before/after sunset time, it is dusk
		gSaveBlock2Ptr->dayNightStatus = TIME_DUSK;
	else if (gSaveBlock2Ptr->timeHour < gSunriseTimes[season] || gSaveBlock2Ptr->timeHour > gSunsetTimes[season]) //if hour is lower than sunrise or after sunset time, it is night
		gSaveBlock2Ptr->dayNightStatus = TIME_NIGHT;
	else //if not any of these, it is day
		gSaveBlock2Ptr->dayNightStatus = TIME_DAY;
}

void TryIncrementWaitTime(void)
{
	if (gSaveBlock2Ptr->waitTime < 60)
		gSaveBlock2Ptr->waitTime++;
}
