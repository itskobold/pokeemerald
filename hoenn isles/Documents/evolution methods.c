//evolve at a specific level if intellect is higher than resistance
//EVO_LEVEL_SPATK_GT_SPDEF, (level), SPECIES_(species)
			
			case EVO_LEVEL_SPATK_GT_SPDEF:
                if (gEvolutionTable[species][i].param <= level)
                    if (GetMonData(mon, MON_DATA_SPATK, 0) > GetMonData(mon, MON_DATA_SPDEF, 0))
                        targetSpecies = gEvolutionTable[species][i].targetSpecies;
                break;
				
//evolve at a specific level if intellect is equal to resistance
//EVO_LEVEL_SPATK_EQ_SPDEF, (level), SPECIES_(species)

            case EVO_LEVEL_SPATK_EQ_SPDEF:
                if (gEvolutionTable[species][i].param <= level)
                    if (GetMonData(mon, MON_DATA_SPATK, 0) == GetMonData(mon, MON_DATA_SPDEF, 0))
                        targetSpecies = gEvolutionTable[species][i].targetSpecies;
                break;
				
//evolve at a specific level if intellect is lower than resistance
//EVO_LEVEL_SPATK_LT_SPDEF, (level), SPECIES_(species)

            case EVO_LEVEL_SPATK_LT_SPDEF:
                if (gEvolutionTable[species][i].param <= level)
                    if (GetMonData(mon, MON_DATA_SPATK, 0) < GetMonData(mon, MON_DATA_SPDEF, 0))
                        targetSpecies = gEvolutionTable[species][i].targetSpecies;
                break;
				
//===================================================================================================================================================
				
//evolve at any level if holding a specific item
//literally exactly the same as trade with item!
//EVO_HELD_ITEM, (held item), SPECIES_(species)				

            case EVO_HELD_ITEM:
                if (gEvolutionTable[species][i].param == heldItem)
                {
                    heldItem = 0;
                    SetMonData(mon, MON_DATA_HELD_ITEM, &heldItem);
                    targetSpecies = gEvolutionTable[species][i].targetSpecies;
                }
                break;
				
//===================================================================================================================================================
				
//evolve at any level with a status condition

//Status numbers
//1 - Poison/toxic
//2 - Paralysis
//3 - Sleep
//4 - Freeze
//5 - Burn
//should probably change this to use STATUS_BURN etc. maybe later, this works for now

//ADD THE FOLLOWING INCLUDE TO THE TOP OF pokemon_3.c - "#include "pokemon_summary_screen.h""
//ADD TO DECLARATIONS IN GetEvolutionTargetSpecies - "u8 status;"

//EVO_STATUS_CONDITION, (status #), SPECIES_(species)

            case EVO_STATUS_CONDITION:
				status = pokemon_ailments_get_primary(GetMonData(mon, MON_DATA_STATUS));
                if (gEvolutionTable[species][i].param == status)
                {
                    status = 0;
                    SetMonData(mon, MON_DATA_STATUS, &status);
                    targetSpecies = gEvolutionTable[species][i].targetSpecies;
                }
                break;
				
//===================================================================================================================================================
				
//evolve at any level whilst fainted. fully restores HP on evolution
//this hasn't been tested but *should* work. only problem is there is no way to evolve with 0 HP as Rare Candy revives a fainted Pokemon
//EVO_FAINTED, 0, SPECIES_(species)

            case EVO_FAINTED:
                if (GetMonData(mon, MON_DATA_HP, 0) == 0)
                {
                    SetMonData(mon, MON_DATA_HP, MON_DATA_MAX_HP);
                    targetSpecies = gEvolutionTable[species][i].targetSpecies;
                }
                break;
				
//===================================================================================================================================================
				
//evolve at any level when a stat has an EV higher than the parameter
//ADD THE FOLLOWING TO DECLARATIONS IN GetEvolutionTargetSpecies...

//	u8 hpEV = GetMonData(mon, MON_DATA_HP_EV, 0);
//	u8 attackEV = GetMonData(mon, MON_DATA_ATK_EV, 0);
//	u8 defenseEV = GetMonData(mon, MON_DATA_DEF_EV, 0);
//	u8 speedEV = GetMonData(mon, MON_DATA_SPEED_EV, 0);
//	u8 spAttackEV = GetMonData(mon, MON_DATA_SPATK_EV, 0);
//	u8 spDefenseEV = GetMonData(mon, MON_DATA_SPDEF_EV, 0);

//EVO_HP_EV, (parameter), SPECIES_(species)
//EVO_ATK_EV, (parameter), SPECIES_(species)
//EVO_DEF_EV, (parameter), SPECIES_(species)
//EVO_SPEED_EV, (parameter), SPECIES_(species)
//EVO_SPATK_EV, (parameter), SPECIES_(species)
//EVO_SPDEF_EV, (parameter), SPECIES_(species)

            case EVO_HP_EV:
                if (gEvolutionTable[species][i].param <= hpEV)
                    targetSpecies = gEvolutionTable[species][i].targetSpecies;
                break;
            case EVO_ATK_EV:
                if (gEvolutionTable[species][i].param <= attackEV)
                    targetSpecies = gEvolutionTable[species][i].targetSpecies;
                break;
            case EVO_DEF_EV:
                if (gEvolutionTable[species][i].param <= defenseEV)
                    targetSpecies = gEvolutionTable[species][i].targetSpecies;
                break;
            case EVO_SPEED_EV:
                if (gEvolutionTable[species][i].param <= speedEV)
                    targetSpecies = gEvolutionTable[species][i].targetSpecies;
                break;
            case EVO_SPATK_EV:
                if (gEvolutionTable[species][i].param <= spAttackEV)
                    targetSpecies = gEvolutionTable[species][i].targetSpecies;
                break;
            case EVO_SPDEF_EV:
                if (gEvolutionTable[species][i].param <= spDefenseEV)
                    targetSpecies = gEvolutionTable[species][i].targetSpecies;
                break;
	
//===================================================================================================================================================
				
//gender based evolutions at a specific level (for Nidoran, Kirlia etc)
//EVO_LEVEL_MALE, (level), SPECIES_(species)
//EVO_LEVEL_FEMALE, (level), SPECIES_(species)

            case EVO_LEVEL_MALE:
                if (gEvolutionTable[species][i].param <= level && (GetMonGender(mon) == MON_MALE))
                    targetSpecies = gEvolutionTable[species][i].targetSpecies;
                break;
			case EVO_LEVEL_FEMALE:
                if (gEvolutionTable[species][i].param <= level && (GetMonGender(mon) == MON_FEMALE))
                    targetSpecies = gEvolutionTable[species][i].targetSpecies;
                break;