#ifndef GUARD_CONSTANTS_GLOBAL_H
#define GUARD_CONSTANTS_GLOBAL_H
// Invalid Versions show as "----------" in Gen 4 and Gen 5's summary screen.
// In Gens 6 and 7, invalid versions instead show "a distant land" in the summary screen.
// In Gen 4 only, migrated Pokemon with Diamond, Pearl, or Platinum's ID show as "----------".
// Gen 5 and up read Diamond, Pearl, or Platinum's ID as "Sinnoh".
// In Gen 4 and up, migrated Pokemon with HeartGold or SoulSilver's ID show the otherwise unused "Johto" string.
#define VERSION_SAPPHIRE 1
#define VERSION_RUBY 2
#define VERSION_EMERALD 3
#define VERSION_FIRE_RED 4
#define VERSION_LEAF_GREEN 5
#define VERSION_HEART_GOLD 7
#define VERSION_SOUL_SILVER 8
#define VERSION_DIAMOND 10
#define VERSION_PEARL 11
#define VERSION_PLATINUM 12
#define VERSION_GAMECUBE 15

#define LANGUAGE_JAPANESE 1
#define LANGUAGE_ENGLISH 2
#define LANGUAGE_FRENCH 3
#define LANGUAGE_ITALIAN 4
#define LANGUAGE_GERMAN 5
#define LANGUAGE_KOREAN 6 // 6 goes unused but the theory is it was meant to be Korean
#define LANGUAGE_SPANISH 7

#define GAME_VERSION (VERSION_EMERALD)
#define GAME_LANGUAGE (LANGUAGE_ENGLISH)

// capacities of various saveblock objects
#define DAYCARE_MON_COUNT 2
#define POKEBLOCKS_COUNT 40
#define EVENT_OBJECTS_COUNT 16
#define BERRY_TREES_COUNT 128
#define FLAGS_COUNT 300
#define VARS_COUNT 256
#define MAIL_COUNT 16
#define SECRET_BASES_COUNT 20
#define TV_SHOWS_COUNT 25
#define POKE_NEWS_COUNT 16
#define PC_ITEMS_COUNT 50
#define BAG_ITEMS_COUNT 30
#define BAG_KEYITEMS_COUNT 30
#define BAG_POKEBALLS_COUNT 16
#define BAG_TMHM_COUNT 64
#define BAG_BERRIES_COUNT 46
#define EVENT_OBJECT_TEMPLATES_COUNT 64

#define PYRAMID_BAG_ITEMS_COUNT 10
#define HALL_FACILITIES_COUNT 9 // 7 facilities for single mode + tower double mode + tower multi mode.

// string lengths
#define ITEM_NAME_LENGTH 14
#define POKEMON_NAME_LENGTH 10
#define PLAYER_NAME_LENGTH 7
#define MAIL_WORDS_COUNT 9
#define MOVE_NAME_LENGTH 12

#define MALE 0
#define FEMALE 1

#define WAIT_UNABLE 0		//player is unable to wait
#define WAIT_ABLE 1			//player is able to wait normally

#define OPTIONS_SOUND_MONO 0
#define OPTIONS_SOUND_STEREO 1

#define OPTIONS_BATTLE_STYLE_SHIFT 0
#define OPTIONS_BATTLE_STYLE_SET 1

#define OPTIONS_QUICK_FLEE_ON 0
#define OPTIONS_QUICK_FLEE_OFF 1

#define OPTIONS_KEYPAD_SOUND_ON 0
#define OPTIONS_KEYPAD_SOUND_OFF 1

#define OPTIONS_LOW_HP_SOUND_ON 0
#define OPTIONS_LOW_HP_SOUND_OFF 1

#define OPTIONS_BIKE_MODE_HOLD_B 0
#define OPTIONS_BIKE_MODE_AUTO 1

#define OPTIONS_FULL_PARTY_SWAP 0
#define OPTIONS_FULL_PARTY_SEND_TO_PC 1

#define OPTIONS_KEYBOARD_QWERTY 0
#define OPTIONS_KEYBOARD_QWERTY_PLUS 1
#define OPTIONS_KEYBOARD_ABC 2
#define OPTIONS_KEYBOARD_ABC_PLUS 3
#define OPTIONS_KEYBOARD_AZERTY 4
#define OPTIONS_KEYBOARD_AZERTY_PLUS 5
#define OPTIONS_KEYBOARD_DVORAK 6
#define OPTIONS_KEYBOARD_DVORAK_PLUS 7
#define OPTIONS_KEYBOARD_COLEMAK 8
#define OPTIONS_KEYBOARD_COLEMAK_PLUS 9
#define OPTIONS_KEYBOARD_VANILLA 10

#define OPTIONS_FONT_ROCKET 0
#define OPTIONS_FONT_MAGMA 1
#define OPTIONS_FONT_AQUA 2
#define OPTIONS_FONT_GALACTIC 3

#define NUZLOCKE_MODE_OFF 0
#define NUZLOCKE_MODE_NUZLOCKE 1
#define NUZLOCKE_MODE_HARDLOCKE 2
#define NUZLOCKE_MODE_DEADLOCKE 3

#define GAME_MODE_STORY 0
#define GAME_MODE_SANDBOX 1
#define GAME_MODE_RANDOM 2
#define GAME_MODE_SUPER_RANDOM 3

#endif // GUARD_CONSTANTS_GLOBAL_H
