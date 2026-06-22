#include "global.h"
#include "constants/weather.h"
#include "coord_event_weather.h"
#include "field_weather.h"

struct CoordEventWeather
{
    u8 coordEventWeather;
    void (*func)(void);
};

static void CoordEventWeather_Sunny(void);
static void CoordEventWeather_Rain(void);
static void CoordEventWeather_Snow(void);
static void CoordEventWeather_Thunderstorm(void);
static void CoordEventWeather_Ash(void);
static void CoordEventWeather_Sandstorm(void);
static void CoordEventWeather_Drought(void);

static const struct CoordEventWeather sCoordEventWeatherFuncs[] =
{
    { COORD_EVENT_WEATHER_SUNNY,             CoordEventWeather_Sunny },
    { COORD_EVENT_WEATHER_RAIN,              CoordEventWeather_Rain },
    { COORD_EVENT_WEATHER_SNOW,              CoordEventWeather_Snow },
    { COORD_EVENT_WEATHER_RAIN_THUNDERSTORM, CoordEventWeather_Thunderstorm },
    { COORD_EVENT_WEATHER_VOLCANIC_ASH,      CoordEventWeather_Ash },
    { COORD_EVENT_WEATHER_SANDSTORM,         CoordEventWeather_Sandstorm },
    { COORD_EVENT_WEATHER_DROUGHT,           CoordEventWeather_Drought },
};

static void CoordEventWeather_Sunny(void)
{
    SetWeather(WEATHER_SUNNY);
}

static void CoordEventWeather_Rain(void)
{
    SetWeather(WEATHER_RAIN);
}

static void CoordEventWeather_Snow(void)
{
    SetWeather(WEATHER_SNOW);
}

static void CoordEventWeather_Thunderstorm(void)
{
    SetWeather(WEATHER_RAIN_THUNDERSTORM);
}

static void CoordEventWeather_Ash(void)
{
    SetWeather(WEATHER_VOLCANIC_ASH);
}

static void CoordEventWeather_Sandstorm(void)
{
    SetWeather(WEATHER_SANDSTORM);
}

static void CoordEventWeather_Drought(void)
{
    SetWeather(WEATHER_DROUGHT);
}

void DoCoordEventWeather(u8 coordEventWeather)
{
    u8 i;
    for (i = 0; i < ARRAY_COUNT(sCoordEventWeatherFuncs); i++)
    {
        if (sCoordEventWeatherFuncs[i].coordEventWeather == coordEventWeather)
        {
            sCoordEventWeatherFuncs[i].func();
            return;
        }
    }
}
