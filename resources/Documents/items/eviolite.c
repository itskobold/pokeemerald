//the eviolite item!
//works on new rules - if a Pokemon can evolve, and it has a BST of less than 512, it will recieve a stat boost. also prevents evolution
//their BST is subtracted from 512, then the result is divided by 6 and added onto each of their base stats

//to do - make sure wild and trainer Pokemon stats are affected too (wild Pokemon caught holding eviolite are not affected - check if item held is eviolite and update stats on Pokemon creation) 

//firstly, open src/data/items_en.h
//create the following Eviolite item somewhere
//DON'T copy the entry below, there'll probably be some new parameters added to item definitions

    {
        .name = _("EVIOLITE"),
        .itemId = ITEM_EVIOLITE,
        .price = 10,								//don't forget to add a price!
        .holdEffect = HOLD_EFFECT_PREVENT_EVOLVE,	//eviolite prevents evolution!
        .holdEffectParam = 0,
        .description = gItemDescription_Eviolite,	//don't forget to add a description either!
        .importance = 0,
        .unk19 = 0, 								//this parameter might change
        .pocket = POCKET_ITEMS,
        .type = 4,
        .fieldUseFunc = ItemUseOutOfBattle_CannotUse,
        .battleUsage = 0,
        .battleUseFunc = NULL,
        .secondaryId = 0,
    },
	
//make sure to define the item in include/constants/items.h
//for testing purposes, the eviolite item was created after HM08 Dive in items_en.h, so in items.h it would be defined as...

#define ITEM_EVIOLITE 347

//...after "#define ITEM_HM08_DIVE 346"
//that's the actual item created!

//======================================================================================================================================

//in src/pokemon/pokemon_1.c -
//find the calc stat macro - begins with "#define CALC_STAT(base, iv, ev, statIndex, field)"
//it needs another parameter for the eviolite boost so change it to the one below

#define CALC_STAT(base, eviolite, iv, ev, statIndex, field)                	\ //eviolite is new here
{                                                               			\
    u8 baseStat = gBaseStats[species].base;                     			\
    s32 n = (((2 * baseStat + eviolite + iv + ev / 4) * level) / 100) + 5;  \ //eviolite is new here too
    u8 nature = GetNature(mon);                                 			\
    n = nature_stat_mod(nature, n, statIndex);                  			\
    SetMonData(mon, field, &n);                                 			\
}

//update the uses of the CALC_STAT macro to add the eviolite boost parameter -

    CALC_STAT(baseAttack, evioliteBoost, attackIV, attackEV, 1, MON_DATA_ATK)
    CALC_STAT(baseDefense, evioliteBoost, defenseIV, defenseEV, 2, MON_DATA_DEF)
    CALC_STAT(baseSpeed, evioliteBoost, speedIV, speedEV, 3, MON_DATA_SPEED)
    CALC_STAT(baseSpAttack, evioliteBoost, spAttackIV, spAttackEV, 4, MON_DATA_SPATK)
    CALC_STAT(baseSpDefense, evioliteBoost, spDefenseIV, spDefenseEV, 5, MON_DATA_SPDEF)

//add a function to calculate a Pokemon's base stat total
//this will make bst equal the Pokemon's base stat total. there might be a better way to do this part...

u16 CalculateBaseStatTotal(struct Pokemon *mon)
{
	u16 bst = 0;
    u16 species = GetMonData(mon, MON_DATA_SPECIES, NULL);

	u8 bstHP = gBaseStats[species].baseHP;
	u8 bstATK = gBaseStats[species].baseAttack;
	u8 bstDEF = gBaseStats[species].baseDefense;
	u8 bstSPD = gBaseStats[species].baseSpeed;
	u8 bstSPATK = gBaseStats[species].baseSpAttack;
	u8 bstSPDEF = gBaseStats[species].baseSpDefense;

	bst = (bstHP + bstATK + bstDEF + bstSPD + bstSPATK + bstSPDEF);
	return bst;
}

//in CalculateMonStats below the new CalculateBaseStatTotal function add the following declarations -

	u16 item = GetMonData(mon, MON_DATA_HELD_ITEM, NULL);
	u16 evioliteBoost = 0;

//a new bit of code needs to be added to calculate the boost each stat should recieve
//the Pokemon needs to be raised to a BST of (roughly) 512
//this code also contains a bit that prevents Pokemon with a BST of 512 or above being affected just to be sure

	if ((item == ITEM_EVIOLITE) && (gEvolutionTable[species][0].targetSpecies != SPECIES_NONE))	//if mon is holding eviolite & can evolve
	{
		u16 statTotal = CalculateBaseStatTotal(mon);
		if(statTotal > 512)
		{
			evioliteBoost = 0;
		}
		else
		{
			evioliteBoost = ((512 - statTotal) / 6);
		}
	}
	
//find the line "s32 n = 2 * gBaseStats[species].baseHP + hpIV;" not far below
//it needs to be changed to add the eviolite boost we calculated -

        s32 n = 2 * gBaseStats[species].baseHP + evioliteBoost + hpIV;
		
//we need to add an underflow check to the HP calculator
//this is because in vanilla, HP only increments. without these checks, a Pokemon's HP could end up at 0 or some ridiculous high number
//here is the original...

	else
    {
        if (currentHP == 0 && oldMaxHP == 0)
            currentHP = newMaxHP;
        else if (currentHP != 0)
            currentHP += newMaxHP - oldMaxHP;
        else
            return;
    }
	
//it needs to be replaced with this

	else
    {
        if (currentHP == 0 && oldMaxHP == 0)
            currentHP = newMaxHP;
        else if (currentHP != 0)
		{
			if (currentHP < (oldMaxHP - newMaxHP)) 	//check for underflow
				currentHP = 1;						//set HP to 1
			else
				currentHP += newMaxHP - oldMaxHP;
				if (currentHP == 0)
					currentHP = 1;					//make sure HP can never be 0
		}
		else
            return;
    }

//this is everything that needs to be changed in pokemon/pokemon_1.c!

//======================================================================================================================================

//now open src/field/party_menu.c
//we need to make the Pokemon's stats update when an item is given to it, or removed from it, and redraw the Pokemon's info in the menu
//this will probably also be useful for other items that affect a Pokemon's stats!

//find every instance that sets a Pokemon's held item data - look for "SetMonData" and "MON_DATA_HELD_ITEM"
//There should be two instances in PartyMenuUpdateMonHeldItem and PartyMenuTryGiveMonHeldItem_806ECE8 (which will probably be renamed)
//insert the following AFTER every entry -

	CalculateMonStats(ewram1C000.pokemon); //note that "ewram1C000.pokemon" will probably change - update it to the Pokemon being affected
	RedrawPokemonInfoInMenu(ewram1C000.unk5, ewram1C000.pokemon); //ewram1C000.unk5 will probably change too
	
//in PartyMenuTryGiveMonHeldItem_806ECE8, a graphical glitch will occur caused by this line -

	DisplayTakeHeldItemMessage(ewram1C000.unk5, currentItem, 0)
	
//find it and move it after the new RedrawPokemonInfoInMenu line to fix it and remove the first instance
//the whole section should look like this -

        {
            if (ItemIsMail(currentItem) == TRUE)
                TakeMailFromMon(ewram1C000.pokemon);
            SetMonData(ewram1C000.pokemon, MON_DATA_HELD_ITEM, itemData);
			CalculateMonStats(ewram1C000.pokemon);
		    RedrawPokemonInfoInMenu(ewram1C000.unk5, ewram1C000.pokemon);
            DisplayTakeHeldItemMessage(ewram1C000.unk5, currentItem, 0); //this line has been moved
        }
		
//here is the original for reference -

        {
            if (ItemIsMail(currentItem) == TRUE)
                TakeMailFromMon(ewram1C000.pokemon);
            DisplayTakeHeldItemMessage(ewram1C000.unk5, currentItem, 0);
            SetMonData(ewram1C000.pokemon, MON_DATA_HELD_ITEM, itemData);
        }
		
//======================================================================================================================================

//add a guide on making eviolite affect Pokemon if held immediately after creation here