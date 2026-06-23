#include "global.h"
#include "clock.h"
#include "berry.h"
#include "dewford_trend.h"
#include "event_data.h"
#include "field_specials.h"
#include "lottery_corner.h"
#include "main.h"
#include "overworld.h"
#include "pokemon.h"
#include "script.h"
#include "string_util.h"
#include "time_events.h"
#include "trig.h"
#include "tv.h"
#include "constants/characters.h"

#define FRAMES_PER_SECOND 60
#define MINUTES_PER_DAY (HOURS_PER_DAY * MINUTES_PER_HOUR)

// Sunrise/sunset follow a yearly sine wave between the solstices. Times in
// minutes past midnight: winter (Dec 21) sunrise 8:06 / sunset 4:02, summer
// (Jun 21) sunrise 4:42 / sunset 9:21. mid is the average of the two extremes,
// amp is half their swing; the wave peaks at the winter solstice.
#define SUNRISE_MID 384 // (8:06 + 4:42) / 2
#define SUNRISE_AMP 102 // (8:06 - 4:42) / 2
#define SUNSET_MID  1121 // (4:02PM + 9:21PM) / 2
#define SUNSET_AMP  160  // (9:21PM - 4:02PM) / 2

#define WINTER_SOLSTICE_DOY 354 // 0-based day-of-year of Dec 21 (+1 in a leap year)

// Twilight runs for 128 minutes from sunrise (morning) and up to sunset (evening).
#define TWILIGHT_MINUTES 128

// Time-of-day bands. Midnight and midday each span the 30 minutes from the hour.
#define NOON_MINUTES      (MINUTES_PER_DAY / 2)
#define SOLAR_BAND_LENGTH 30

// Early season is the first two weeks, late season the last two weeks.
#define SEASON_PHASE_DAYS 14

// Weekday of year 0, Jan 1 (Monday), used as the day-of-week reference point.
#define EPOCH_WEEKDAY WEEKDAY_MON

// Sub-second frame accumulator. Not saved; losing <1s on save/load is harmless.
static u8 sClockVBlanks;

// Minutes into the current twilight window (1..TWILIGHT_MINUTES), else 0.
// RAM-only by design; recomputed each minute, not stored in the save.
static u8 sTwilightCounter;

// Current time-of-day band (TIME_OF_DAY_*). RAM-only, recomputed each minute.
static u8 sTimeOfDay;

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

static const u8 sDayName_Sun[] = _("Sunday");
static const u8 sDayName_Mon[] = _("Monday");
static const u8 sDayName_Tue[] = _("Tuesday");
static const u8 sDayName_Wed[] = _("Wednesday");
static const u8 sDayName_Thu[] = _("Thursday");
static const u8 sDayName_Fri[] = _("Friday");
static const u8 sDayName_Sat[] = _("Saturday");

static const u8 *const sDayNames[] =
{
    [WEEKDAY_SUN] = sDayName_Sun,
    [WEEKDAY_MON] = sDayName_Mon,
    [WEEKDAY_TUE] = sDayName_Tue,
    [WEEKDAY_WED] = sDayName_Wed,
    [WEEKDAY_THU] = sDayName_Thu,
    [WEEKDAY_FRI] = sDayName_Fri,
    [WEEKDAY_SAT] = sDayName_Sat,
};

static const u8 sDayNameShort_Sun[] = _("Sun");
static const u8 sDayNameShort_Mon[] = _("Mon");
static const u8 sDayNameShort_Tue[] = _("Tue");
static const u8 sDayNameShort_Wed[] = _("Wed");
static const u8 sDayNameShort_Thu[] = _("Thu");
static const u8 sDayNameShort_Fri[] = _("Fri");
static const u8 sDayNameShort_Sat[] = _("Sat");

static const u8 *const sDayNamesShort[] =
{
    [WEEKDAY_SUN] = sDayNameShort_Sun,
    [WEEKDAY_MON] = sDayNameShort_Mon,
    [WEEKDAY_TUE] = sDayNameShort_Tue,
    [WEEKDAY_WED] = sDayNameShort_Wed,
    [WEEKDAY_THU] = sDayNameShort_Thu,
    [WEEKDAY_FRI] = sDayNameShort_Fri,
    [WEEKDAY_SAT] = sDayNameShort_Sat,
};

static const u8 sMonthName_Jan[] = _("January");
static const u8 sMonthName_Feb[] = _("February");
static const u8 sMonthName_Mar[] = _("March");
static const u8 sMonthName_Apr[] = _("April");
static const u8 sMonthName_May[] = _("May");
static const u8 sMonthName_Jun[] = _("June");
static const u8 sMonthName_Jul[] = _("July");
static const u8 sMonthName_Aug[] = _("August");
static const u8 sMonthName_Sep[] = _("September");
static const u8 sMonthName_Oct[] = _("October");
static const u8 sMonthName_Nov[] = _("November");
static const u8 sMonthName_Dec[] = _("December");

static const u8 *const sMonthNames[MONTH_COUNT] =
{
    [MONTH_JAN - 1] = sMonthName_Jan,
    [MONTH_FEB - 1] = sMonthName_Feb,
    [MONTH_MAR - 1] = sMonthName_Mar,
    [MONTH_APR - 1] = sMonthName_Apr,
    [MONTH_MAY - 1] = sMonthName_May,
    [MONTH_JUN - 1] = sMonthName_Jun,
    [MONTH_JUL - 1] = sMonthName_Jul,
    [MONTH_AUG - 1] = sMonthName_Aug,
    [MONTH_SEP - 1] = sMonthName_Sep,
    [MONTH_OCT - 1] = sMonthName_Oct,
    [MONTH_NOV - 1] = sMonthName_Nov,
    [MONTH_DEC - 1] = sMonthName_Dec,
};

static const u8 sMonthNameShort_Jan[] = _("Jan");
static const u8 sMonthNameShort_Feb[] = _("Feb");
static const u8 sMonthNameShort_Mar[] = _("Mar");
static const u8 sMonthNameShort_Apr[] = _("Apr");
static const u8 sMonthNameShort_May[] = _("May");
static const u8 sMonthNameShort_Jun[] = _("Jun");
static const u8 sMonthNameShort_Jul[] = _("Jul");
static const u8 sMonthNameShort_Aug[] = _("Aug");
static const u8 sMonthNameShort_Sep[] = _("Sep");
static const u8 sMonthNameShort_Oct[] = _("Oct");
static const u8 sMonthNameShort_Nov[] = _("Nov");
static const u8 sMonthNameShort_Dec[] = _("Dec");

static const u8 *const sMonthNamesShort[MONTH_COUNT] =
{
    [MONTH_JAN - 1] = sMonthNameShort_Jan,
    [MONTH_FEB - 1] = sMonthNameShort_Feb,
    [MONTH_MAR - 1] = sMonthNameShort_Mar,
    [MONTH_APR - 1] = sMonthNameShort_Apr,
    [MONTH_MAY - 1] = sMonthNameShort_May,
    [MONTH_JUN - 1] = sMonthNameShort_Jun,
    [MONTH_JUL - 1] = sMonthNameShort_Jul,
    [MONTH_AUG - 1] = sMonthNameShort_Aug,
    [MONTH_SEP - 1] = sMonthNameShort_Sep,
    [MONTH_OCT - 1] = sMonthNameShort_Oct,
    [MONTH_NOV - 1] = sMonthNameShort_Nov,
    [MONTH_DEC - 1] = sMonthNameShort_Dec,
};

static const u8 sText_AM[] = _("AM");
static const u8 sText_PM[] = _("PM");

static const u8 sTimeOfDayName_Midnight[] = _("MIDNIGHT");
static const u8 sTimeOfDayName_Night[]    = _("NIGHT");
static const u8 sTimeOfDayName_Dawn[]     = _("DAWN");
static const u8 sTimeOfDayName_Day[]      = _("DAY");
static const u8 sTimeOfDayName_Midday[]   = _("MIDDAY");
static const u8 sTimeOfDayName_Dusk[]     = _("DUSK");

static const u8 *const sTimeOfDayNames[] =
{
    [TIME_OF_DAY_MIDNIGHT] = sTimeOfDayName_Midnight,
    [TIME_OF_DAY_NIGHT]    = sTimeOfDayName_Night,
    [TIME_OF_DAY_DAWN]     = sTimeOfDayName_Dawn,
    [TIME_OF_DAY_DAY]      = sTimeOfDayName_Day,
    [TIME_OF_DAY_MIDDAY]   = sTimeOfDayName_Midday,
    [TIME_OF_DAY_DUSK]     = sTimeOfDayName_Dusk,
};

static const u8 sSeasonName_Spring[] = _("SPRING");
static const u8 sSeasonName_Summer[] = _("SUMMER");
static const u8 sSeasonName_Autumn[] = _("AUTUMN");
static const u8 sSeasonName_Winter[] = _("WINTER");

static const u8 *const sSeasonNames[] =
{
    [SEASON_SPRING] = sSeasonName_Spring,
    [SEASON_SUMMER] = sSeasonName_Summer,
    [SEASON_AUTUMN] = sSeasonName_Autumn,
    [SEASON_WINTER] = sSeasonName_Winter,
};

static const u8 sSeasonNameShort_Spring[] = _("SPRG");
static const u8 sSeasonNameShort_Summer[] = _("SMMR");
static const u8 sSeasonNameShort_Autumn[] = _("ATMN");
static const u8 sSeasonNameShort_Winter[] = _("WNTR");

static const u8 *const sSeasonNamesShort[] =
{
    [SEASON_SPRING] = sSeasonNameShort_Spring,
    [SEASON_SUMMER] = sSeasonNameShort_Summer,
    [SEASON_AUTUMN] = sSeasonNameShort_Autumn,
    [SEASON_WINTER] = sSeasonNameShort_Winter,
};

// Prefixes for the early/late portions of a season; mid has no prefix.
static const u8 sSeasonPhase_Early[]      = _("EARLY ");
static const u8 sSeasonPhase_Late[]       = _("LATE ");
static const u8 sSeasonPhaseShort_Early[] = _("E. ");
static const u8 sSeasonPhaseShort_Late[]  = _("L. ");

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

static void Clock_RecalcSunTimes(struct Clock *clock);
static void Clock_UpdateTwilight(struct Clock *clock);
static void Clock_UpdateTimeOfDay(struct Clock *clock);

void Clock_Init(void)
{
    struct Clock *clock = &gSaveBlock1Ptr->gameClock;

    clock->seconds = 0;
    clock->minutes = 30;
    clock->hours = 9;
    clock->day = 15;
    clock->month = MONTH_MAY;
    clock->year = 0;
    clock->timeScale = 20;

    Clock_RecalcSunTimes(clock);
    Clock_UpdateTwilight(clock);
    Clock_UpdateTimeOfDay(clock);

    sClockVBlanks = 0;
}

// Whole days elapsed since the year 0, Jan 1 epoch.
static u32 DaysSinceEpoch(struct Clock *clock)
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
static u32 AbsMinutes(struct Clock *clock)
{
    return DaysSinceEpoch(clock) * MINUTES_PER_DAY + clock->hours * MINUTES_PER_HOUR + clock->minutes;
}

static u32 AbsMonths(struct Clock *clock)
{
    return clock->year * MONTH_COUNT + (clock->month - 1);
}

// 0-based day of the year (Jan 1 = 0), used to position the sun-time sine wave.
static u32 DayOfYear(struct Clock *clock)
{
    u32 doy = clock->day - 1;
    u32 m;

    for (m = MONTH_JAN; m < clock->month; m++)
        doy += DaysInMonth(m, clock->year);

    return doy;
}

// Position on the yearly sun-time wave: sine-table index 0..255 spanning the
// year, 0 at the winter solstice. Uses the actual year length and a shifted
// solstice day so the leap day keeps the wave aligned to the calendar.
static s16 SunWaveIndex(struct Clock *clock)
{
    bool32 leap = IsLeapYear(clock->year);
    u32 yearLen = leap ? 366 : 365;
    u32 solsticeDoy = WINTER_SOLSTICE_DOY + (leap ? 1 : 0);
    u32 d = (DayOfYear(clock) + yearLen - solsticeDoy) % yearLen;

    return (d * 256) / yearLen;
}

// Sunrise/sunset minutes-of-day for the clock's current date. Cos peaks (+) at
// the winter solstice and troughs (-) at the summer solstice.
static u16 CalcSunriseTime(struct Clock *clock)
{
    return SUNRISE_MID + Cos(SunWaveIndex(clock), SUNRISE_AMP);
}

static u16 CalcSunsetTime(struct Clock *clock)
{
    return SUNSET_MID - Cos(SunWaveIndex(clock), SUNSET_AMP);
}

static void Clock_RecalcSunTimes(struct Clock *clock)
{
    clock->sunriseTime = CalcSunriseTime(clock);
    clock->sunsetTime = CalcSunsetTime(clock);
}

// Recomputes the twilight counter for the current time of day. Morning twilight
// runs [sunrise, sunrise + 128); evening twilight runs [sunset - 128, sunset).
// The counter reads 1 on the first minute, climbs to 128 on the last, else 0.
static void Clock_UpdateTwilight(struct Clock *clock)
{
    u32 tod = clock->hours * MINUTES_PER_HOUR + clock->minutes;

    if (tod >= clock->sunriseTime && tod < clock->sunriseTime + TWILIGHT_MINUTES)
        sTwilightCounter = tod - clock->sunriseTime + 1;
    else if (tod >= clock->sunsetTime - TWILIGHT_MINUTES && tod < clock->sunsetTime)
        sTwilightCounter = tod - (clock->sunsetTime - TWILIGHT_MINUTES) + 1;
    else
        sTwilightCounter = 0;
}

// Recomputes the time-of-day band for the current time of day. Dawn/dusk are the
// sunrise/sunset twilight windows; midnight and midday are the first 30 minutes
// of 12am/12pm; everything else is night (around midnight) or day.
static void Clock_UpdateTimeOfDay(struct Clock *clock)
{
    u32 tod = clock->hours * MINUTES_PER_HOUR + clock->minutes;

    if (tod < SOLAR_BAND_LENGTH)
        sTimeOfDay = TIME_OF_DAY_MIDNIGHT;
    else if (tod < clock->sunriseTime)
        sTimeOfDay = TIME_OF_DAY_NIGHT;
    else if (tod < clock->sunriseTime + TWILIGHT_MINUTES)
        sTimeOfDay = TIME_OF_DAY_DAWN;
    else if (tod < NOON_MINUTES)
        sTimeOfDay = TIME_OF_DAY_DAY;
    else if (tod < NOON_MINUTES + SOLAR_BAND_LENGTH)
        sTimeOfDay = TIME_OF_DAY_MIDDAY;
    else if (tod < clock->sunsetTime - TWILIGHT_MINUTES)
        sTimeOfDay = TIME_OF_DAY_DAY;
    else if (tod < clock->sunsetTime)
        sTimeOfDay = TIME_OF_DAY_DUSK;
    else
        sTimeOfDay = TIME_OF_DAY_NIGHT;
}

// Returns the current day of the week (WEEKDAY_SUN..WEEKDAY_SAT), counted
// from the number of whole days elapsed since the year 0, Jan 1 epoch.
u32 Clock_GetDayOfWeek(void)
{
    return (DaysSinceEpoch(&gSaveBlock1Ptr->gameClock) + EPOCH_WEEKDAY) % 7;
}

// Whole days elapsed since the epoch. Replaces the old RTC gLocalTime.days.
u32 Clock_GetDayCount(void)
{
    return DaysSinceEpoch(&gSaveBlock1Ptr->gameClock);
}

// Absolute minute count since the epoch. Replaces RTC-based total-minute math.
u32 Clock_GetTotalMinutes(void)
{
    return AbsMinutes(&gSaveBlock1Ptr->gameClock);
}

// Minutes into the current twilight window (1..TWILIGHT_MINUTES), or 0 if it is
// not currently twilight.
u32 Clock_GetTwilight(void)
{
    return sTwilightCounter;
}

// Current time-of-day band (TIME_OF_DAY_*).
u32 Clock_GetTimeOfDay(void)
{
    return sTimeOfDay;
}

// Writes the current time-of-day band name to dest (e.g. "MIDDAY"). Returns a
// pointer to the terminator.
u8 *Clock_GetTimeOfDayString(u8 *dest)
{
    return StringCopy(dest, sTimeOfDayNames[sTimeOfDay]);
}

// Current season, derived from the month (boundaries on the 1st of Mar/Jun/Sep/Dec).
u32 Clock_GetSeason(void)
{
    u32 month = gSaveBlock1Ptr->gameClock.month;

    if (month < MONTH_JAN || month > MONTH_DEC)
        month = MONTH_JAN; // defensive: stale clock with an out-of-range month
    return ((month + 9) % MONTH_COUNT) / 3; // shift Mar to 0, group months in threes
}

// Writes the current season name to dest ("AUTUMN", or "ATMN" abbreviated).
// Returns a pointer to the terminator.
u8 *Clock_GetSeasonString(u8 *dest, bool32 abbreviate)
{
    const u8 *const *names = abbreviate ? sSeasonNamesShort : sSeasonNames;
    return StringCopy(dest, names[Clock_GetSeason()]);
}

// Day index within the current season (0 = first day), and the season's length
// in days via lengthOut. Winter spans the year boundary, so its Jan/Feb fall in
// the year after its December, and it runs a day longer in a leap year.
static u32 SeasonDayIndex(struct Clock *clock, u32 *lengthOut)
{
    u32 season = Clock_GetSeason();
    u32 startMonth = season * 3 + MONTH_MAR; // MAR, JUN, SEP, or DEC
    u32 startYear = clock->year;
    u32 dayInSeason = 0;
    u32 length = 0;
    u32 i;

    if (season == SEASON_WINTER && clock->month != MONTH_DEC)
        startYear = clock->year - 1;

    for (i = 0; i < 3; i++)
    {
        u32 month = ((startMonth - 1 + i) % MONTH_COUNT) + 1;
        u32 year = (season == SEASON_WINTER && month != MONTH_DEC) ? startYear + 1 : startYear;

        if (month == clock->month)
            dayInSeason = length + (clock->day - 1);
        length += DaysInMonth(month, year);
    }

    *lengthOut = length;
    return dayInSeason;
}

// Phase within the season (SEASON_PHASE_*): the first or last two weeks, else mid.
u32 Clock_GetSeasonPhase(void)
{
    u32 length;
    u32 dayInSeason = SeasonDayIndex(&gSaveBlock1Ptr->gameClock, &length);
    u32 lateDays = SEASON_PHASE_DAYS;

    // The leap day lands at the end of winter (length 91, not 90); extend late
    // winter by it so the window keeps its usual start date and just absorbs Feb 29.
    if (Clock_GetSeason() == SEASON_WINTER && length > 90)
        lateDays++;

    if (dayInSeason < SEASON_PHASE_DAYS)
        return SEASON_PHASE_EARLY;
    if (dayInSeason >= length - lateDays)
        return SEASON_PHASE_LATE;
    return SEASON_PHASE_MID;
}

// Writes the phase-prefixed season name ("EARLY AUTUMN", or "E. ATMN" abbreviated;
// mid season has no prefix). Returns a pointer to the terminator.
u8 *Clock_GetFullSeasonString(u8 *dest, bool32 abbreviate)
{
    switch (Clock_GetSeasonPhase())
    {
    case SEASON_PHASE_EARLY:
        dest = StringCopy(dest, abbreviate ? sSeasonPhaseShort_Early : sSeasonPhase_Early);
        break;
    case SEASON_PHASE_LATE:
        dest = StringCopy(dest, abbreviate ? sSeasonPhaseShort_Late : sSeasonPhase_Late);
        break;
    }
    return Clock_GetSeasonString(dest, abbreviate);
}

// Season predicates over the given season (SEASON_*). IsSeason matches it in any
// phase; the others also require that the season be in that early/mid/late phase.
bool32 IsSeason(u32 season)      { return Clock_GetSeason() == season; }
bool32 IsEarlySeason(u32 season) { return IsSeason(season) && Clock_GetSeasonPhase() == SEASON_PHASE_EARLY; }
bool32 IsMidSeason(u32 season)   { return IsSeason(season) && Clock_GetSeasonPhase() == SEASON_PHASE_MID; }
bool32 IsLateSeason(u32 season)  { return IsSeason(season) && Clock_GetSeasonPhase() == SEASON_PHASE_LATE; }

// Convenience predicates over the current time-of-day band.
bool32 IsDay(void)      { return sTimeOfDay == TIME_OF_DAY_DAY || sTimeOfDay == TIME_OF_DAY_MIDDAY; }
bool32 IsNight(void)    { return sTimeOfDay == TIME_OF_DAY_NIGHT || sTimeOfDay == TIME_OF_DAY_MIDNIGHT; }
bool32 IsMidday(void)   { return sTimeOfDay == TIME_OF_DAY_MIDDAY; }
bool32 IsMidnight(void) { return sTimeOfDay == TIME_OF_DAY_MIDNIGHT; }
bool32 IsDawn(void)     { return sTimeOfDay == TIME_OF_DAY_DAWN; }
bool32 IsDusk(void)     { return sTimeOfDay == TIME_OF_DAY_DUSK; }
bool32 IsTwilight(void) { return sTimeOfDay == TIME_OF_DAY_DAWN || sTimeOfDay == TIME_OF_DAY_DUSK; }

// Half-of-day predicates. DAY straddles noon, so these check the clock too.
bool32 IsMorning(void)
{
    struct Clock *clock = &gSaveBlock1Ptr->gameClock;
    u32 tod = clock->hours * MINUTES_PER_HOUR + clock->minutes;

    return sTimeOfDay == TIME_OF_DAY_DAWN
        || (sTimeOfDay == TIME_OF_DAY_DAY && tod < NOON_MINUTES);
}

bool32 IsAfternoon(void)
{
    struct Clock *clock = &gSaveBlock1Ptr->gameClock;
    u32 tod = clock->hours * MINUTES_PER_HOUR + clock->minutes;

    return sTimeOfDay == TIME_OF_DAY_MIDDAY
        || sTimeOfDay == TIME_OF_DAY_DUSK
        || (sTimeOfDay == TIME_OF_DAY_DAY && tod >= NOON_MINUTES);
}

// Writes the current time to dest as "9:30AM" (12-hour, no leading zero on the
// hour). Returns a pointer to the terminator so calls can be chained.
u8 *Clock_GetTimeString(u8 *dest)
{
    struct Clock *clock = &gSaveBlock1Ptr->gameClock;
    u32 hour12 = clock->hours % 12;

    if (hour12 == 0)
        hour12 = 12; // midnight and noon read as 12, not 0

    dest = ConvertIntToDecimalStringN(dest, hour12, STR_CONV_MODE_LEFT_ALIGN, 2);
    *dest++ = CHAR_COLON;
    dest = ConvertIntToDecimalStringN(dest, clock->minutes, STR_CONV_MODE_LEADING_ZEROS, 2);
    return StringAppend(dest, clock->hours < 12 ? sText_AM : sText_PM);
}

// Writes the current day of the week to dest (e.g. "Monday", or "Mon" if
// abbreviate). Returns a pointer to the terminator.
u8 *Clock_GetDayString(u8 *dest, bool32 abbreviate)
{
    u32 weekday = Clock_GetDayOfWeek();

    return StringCopy(dest, abbreviate ? sDayNamesShort[weekday] : sDayNames[weekday]);
}

// Writes the current month to dest (e.g. "May", or "May"/"Jan" if abbreviate).
// Returns a pointer to the terminator.
u8 *Clock_GetMonthString(u8 *dest, bool32 abbreviate)
{
    u32 month = gSaveBlock1Ptr->gameClock.month - 1;

    return StringCopy(dest, abbreviate ? sMonthNamesShort[month] : sMonthNames[month]);
}

// ---------------------------------------------------------------------------
// Interval hooks. Each runs when the clock crosses the matching boundary as
// in-game time passes (aligned to the clock: OnHour at :00, OnDay at midnight,
// OnMonth on the 1st, OnYear in January). They are intentionally empty - fill
// in a body here to make something happen every minute, hour, day, etc.
// ---------------------------------------------------------------------------

static void Clock_OnMinute(void)
{
    BerryTreeTimeUpdate(1);
    Clock_UpdateTwilight(&gSaveBlock1Ptr->gameClock);
    Clock_UpdateTimeOfDay(&gSaveBlock1Ptr->gameClock);
}
static void Clock_On5Minutes(void)  {}
static void Clock_On10Minutes(void) {}
static void Clock_On15Minutes(void) {}
static void Clock_On20Minutes(void) {}
static void Clock_On30Minutes(void) {}
static void Clock_OnHour(void)      {}
static void Clock_On2Hours(void)    {}
static void Clock_On3Hours(void)    {}
static void Clock_On4Hours(void)    {}
static void Clock_On6Hours(void)    {}
static void Clock_On12Hours(void)   {}
static void Clock_OnDay(void)
{
    ClearDailyFlags();
    UpdateDewfordTrendPerDay(1);
    UpdateTVShowsPerDay(1);
    UpdatePartyPokerusTime(1);
    UpdateMirageRnd(1);
    UpdateBirchState(1);
    UpdateFrontierManiac(1);
    UpdateFrontierGambler(1);
    SetShoalItemFlag(1);
    SetRandomLotteryNumber(1);
}
static void Clock_On3Days(void)     {}
static void Clock_On5Days(void)     {}
static void Clock_OnWeek(void)      {}
static void Clock_On2Weeks(void)    {}
static void Clock_OnMonth(void)     {}
static void Clock_On2Months(void)   {}
static void Clock_On3Months(void)   {}
static void Clock_On4Months(void)   {}
static void Clock_On6Months(void)   {}
static void Clock_OnYear(void)      {}

// Fire when the clock passes the day's sunrise/sunset
static void Clock_OnSunrise(void)   {}
static void Clock_OnSunset(void)    {}

// Fires each interval hook whose boundary was crossed between the before/after snapshots. A boundary
// of N units is crossed when before/N != after/N, so this stays correct even if a tick spans several.
static void Clock_FireIntervals(u32 beforeMin, u32 afterMin, u32 beforeMonth, u32 afterMonth)
{
    u32 beforeDay = beforeMin / MINUTES_PER_DAY;
    u32 afterDay = afterMin / MINUTES_PER_DAY;

    if (afterMin == beforeMin)
        return; // less than a minute passed; no boundary can have been crossed

#define MIN_CROSSED(n)   ((beforeMin / (n)) != (afterMin / (n)))
#define DAY_CROSSED(n)   ((beforeDay / (n)) != (afterDay / (n)))
#define MONTH_CROSSED(n) ((beforeMonth / (n)) != (afterMonth / (n)))

    if (MIN_CROSSED(1))    Clock_OnMinute();
    if (MIN_CROSSED(5))    Clock_On5Minutes();
    if (MIN_CROSSED(10))   Clock_On10Minutes();
    if (MIN_CROSSED(15))   Clock_On15Minutes();
    if (MIN_CROSSED(20))   Clock_On20Minutes();
    if (MIN_CROSSED(30))   Clock_On30Minutes();
    if (MIN_CROSSED(60))   Clock_OnHour();
    if (MIN_CROSSED(120))  Clock_On2Hours();
    if (MIN_CROSSED(180))  Clock_On3Hours();
    if (MIN_CROSSED(240))  Clock_On4Hours();
    if (MIN_CROSSED(360))  Clock_On6Hours();
    if (MIN_CROSSED(720))  Clock_On12Hours();
    if (DAY_CROSSED(1))    Clock_OnDay();
    if (DAY_CROSSED(3))    Clock_On3Days();
    if (DAY_CROSSED(5))    Clock_On5Days();
    if (DAY_CROSSED(7))    Clock_OnWeek();
    if (DAY_CROSSED(14))   Clock_On2Weeks();
    if (MONTH_CROSSED(1))  Clock_OnMonth();
    if (MONTH_CROSSED(2))  Clock_On2Months();
    if (MONTH_CROSSED(3))  Clock_On3Months();
    if (MONTH_CROSSED(4))  Clock_On4Months();
    if (MONTH_CROSSED(6))  Clock_On6Months();
    if (MONTH_CROSSED(12)) Clock_OnYear();

#undef MIN_CROSSED
#undef DAY_CROSSED
#undef MONTH_CROSSED
}

// True if the given time-of-day (minutes past midnight) falls in the half-open
// span (beforeMin, afterMin] of absolute minutes, i.e. the clock just passed it.
static bool32 CrossedTimeOfDay(u32 beforeMin, u32 afterMin, u32 timeOfDay)
{
    u32 target = (beforeMin / MINUTES_PER_DAY) * MINUTES_PER_DAY + timeOfDay;

    if (target <= beforeMin)
        target += MINUTES_PER_DAY; // already passed today; check the next occurrence

    return target <= afterMin;
}

// Fires the sunrise/sunset hooks when the clock crosses either time, then
// refreshes the stored time so it tracks the date as the seasons drift.
static void Clock_FireSunEvents(u32 beforeMin, u32 afterMin)
{
    struct Clock *clock = &gSaveBlock1Ptr->gameClock;

    if (afterMin == beforeMin)
        return;

    if (CrossedTimeOfDay(beforeMin, afterMin, clock->sunriseTime))
    {
        Clock_OnSunrise();
        clock->sunriseTime = CalcSunriseTime(clock);
    }
    if (CrossedTimeOfDay(beforeMin, afterMin, clock->sunsetTime))
    {
        Clock_OnSunset();
        clock->sunsetTime = CalcSunsetTime(clock);
    }
}

// Advances the calendar clock by the given number of seconds, rolling over
// minutes, hours, days, months and years as needed, then runs any interval
// hooks whose boundary was crossed.
void Clock_AdvanceSeconds(u32 seconds)
{
    struct Clock *clock = &gSaveBlock1Ptr->gameClock;
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

    Clock_FireIntervals(beforeMin, AbsMinutes(clock), beforeMonth, AbsMonths(clock));
    Clock_FireSunEvents(beforeMin, AbsMinutes(clock));
}

// The clock only advances while the player is roaming the overworld freely.
// Field controls are locked by scripts, message boxes, NPC/signpost
// interactions and the start/debug menus; a non-overworld callback2 means a
// full-screen menu (bag, party, etc.) is open.
static bool32 ShouldClockTick(void)
{
    return gMain.callback2 == CB2_Overworld && !ArePlayerFieldControlsLocked();
}

// Called every frame. Locked at 60 fps, so 60 frames advance the clock by
// timeScale in-game seconds (timeScale = in-game seconds per real second).
void Clock_Update(void)
{
    if (!ShouldClockTick())
        return;

    if (++sClockVBlanks < FRAMES_PER_SECOND)
        return;

    sClockVBlanks = 0;
    Clock_AdvanceSeconds(gSaveBlock1Ptr->gameClock.timeScale);
}

// Recomputes the RAM-only derived state (twilight window and time-of-day band)
// from the current clock, so it stays correct after a setter jumps the time.
// Season and season-phase are derived on demand, so they need no refresh.
static void Clock_RefreshDerived(struct Clock *clock)
{
    Clock_UpdateTwilight(clock);
    Clock_UpdateTimeOfDay(clock);
}

// Clamps the day into the current month's valid range; call after changing month/year, since a
// shorter month (or a non-leap February) can leave the stored day past the end of the month.
static void ClampDay(struct Clock *clock)
{
    u8 max = DaysInMonth(clock->month, clock->year);

    if (clock->day < 1)
        clock->day = 1;
    else if (clock->day > max)
        clock->day = max;
}

void Clock_SetMinutes(u32 minutes)
{
    struct Clock *clock = &gSaveBlock1Ptr->gameClock;

    clock->minutes = minutes % MINUTES_PER_HOUR;
    Clock_RefreshDerived(clock);
}

void Clock_SetHours(u32 hours)
{
    struct Clock *clock = &gSaveBlock1Ptr->gameClock;

    clock->hours = hours % HOURS_PER_DAY;
    Clock_RefreshDerived(clock);
}

void Clock_SetDay(u32 day)
{
    struct Clock *clock = &gSaveBlock1Ptr->gameClock;
    u8 max = DaysInMonth(clock->month, clock->year);

    if (day < 1)
        day = 1;
    else if (day > max)
        day = max;
    clock->day = day;
    Clock_RecalcSunTimes(clock);
    Clock_RefreshDerived(clock);
}

void Clock_SetMonth(u32 month)
{
    struct Clock *clock = &gSaveBlock1Ptr->gameClock;

    if (month < MONTH_JAN)
        month = MONTH_JAN;
    else if (month > MONTH_DEC)
        month = MONTH_DEC;
    clock->month = month;
    ClampDay(clock);
    Clock_RecalcSunTimes(clock);
    Clock_RefreshDerived(clock);
}

void Clock_SetYear(u32 year)
{
    struct Clock *clock = &gSaveBlock1Ptr->gameClock;

    clock->year = year;
    ClampDay(clock); // a leap/non-leap change can shorten February
    Clock_RecalcSunTimes(clock);
    Clock_RefreshDerived(clock);
}

void Clock_SetTimeScale(u32 scale)
{
    gSaveBlock1Ptr->gameClock.timeScale = scale; // in-game seconds per real second (0 pauses)
}
