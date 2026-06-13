#include "global.h"
#include "debug.h"
#include "event_data.h"
#include "item.h"
#include "malloc.h"
#include "pokedex.h"
#include "constants/flags.h"
#include "constants/moves.h"

void SetDebugNewGameFlags()
{
    FlagSet(FLAG_SYS_B_DASH);
    FlagSet(FLAG_SYS_POKEMON_GET);
    FlagSet(FLAG_SYS_POKENAV_GET);
    FlagSet(FLAG_SYS_POKEDEX_GET);
    FlagSet(FLAG_SYS_NATIONAL_DEX);
    FlagSet(FLAG_ADVENTURE_STARTED);
    FlagSet(FLAG_SYS_GAME_CLEAR);
    FlagSet(FLAG_BADGE01_GET);
    FlagSet(FLAG_BADGE02_GET);
    FlagSet(FLAG_BADGE03_GET);
    FlagSet(FLAG_BADGE04_GET);
    FlagSet(FLAG_BADGE05_GET);
    FlagSet(FLAG_BADGE06_GET);
    FlagSet(FLAG_BADGE07_GET);
    FlagSet(FLAG_BADGE08_GET);
    FlagSet(FLAG_VISITED_LITTLEROOT_TOWN);
    FlagSet(FLAG_VISITED_OLDALE_TOWN);
    FlagSet(FLAG_VISITED_DEWFORD_TOWN);
    FlagSet(FLAG_VISITED_LAVARIDGE_TOWN);
    FlagSet(FLAG_VISITED_FALLARBOR_TOWN);
    FlagSet(FLAG_VISITED_VERDANTURF_TOWN);
    FlagSet(FLAG_VISITED_PACIFIDLOG_TOWN);
    FlagSet(FLAG_VISITED_PETALBURG_CITY);
    FlagSet(FLAG_VISITED_SLATEPORT_CITY);
    FlagSet(FLAG_VISITED_MAUVILLE_CITY);
    FlagSet(FLAG_VISITED_RUSTBORO_CITY);
    FlagSet(FLAG_VISITED_FORTREE_CITY);
    FlagSet(FLAG_VISITED_LILYCOVE_CITY);
    FlagSet(FLAG_VISITED_MOSSDEEP_CITY);
    FlagSet(FLAG_VISITED_SOOTOPOLIS_CITY);
    FlagSet(FLAG_VISITED_EVER_GRANDE_CITY);

    struct Pokemon *mon = AllocZeroed(sizeof(struct Pokemon));
    CreateMon(mon, SPECIES_RAYQUAZA, 100, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);
    SetMonMoveSlot(mon, MOVE_FLY, 0);
    GiveMonToPlayer(mon);
    Free(mon);
    int nationalDexNum = SpeciesToNationalPokedexNum(SPECIES_RAYQUAZA);
    GetSetPokedexFlag(nationalDexNum, FLAG_SET_SEEN);
    GetSetPokedexFlag(nationalDexNum, FLAG_SET_CAUGHT);

    AddBagItem(ITEM_MASTER_BALL, 99);
    AddBagItem(ITEM_POKE_BALL, 99);
    AddBagItem(ITEM_FULL_RESTORE, 99);
    AddBagItem(ITEM_FULL_HEAL, 99);
    AddBagItem(ITEM_MAX_POTION, 99);
    AddBagItem(ITEM_SUPER_POTION, 99);
    AddBagItem(ITEM_MAX_REVIVE, 99);
    AddBagItem(ITEM_REVIVE, 99);
    AddBagItem(ITEM_MAX_ETHER, 99);
    AddBagItem(ITEM_ETHER, 99);
    AddBagItem(ITEM_MAX_REPEL, 99);
}
