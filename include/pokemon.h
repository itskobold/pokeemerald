#ifndef GUARD_POKEMON_H
#define GUARD_POKEMON_H

#include "constants/pokemon.h"
#include "sprite.h"

#define MON_DATA_PERSONALITY        0
#define MON_DATA_OT_ID              1
#define MON_DATA_NICKNAME           2
#define MON_DATA_LANGUAGE           3
#define MON_DATA_SANITY_IS_BAD_EGG  4
#define MON_DATA_SANITY_HAS_SPECIES 5
#define MON_DATA_SANITY_IS_EGG      6
#define MON_DATA_OT_NAME            7
#define MON_DATA_MARKINGS           8
#define MON_DATA_CHECKSUM           9
#define MON_DATA_10                10
#define MON_DATA_SPECIES           11
#define MON_DATA_HELD_ITEM         12
#define MON_DATA_MOVE1             13
#define MON_DATA_MOVE2             14
#define MON_DATA_MOVE3             15
#define MON_DATA_MOVE4             16
#define MON_DATA_PP1               17
#define MON_DATA_PP2               18
#define MON_DATA_PP3               19
#define MON_DATA_PP4               20
#define MON_DATA_PP_BONUSES        21
#define MON_DATA_COOL              22
#define MON_DATA_BEAUTY            23
#define MON_DATA_CUTE              24
#define MON_DATA_EXP               25
#define MON_DATA_HP_EV             26
#define MON_DATA_ATK_EV            27
#define MON_DATA_DEF_EV            28
#define MON_DATA_SPEED_EV          29
#define MON_DATA_SPATK_EV          30
#define MON_DATA_SPDEF_EV          31
#define MON_DATA_FRIENDSHIP        32
#define MON_DATA_SMART             33
#define MON_DATA_POKERUS           34
#define MON_DATA_MET_LOCATION      35
#define MON_DATA_MET_LEVEL         36
#define MON_DATA_MET_GAME          37
#define MON_DATA_POKEBALL          38
#define MON_DATA_HP_IV             39
#define MON_DATA_ATK_IV            40
#define MON_DATA_DEF_IV            41
#define MON_DATA_SPEED_IV          42
#define MON_DATA_SPATK_IV          43
#define MON_DATA_SPDEF_IV          44
#define MON_DATA_IS_EGG            45
#define MON_DATA_ALT_ABILITY       46
#define MON_DATA_TOUGH             47
#define MON_DATA_SHEEN             48
#define MON_DATA_OT_GENDER         49
#define MON_DATA_COOL_RIBBON       50
#define MON_DATA_BEAUTY_RIBBON     51
#define MON_DATA_CUTE_RIBBON       52
#define MON_DATA_SMART_RIBBON      53
#define MON_DATA_TOUGH_RIBBON      54
#define MON_DATA_STATUS            55
#define MON_DATA_LEVEL             56
#define MON_DATA_HP                57
#define MON_DATA_MAX_HP            58
#define MON_DATA_ATK               59
#define MON_DATA_DEF               60
#define MON_DATA_SPEED             61
#define MON_DATA_SPATK             62
#define MON_DATA_SPDEF             63
#define MON_DATA_MAIL              64
#define MON_DATA_SPECIES2          65
#define MON_DATA_IVS               66
#define MON_DATA_TYPE_1            67
#define MON_DATA_TYPE_2            68
#define MON_DATA_HIDDEN_TYPE       69
#define MON_DATA_ABILITY           70
#define MON_DATA_NATURE    		   71
#define MON_DATA_FATEFUL_ENCOUNTER 72
#define MON_DATA_OBEDIENCE         73
#define MON_DATA_KNOWN_MOVES       74
#define MON_DATA_RIBBON_COUNT      75
#define MON_DATA_RIBBONS           76
#define MON_DATA_ATK2              77
#define MON_DATA_DEF2              78
#define MON_DATA_SPEED2            79
#define MON_DATA_SPATK2            80
#define MON_DATA_SPDEF2            81
#define MON_DATA_RARITY			   82

#define MAX_LEVEL 100

#define OT_ID_RANDOM_NO_SHINY 2
#define OT_ID_PRESET 1
#define OT_ID_PLAYER_ID 0

#define MON_GIVEN_TO_PARTY      0x0
#define MON_GIVEN_TO_PC         0x1
#define MON_CANT_GIVE           0x2

#define PLAYER_HAS_TWO_USABLE_MONS              0x0
#define PLAYER_HAS_ONE_MON                      0x1
#define PLAYER_HAS_ONE_USABLE_MON               0x2

#define MON_MALE       0x00
#define MON_FEMALE     0xFE
#define MON_GENDERLESS 0xFF

#define FRIENDSHIP_EVENT_GROW_LEVEL           0x0
#define FRIENDSHIP_EVENT_VITAMIN              0x1 // unused
#define FRIENDSHIP_EVENT_BATTLE_ITEM          0x2 // unused
#define FRIENDSHIP_EVENT_LEAGUE_BATTLE        0x3
#define FRIENDSHIP_EVENT_LEARN_TMHM           0x4
#define FRIENDSHIP_EVENT_WALKING              0x5
#define FRIENDSHIP_EVENT_FAINT_SMALL          0x6
#define FRIENDSHIP_EVENT_FAINT_OUTSIDE_BATTLE 0x7
#define FRIENDSHIP_EVENT_FAINT_LARGE          0x8

#define STATUS_PRIMARY_NONE      0x0
#define STATUS_PRIMARY_POISON    0x1
#define STATUS_PRIMARY_PARALYSIS 0x2
#define STATUS_PRIMARY_SLEEP     0x3
#define STATUS_PRIMARY_FREEZE    0x4
#define STATUS_PRIMARY_BURN      0x5
#define STATUS_PRIMARY_POKERUS   0x6
#define STATUS_PRIMARY_FAINTED   0x7

#define MAX_TOTAL_EVS 510
#define UNOWN_FORM_COUNT 28

#define NUM_ABILITIES 78 //don't think this exists already. remove & replace if it does
#define NUM_BANNED_RANDOM_ABILITIES 3 //number of abilities in random ability banlist
#define NUM_BANNED_RANDOM_MOVES 6 //number of moves in random move banlist
#define NUM_BANNED_RANDOM_MONS 47 //number of Pokemon in random mon banlist
#define NUM_MONS_WITH_BRANCHING_EVOS 8 //number of pokemon with evolution lines that branch out (like eevee, gloom, poliwhirl etc)

enum
{
	COSMETIC_RARITY_NONE,
	COSMETIC_RARITY_TYPICAL,
	COSMETIC_RARITY_COMMON,
	COSMETIC_RARITY_UNCOMMON,
	COSMETIC_RARITY_LESSER,
	COSMETIC_RARITY_RARE,
	COSMETIC_RARITY_ELITE,
	COSMETIC_RARITY_EXOTIC,
	COSMETIC_RARITY_MYTHICAL,
	COSMETIC_RARITY_GOLD
};

struct PokemonSubstruct0
{
    u16 species;
    u16 heldItem;
    u32 experience;
    u8 ppBonuses;
    u8 friendship;
};

struct PokemonSubstruct1
{
    u16 moves[4];
    u8 pp[4];
};

struct PokemonSubstruct2
{
    u8 hpEV;
    u8 attackEV;
    u8 defenseEV;
    u8 speedEV;
    u8 spAttackEV;
    u8 spDefenseEV;
    u8 type1;
    u8 type2;
    u8 hiddenType;
	u8 nature;
    u16 ability;
};

struct PokemonSubstruct3
{
 /* 0x00 */ u8 pokerus;
 /* 0x01 */ u8 metLocation;

 /* 0x02 */ u16 metLevel:7;
 /* 0x02 */ u16 metGame:4;
 /* 0x03 */ u16 filler4b:4;
 /* 0x03 */ u16 otGender:1;

 /* 0x04 */ u32 hpIV:5;
 /* 0x04 */ u32 attackIV:5;
 /* 0x05 */ u32 defenseIV:5;
 /* 0x05 */ u32 speedIV:5;
 /* 0x05 */ u32 spAttackIV:5;
 /* 0x06 */ u32 spDefenseIV:5;
 /* 0x07 */ u32 isEgg:1;
 /* 0x07 */ u32 altAbility:1;

 /* 0x08 */ u32 pokeball:6;
 /* 0x08 */ u32 rarity:4;
 /* 0x0A */ u32 filler17b:17;
 /* 0x0B */ u32 fatefulEncounter:4;
 /* 0x0B */ u32 obedient:1;
};

union PokemonSubstruct
{
    struct PokemonSubstruct0 type0;
    struct PokemonSubstruct1 type1;
    struct PokemonSubstruct2 type2;
    struct PokemonSubstruct3 type3;
    u16 raw[6];
};

struct BoxPokemon
{
    u32 personality;
    u32 otId;
    u8 nickname[POKEMON_NAME_LENGTH];
    u8 language;
    u8 isBadEgg:1;
    u8 hasSpecies:1;
    u8 isEgg:1;
    u8 unused:5;
    u8 otName[PLAYER_NAME_LENGTH];
    u8 markings;
    u16 checksum;
    u16 unknown;

    union
    {
        u32 raw[12];
        union PokemonSubstruct substructs[4];
    } secure;
};

struct Pokemon
{
    struct BoxPokemon box;
    u32 status;
    u8 level;
    u8 mail;
    u16 hp;
    u16 maxHP;
    u16 attack;
    u16 defense;
    u16 speed;
    u16 spAttack;
    u16 spDefense;
};

struct Unknown_806F160_Struct
{
    u8 field_0_0:4;
    u8 field_0_1:4;
    u8 field_1;
    u8 magic;
    u8 field_3_0:4;
    u8 field_3_1:4;
    void *bytes;
    u8 **byteArrays;
    struct SpriteTemplate *templates;
    struct SpriteFrameImage *frameImages;
};

struct BattlePokemon
{
    /*0x00*/ u16 species;
    /*0x02*/ u16 attack;
    /*0x04*/ u16 defense;
    /*0x06*/ u16 speed;
    /*0x08*/ u16 spAttack;
    /*0x0A*/ u16 spDefense;
    /*0x0C*/ u16 moves[4];
    /*0x14*/ u32 hpIV:5;
    /*0x14*/ u32 attackIV:5;
    /*0x15*/ u32 defenseIV:5;
    /*0x15*/ u32 speedIV:5;
    /*0x16*/ u32 spAttackIV:5;
    /*0x17*/ u32 spDefenseIV:5;
    /*0x17*/ u32 isEgg:1;
    /*0x17*/ u32 altAbility:1;
    /*0x18*/ s8 statStages[NUM_BATTLE_STATS];
    /*0x20*/ u16 ability;
    /*0x21*/ u8 type1;
    /*0x22*/ u8 type2;
    /*0x24*/ u8 pp[4];
    /*0x28*/ u16 hp;
    /*0x2A*/ u8 level;
    /*0x2B*/ u8 friendship;
    /*0x2C*/ u16 maxHP;
    /*0x2E*/ u16 item;
    /*0x30*/ u8 nickname[POKEMON_NAME_LENGTH + 1];
    /*0x3B*/ u8 ppBonuses;
    /*0x3C*/ u8 otName[8];
    /*0x44*/ u32 experience;
    /*0x48*/ u32 personality;
    /*0x4C*/ u32 status1;
    /*0x50*/ u32 status2;
    /*0x54*/ u32 otId;
};

struct BaseStats
{
 /* 0x00 */ u8 baseHP;
 /* 0x01 */ u8 baseAttack;
 /* 0x02 */ u8 baseDefense;
 /* 0x03 */ u8 baseSpeed;
 /* 0x04 */ u8 baseSpAttack;
 /* 0x05 */ u8 baseSpDefense;
 /* 0x06 */ u8 type1;
 /* 0x07 */ u8 type2;
 /* 0x08 */ u8 catchRate;
 /* 0x09 */ u8 expYield;
 /* 0x0A */ u16 evYield_HP:2;
 /* 0x0A */ u16 evYield_Attack:2;
 /* 0x0A */ u16 evYield_Defense:2;
 /* 0x0A */ u16 evYield_Speed:2;
 /* 0x0B */ u16 evYield_SpAttack:2;
 /* 0x0B */ u16 evYield_SpDefense:2;
 /* 0x0C */ u16 item1;
 /* 0x0E */ u16 item2;
 /* 0x10 */ u8 genderRatio;
 /* 0x11 */ u8 eggCycles;
 /* 0x12 */ u8 friendship;
 /* 0x13 */ u8 growthRate;
 /* 0x14 */ u8 eggGroup1;
 /* 0x15 */ u8 eggGroup2;
 /* 0x16 */ u8 ability1;
 /* 0x17 */ u8 ability2;
 /* 0x18 */ u8 safariZoneFleeRate;
 /* 0x19 */ u8 bodyColor : 7;
            u8 noFlip : 1;
};

struct BattleMove
{
    u8 effect;
    u8 power;
    u8 type;
    u8 accuracy;
    u8 pp;
    u8 secondaryEffectChance;
    u8 target;
    s8 priority;
    u8 flags;
	u8 pss;
};

#define FLAG_MAKES_CONTACT          0x1
#define FLAG_PROTECT_AFFECTED       0x2
#define FLAG_MAGICCOAT_AFFECTED     0x4
#define FLAG_SNATCH_AFFECTED        0x8
#define FLAG_MIRROR_MOVE_AFFECTED   0x10
#define FLAG_KINGSROCK_AFFECTED     0x20

struct SpindaSpot
{
    u8 x, y;
    u16 image[16];
};

struct __attribute__((packed)) LevelUpMove
{
    u16 move:9;
    u16 level:7;
};

enum
{
    GROWTH_MEDIUM_FAST,
    GROWTH_ERRATIC,
    GROWTH_FLUCTUATING,
    GROWTH_MEDIUM_SLOW,
    GROWTH_FAST,
    GROWTH_SLOW
};

enum
{
    BODY_COLOR_RED,
    BODY_COLOR_BLUE,
    BODY_COLOR_YELLOW,
    BODY_COLOR_GREEN,
    BODY_COLOR_BLACK,
    BODY_COLOR_BROWN,
    BODY_COLOR_PURPLE,
    BODY_COLOR_GRAY,
    BODY_COLOR_WHITE,
    BODY_COLOR_PINK
};

#define EVO_LEVEL_FRIENDSHIP       	0x0001 // Pokémon levels up with friendship at a certain value
#define EVO_LEVEL_MALE       		0x0002 // Pokémon reaches a certain level and is male
#define EVO_LEVEL_FEMALE     		0x0003 // Pokémon reaches a certain level and is female
#define EVO_LEVEL            		0x0004 // Pokémon reaches the specified level
#define EVO_LEVEL_MOVE        		0x0005 // Pokémon reaches a certain level knowing a move
#define EVO_MAP	      	 			0x0006 // Pokémon levels up in a map
#define EVO_ITEM             		0x0007 // specified item is used on Pokémon
#define EVO_LEVEL_ATK_GT_DEF 		0x0008 // Pokémon reaches the specified level with attack > defense
#define EVO_LEVEL_ATK_EQ_DEF 		0x0009 // Pokémon reaches the specified level with attack = defense
#define EVO_LEVEL_ATK_LT_DEF 		0x000a // Pokémon reaches the specified level with attack < defense
#define EVO_LEVEL_SILCOON    		0x000b // Pokémon reaches the specified level with a Silcoon personality value
#define EVO_LEVEL_CASCOON    		0x000c // Pokémon reaches the specified level with a Cascoon personality value
#define EVO_LEVEL_NINJASK    		0x000d // Pokémon reaches the specified level (special value for Ninjask)
#define EVO_LEVEL_SHEDINJA   		0x000e // Pokémon reaches the specified level (special value for Shedinja)
#define EVO_LEVEL_DAY_ONLY   		0x000f // Pokémon reaches a certain level during the day. Use when mon can evolve in twilight as well
#define EVO_LEVEL_NIGHT_ONLY 		0x0010 // Pokémon reaches a certain level at night. Use when mon can evolve in twilight as well
#define EVO_LEVEL_DAY   	   		0x0011 // Pokémon reaches a certain level during dawn/day
#define EVO_LEVEL_NIGHT 	   		0x0012 // Pokémon reaches a certain level at dusk/night
#define EVO_LEVEL_TWILIGHT   		0x0013 // Pokémon reaches a certain level at dawn/dusk
#define EVO_LEVEL_SPATK_GT_SPDEF 	0x0014 // Pokémon reaches the specified level with special attack > special defense
#define EVO_LEVEL_SPATK_EQ_SPDEF 	0x0015 // Pokémon reaches the specified level with special attack = special defense
#define EVO_LEVEL_SPATK_LT_SPDEF 	0x0016 // Pokémon reaches the specified level with special attack < special defense
#define EVO_LEVEL_SPRING			0x0017 // Pokémon reaches a certain level in Spring
#define EVO_LEVEL_SUMMER			0x0018 // Pokémon reaches a certain level in Summer
#define EVO_LEVEL_FALL				0x0019 // Pokémon reaches a certain level in Fall
#define EVO_LEVEL_WINTER			0x001a // Pokémon reaches a certain level in Winter
#define EVO_LEVEL_HELD_ITEM			0x001b // Pokémon reaches a certain level holding an item
#define EVO_LEVEL_HP_EV				0x001c // Pokémon reaches a certain level with a certain EV value
#define EVO_LEVEL_ATK_EV			0x001d // Pokémon reaches a certain level with a certain EV value
#define EVO_LEVEL_DEF_EV			0x001e // Pokémon reaches a certain level with a certain EV value
#define EVO_LEVEL_SPEED_EV			0x001f // Pokémon reaches a certain level with a certain EV value
#define EVO_LEVEL_SPATK_EV			0x0020 // Pokémon reaches a certain level with a certain EV value
#define EVO_LEVEL_SPDEF_EV			0x0021 // Pokémon reaches a certain level with a certain EV value

#define NUM_LEVEL_BASED_EVOS 31

#define RARITY_GOLD			0
#define RARITY_MYTHICAL		4
#define RARITY_EXOTIC		16
#define RARITY_ELITE		64
#define RARITY_RARE			256
#define RARITY_LESSER		1024
#define RARITY_UNCOMMON		4096
#define RARITY_COMMON		16384
#define RARITY_TYPICAL		(0xFFFF - RARITY_MYTHICAL - RARITY_EXOTIC - RARITY_ELITE - RARITY_RARE - RARITY_LESSER - RARITY_UNCOMMON - RARITY_COMMON)

#define NUM_ABILITIES 78 //don't think this exists already. remove & replace if it does
#define NUM_BANNED_RANDOM_ABILITIES 3 //number of abilities in random ability banlist
#define NUM_BANNED_RANDOM_MOVES 6 //number of moves in random move banlist
#define NUM_BANNED_RANDOM_MONS 47 //number of Pokemon in random mon banlist
#define NUM_MAX_POSSIBLE_EVOLUTIONS 5 //this should really be in evolution.h but moving it causes problems so eh
#define NUM_MONS_WITH_BRANCHING_EVOS 8 //number of pokemon with evolution lines that branch out (like eevee, gloom, poliwhirl etc)

struct Evolution
{
    u16 method;
    u16 param;
	u16 param2;
    u16 targetSpecies;
};

#define EVOS_PER_MON 5

extern u8 gPlayerPartyCount;
extern struct Pokemon gPlayerParty[PARTY_SIZE];
extern u8 gEnemyPartyCount;
extern struct Pokemon gEnemyParty[PARTY_SIZE];
extern struct SpriteTemplate gMultiuseSpriteTemplate;

extern const struct BattleMove gBattleMoves[];
extern const u8 gFacilityClassToPicIndex[];
extern const u8 gFacilityClassToTrainerClass[];
extern const struct BaseStats gBaseStats[];
extern const struct Evolution gEvolutionTable[][EVOS_PER_MON];
extern const u32 gExperienceTables[][MAX_LEVEL + 1];
extern const u16 *const gLevelUpLearnsets[];
extern const u8 gPPUpGetMask[];
extern const u8 gPPUpSetMask[];
extern const u8 gPPUpAddMask[];
extern const u8 gStatStageRatios[][2];
extern const u16 gUnknown_08329D54[];
extern const struct SpriteTemplate gUnknown_08329D98[];
extern const s8 gNatureStatTable[][5];

void ZeroBoxMonData(struct BoxPokemon *boxMon);
void ZeroMonData(struct Pokemon *mon);
void ZeroPlayerPartyMons(void);
void ZeroEnemyPartyMons(void);
void CreateMon(struct Pokemon *mon, u16 species, u8 level, u8 fixedIV, u8 hasFixedPersonality, u32 fixedPersonality, u8 otIdType, u32 fixedOtId, u8 rarity, bool8 shinyCharm);
void CreateBoxMon(struct BoxPokemon *boxMon, u16 species, u8 level, u8 fixedIV, u8 hasFixedPersonality, u32 fixedPersonality, u8 otIdType, u32 fixedOtId, bool8 shinyCharm);
void CreateMonWithNature(struct Pokemon *mon, u16 species, u8 level, u8 fixedIV, u8 nature, bool8 shinyCharm);
void CreateMonWithGenderNatureLetter(struct Pokemon *mon, u16 species, u8 level, u8 fixedIV, u8 gender, u8 nature, u8 unownLetter, u8 rarity);
void CreateMonWithRarity(struct Pokemon *mon, u16 species, u8 level, u8 fixedIV, u16 rarity, u32 OtId); //untested
void CreateMaleMon(struct Pokemon *mon, u16 species, u8 level, u8 rarity);
void CreateMonWithIVsPersonality(struct Pokemon *mon, u16 species, u8 level, u32 ivs, u32 personality, u8 rarity);
void CreateMonWithIVsOTID(struct Pokemon *mon, u16 species, u8 level, u8 *ivs, u32 otId, u8 rarity);
void CreateMonWithEVSpread(struct Pokemon *mon, u16 species, u8 level, u8 fixedIV, u8 evSpread, u8 rarity);
void CreateBattleTowerMon(struct Pokemon *mon, struct BattleTowerPokemon *src, u8 rarity);
void CreateBattleTowerMon2(struct Pokemon *mon, struct BattleTowerPokemon *src, bool8 lvl50, u8 rarity);
void CreateApprenticeMon(struct Pokemon *mon, const struct Apprentice *src, u8 monId, u8 rarity);
void CreateMonWithEVSpreadNatureOTID(struct Pokemon *mon, u16 species, u8 level, u8 nature, u8 fixedIV, u8 evSpread, u32 otId, u8 rarity);
void sub_80686FC(struct Pokemon *mon, struct BattleTowerPokemon *dest);
void CreateObedientMon(struct Pokemon *mon, u16 species, u8 level, u8 fixedIV, u8 hasFixedPersonality, u32 fixedPersonality, u8 otIdType, u32 fixedOtId, u8 rarity);
bool8 sub_80688F8(u8 caseId, u8 battlerId);
void SetDeoxysStats(void);
u16 sub_8068B48(void);
u16 sub_8068BB0(void);
void CreateObedientEnemyMon(void);
void CalculateMonStats(struct Pokemon *mon);
void BoxMonToMon(const struct BoxPokemon *src, struct Pokemon *dest);
u8 GetLevelFromMonExp(struct Pokemon *mon);
u8 GetLevelFromBoxMonExp(struct BoxPokemon *boxMon);
u16 GiveMoveToMon(struct Pokemon *mon, u16 move);
u16 GiveMoveToBoxMon(struct BoxPokemon *boxMon, u16 move);
u16 GiveMoveToBattleMon(struct BattlePokemon *mon, u16 move);
void SetMonMoveSlot(struct Pokemon *mon, u16 move, u8 slot);
void SetBattleMonMoveSlot(struct BattlePokemon *mon, u16 move, u8 slot);
void GiveMonInitialMoveset(struct Pokemon *mon);
void GiveBoxMonInitialMoveset(struct BoxPokemon *boxMon);
u16 MonTryLearningNewMove(struct Pokemon *mon, bool8 firstMove);
void DeleteFirstMoveAndGiveMoveToMon(struct Pokemon *mon, u16 move);
void DeleteFirstMoveAndGiveMoveToBoxMon(struct BoxPokemon *boxMon, u16 move);
s32 CalculateBaseDamage(struct BattlePokemon *attacker, struct BattlePokemon *defender, u32 move, u16 sideStatus, u16 powerOverride, u8 typeOverride, u8 bankAtk, u8 bankDef);

u16 CalculateBaseStatTotal(u16 species);
u16 CalculateEvioliteBonus(u16 species);
bool8 IsRandomAbilityBanned(u16 ability);
void SetRandomAbilityForMon(struct Pokemon *mon);
void SetRandomAbility(struct BoxPokemon *boxMon);
void GenerateRandomNature(struct BoxPokemon *boxMon);
void GiveMonTypes(struct Pokemon *mon);
void GiveBoxMonTypes(struct BoxPokemon *boxMon);
void GenerateSuperRandomMovesetForMon(struct Pokemon *mon, s32 level, bool8 hatched);
void GenerateSuperRandomMovesetForBoxMon(struct BoxPokemon *boxMon, s32 level, bool8 hatched);
u16 GenerateSuperRandomMove(u8 moveType1, u8 moveType2);
bool8 IsRandomMoveBanned(u16 move);
bool8 IsRandomMonBanned(u16 species);
u8 GetSuperRandomMovesetSize(s32 level, bool8 hatched);
u16 GetRarity(struct Pokemon *mon);
u16 GetRarityOtIdPersonality(u32 otId, u32 personality);

u8 CountAliveMonsInBattle(u8 caseId);
#define BATTLE_ALIVE_EXCEPT_ACTIVE  0
#define BATTLE_ALIVE_ATK_SIDE       1
#define BATTLE_ALIVE_DEF_SIDE       2

u8 GetDefaultMoveTarget(u8 battlerId);
u8 GetMonGender(struct Pokemon *mon);
u8 GetBoxMonGender(struct BoxPokemon *boxMon);
u8 GetGenderFromSpeciesAndPersonality(u16 species, u32 personality);
void SetMultiuseSpriteTemplateToPokemon(u16 speciesTag, u8 battlerPosition);
void SetMultiuseSpriteTemplateToTrainerBack(u16 trainerSpriteId, u8 battlerPosition);
void SetMultiuseSpriteTemplateToTrainerFront(u16 arg0, u8 battlerPosition);

// These are full type signatures for GetMonData() and GetBoxMonData(),
// but they are not used since some code erroneously omits the third arg.
// u32 GetMonData(struct Pokemon *mon, s32 field, u8 *data);
// u32 GetBoxMonData(struct BoxPokemon *boxMon, s32 field, u8 *data);
u32 GetMonData();
u32 GetBoxMonData();

void SetMonData(struct Pokemon *mon, s32 field, const void *dataArg);
void SetBoxMonData(struct BoxPokemon *boxMon, s32 field, const void *dataArg);
void CopyMon(void *dest, void *src, size_t size);
u8 GiveMonToPlayer(struct Pokemon *mon);
u8 SendMonToPC(struct Pokemon* mon);
u8 CalculatePlayerPartyCount(void);
u8 CalculateEnemyPartyCount(void);
u8 GetMonsStateToDoubles(void);
u8 GetMonsStateToDoubles_2(void);
u16 GetAbilityBySpecies(u16 species, bool8 altAbility, u16 customAbility);
u16 GetMonAbility(struct Pokemon *mon);
void CreateSecretBaseEnemyParty(struct SecretBase *secretBaseRecord);
u8 GetSecretBaseTrainerPicIndex(void);
u8 GetSecretBaseTrainerClass(void);
bool8 IsPlayerPartyAndPokemonStorageFull(void);
bool8 IsPokemonStorageFull(void);
void GetSpeciesName(u8 *name, u16 species);
u8 CalculatePPWithBonus(u16 move, u8 ppBonuses, u8 moveIndex);
void RemoveMonPPBonus(struct Pokemon *mon, u8 moveIndex);
void RemoveBattleMonPPBonus(struct BattlePokemon *mon, u8 moveIndex);
void CopyPlayerPartyMonToBattleData(u8 battlerId, u8 partyIndex);
u16 GetItemHealAmount(u16 itemId, u16 maxHP);
bool8 ExecuteTableBasedItemEffect(struct Pokemon *mon, u16 item, u8 partyIndex, u8 moveIndex);
bool8 PokemonUseItemEffects(struct Pokemon *mon, u16 item, u8 partyIndex, u8 moveIndex, u8 e);
bool8 HealStatusConditions(struct Pokemon *mon, u32 battlePartyId, u32 healMask, u8 battlerId);
u8 *sub_806CF78(u16 itemId);
u8 GetNature(struct Pokemon *mon);
u8 GetNatureFromPersonality(u32 personality);
u16 GetEvolutionTargetSpecies(struct Pokemon *mon, u8 type, u16 evolutionItem);
u16 HoennPokedexNumToSpecies(u16 hoennNum);
u16 NationalPokedexNumToSpecies(u16 nationalNum);
u16 NationalToHoennOrder(u16 nationalNum);
u16 SpeciesToNationalPokedexNum(u16 species);
u16 SpeciesToHoennPokedexNum(u16 species);
u16 HoennToNationalOrder(u16 hoennNum);
u16 SpeciesToCryId(u16 species);
void sub_806D544(u16 species, u32 personality, u8 *dest);
void DrawSpindaSpots(u16 species, u32 personality, u8 *dest, u8 a4);
void EvolutionRenameMon(struct Pokemon *mon, u16 oldSpecies, u16 newSpecies);
bool8 sub_806D7EC(void);
bool16 GetLinkTrainerFlankId(u8 id);
s32 GetBattlerMultiplayerId(u16 a1);
u8 GetTrainerEncounterMusicId(u16 trainerOpponentId);
u16 ModifyStatByNature(u8 nature, u16 n, u8 statIndex);
u16 ModifyStatByScarf(u16 item, s32 n, u8 statIndex);
void AdjustFriendship(struct Pokemon *mon, u8 event);
void MonGainEVs(struct Pokemon *mon, u16 defeatedSpecies);
u16 GetMonEVCount(struct Pokemon *mon);
void RandomlyGivePartyPokerus(struct Pokemon *party);
u8 CheckPartyPokerus(struct Pokemon *party, u8 selection);
u8 CheckPartyHasHadPokerus(struct Pokemon *party, u8 selection);
void UpdatePartyPokerusTime(u16 days);
void PartySpreadPokerus(struct Pokemon *party);
bool8 TryIncrementMonLevel(struct Pokemon *mon);
u32 CanMonLearnTMHM(struct Pokemon *mon, u8 tm);
u32 CanSpeciesLearnTMHM(u16 species, u8 tm);
u8 GetMoveRelearnerMoves(struct Pokemon *mon, u16 *moves);
u8 GetLevelUpMovesBySpecies(u16 species, u16 *moves);
u8 GetNumberOfRelearnableMoves(struct Pokemon *mon);
u16 SpeciesToPokedexNum(u16 species);
bool32 IsSpeciesInHoennDex(u16 species);
void ClearBattleMonForms(void);
u16 GetBattleBGM(void);
void PlayBattleBGM(void);
void PlayMapChosenOrBattleBGM(u16 songId);
void sub_806E694(u16 songId);
const u32 *GetMonFrontSpritePal(struct Pokemon *mon);
const u32 *GetFrontSpritePalFromSpeciesAndPersonality(u16 species, u32 otId, u32 personality);
const struct CompressedSpritePalette *GetMonSpritePalStruct(struct Pokemon *mon);
const struct CompressedSpritePalette *GetMonSpritePalStructFromOtIdPersonality(u16 species, u32 otId , u32 personality);
bool32 IsHMMove2(u16 move);
bool8 IsMonSpriteNotFlipped(u16 species);
s8 GetMonFlavorRelation(struct Pokemon *mon, u8 flavor);
s8 GetFlavorRelationByPersonality(u32 personality, u8 flavor);
bool8 IsTradedMon(struct Pokemon *mon);
bool8 IsOtherTrainer(u32 otId, u8 *otName);
void MonRestorePP(struct Pokemon *mon);
void BoxMonRestorePP(struct BoxPokemon *boxMon);
void SetMonPreventsSwitchingString(void);
void SetWildMonHeldItem(void);
bool8 IsMonShiny(struct Pokemon *mon);
bool8 IsShinyOtIdPersonality(u32 otId, u32 personality);
const u8 *GetTrainerPartnerName(void);
void BattleAnimateFrontSprite(struct Sprite* sprite, u16 species, bool8 noCry, u8 arg3);
void DoMonFrontSpriteAnimation(struct Sprite* sprite, u16 species, bool8 noCry, u8 arg3);
void PokemonSummaryDoMonAnimation(struct Sprite* sprite, u16 species, bool8 oneFrame);
void StopPokemonAnimationDelayTask(void);
void BattleAnimateBackSprite(struct Sprite* sprite, u16 species);
u8 sub_806EF08(u8 arg0);
u8 sub_806EF84(u8 arg0, u8 arg1);
u16 sub_806EFF0(u16 arg0);
u16 FacilityClassToPicIndex(u16 facilityClass);
u16 PlayerGenderToFrontTrainerPicId(u8 playerGender);
void HandleSetPokedexFlag(u16 nationalNum, u8 caseId, u32 personality);
const u8 *GetTrainerClassNameFromId(u16 trainerId);
const u8 *GetTrainerNameFromId(u16 trainerId);
bool8 HasTwoFramesAnimation(u16 species);
bool8 sub_806F104(void);
struct Unknown_806F160_Struct *sub_806F2AC(u8 id, u8 arg1);
void sub_806F47C(u8 id);
u8 *sub_806F4F8(u8 id, u8 arg1);

#endif // GUARD_POKEMON_H
