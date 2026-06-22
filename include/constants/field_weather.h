#ifndef GUARD_CONSTANTS_FIELD_WEATHER_H
#define GUARD_CONSTANTS_FIELD_WEATHER_H

#define MAX_RAIN_SPRITES             24
#define NUM_FOG_HORIZONTAL_SPRITES   20
#define NUM_ASH_SPRITES              20
#define NUM_SANDSTORM_SPRITES        20
#define NUM_SWIRL_SANDSTORM_SPRITES  5
#define NUM_SNOWFLAKE_SPRITES        16

// Clouds: a 3x3 set of seamlessly tiling 64x64 images, drawn as a grid of
// overlay sprites (CLOUD_COLS x CLOUD_ROWS) that scrolls across the screen.
#define CLOUD_TILE_IMAGES            9   // 3x3 distinct source tiles
#define CLOUD_COVER_SETS             8   // selectable tile sets (clouds/0..7), chosen by cloudCover

// Cloud overlay brightness, stored as a 3-bit value offset by +2 so raw 0..5
// maps to actual -2..3 (negative darkens, positive lightens the overlay).
#define CLOUD_BRIGHTNESS_MIN         (-2)
#define CLOUD_BRIGHTNESS_MAX         3
#define CLOUD_BRIGHTNESS_DEFAULT     2
#define ToCloudBrightnessRaw(v)      ((v) - CLOUD_BRIGHTNESS_MIN)
#define FromCloudBrightnessRaw(v)    ((s8)(v) + CLOUD_BRIGHTNESS_MIN)
#define CLOUD_PATTERN_DIM            3   // tiles per side of the repeating pattern
#define CLOUD_COLS                   5   // sprite columns (cover screen width + scroll)
#define CLOUD_ROWS                   4   // sprite rows (cover screen height + scroll)
#define NUM_CLOUD_SPRITES            (CLOUD_COLS * CLOUD_ROWS)

// Controls how the weather should be changing the screen palettes.
#define WEATHER_PAL_STATE_CHANGING_WEATHER   0
#define WEATHER_PAL_STATE_SCREEN_FADING_IN   1
#define WEATHER_PAL_STATE_SCREEN_FADING_OUT  2
#define WEATHER_PAL_STATE_IDLE               3

// Modes for FadeScreen
#define FADE_FROM_BLACK  0
#define FADE_TO_BLACK    1
#define FADE_FROM_WHITE  2
#define FADE_TO_WHITE    3

#endif // GUARD_CONSTANTS_FIELD_WEATHER_H
