/*
 *    sfall
 *    Copyright (C) 2008 - 2021 Timeslip and sfall team
 *
 */

#include "..\FalloutEngine\Fallout2.h"

#include "..\main.h"
#include "..\Modules\Perks.h"

#include "stats.h"

namespace game
{

namespace sf = sfall;

static bool smallFrameTraitFix = false;

static int DudeGetBaseStat(DWORD statID) {
	return fo::func::stat_get_base_direct(fo::var::obj_dude, statID);
}

static __forceinline bool CheckTrait(DWORD traitID) {
	return (sf::Perks::TraitIsDisabled(traitID) == false && (fo::var::pc_trait[0] == traitID || fo::var::pc_trait[1] == traitID));
}

int __stdcall Stats::trait_adjust_stat(DWORD statID) {
	if (statID > fo::STAT_max_derived) return 0;

	int result = 0;
	if (sf::Perks::TraitsModEnable()) {
		if (fo::var::pc_trait[0] != -1) result += sf::Perks::GetTraitStatBonus(statID, 0);
		if (fo::var::pc_trait[1] != -1) result += sf::Perks::GetTraitStatBonus(statID, 1);
	}

	switch (statID) {
	case fo::STAT_st:
		if (CheckTrait(fo::TRAIT_gifted)) result++;
		if (CheckTrait(fo::TRAIT_bruiser)) result += 2;
		break;
	case fo::STAT_pe:
		if (CheckTrait(fo::TRAIT_gifted)) result++;
		break;
	case fo::STAT_en:
		if (CheckTrait(fo::TRAIT_gifted)) result++;
		break;
	case fo::STAT_ch:
		if (CheckTrait(fo::TRAIT_gifted)) result++;
		break;
	case fo::STAT_iq:
		if (CheckTrait(fo::TRAIT_gifted)) result++;
		break;
	case fo::STAT_ag:
		if (CheckTrait(fo::TRAIT_gifted)) result++;
		if (CheckTrait(fo::TRAIT_small_frame)) result++;
		break;
	case fo::STAT_lu:
		if (CheckTrait(fo::TRAIT_gifted)) result++;
		break;
	case fo::STAT_max_move_points:
		if (CheckTrait(fo::TRAIT_bruiser)) result -= 2;
		break;
	case fo::STAT_ac:
		if (CheckTrait(fo::TRAIT_kamikaze)) return -DudeGetBaseStat(fo::STAT_ac);
		break;
	case fo::STAT_melee_dmg:
		if (CheckTrait(fo::TRAIT_heavy_handed)) result += 4;
		break;
	case fo::STAT_carry_amt:
		if (CheckTrait(fo::TRAIT_small_frame)) {
			int st;
			if (smallFrameTraitFix) {
				st = fo::func::stat_level(fo::var::obj_dude, fo::STAT_st);
			} else {
				st = DudeGetBaseStat(fo::STAT_st);
			}
			result -= st * 10;
		}
		break;
	case fo::STAT_sequence:
		if (CheckTrait(fo::TRAIT_kamikaze)) result += 5;
		break;
	case fo::STAT_heal_rate:
		if (CheckTrait(fo::TRAIT_fast_metabolism)) result += 2;
		break;
	case fo::STAT_crit_chance:
		if (CheckTrait(fo::TRAIT_finesse)) result += 10;
		break;
	case fo::STAT_better_crit:
		if (CheckTrait(fo::TRAIT_heavy_handed)) result -= 30;
		break;
	case fo::STAT_rad_resist:
		if (CheckTrait(fo::TRAIT_fast_metabolism)) return -DudeGetBaseStat(fo::STAT_rad_resist);
		break;
	case fo::STAT_poison_resist:
		if (CheckTrait(fo::TRAIT_fast_metabolism)) return -DudeGetBaseStat(fo::STAT_poison_resist);
		break;
	}
	return result;
}

static void __declspec(naked) trait_adjust_stat_hack() {
	__asm {
		push edx;
		push ecx;
		push eax; // statID
		call Stats::trait_adjust_stat;
		pop  ecx;
		pop  edx;
		retn;
	}
}

void Stats::init() {
	// Replace functions
	sf::MakeJump(fo::funcoffs::trait_adjust_stat_, trait_adjust_stat_hack); // 0x4B3C7C

	// Fix the calculation of the maximum carry weight of the Small Frame trait for bonus strength points
	smallFrameTraitFix = (sf::GetConfigInt("Misc", "SmallFrameFix", 0) > 0);
}

}