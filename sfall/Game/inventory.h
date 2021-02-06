/*
 *    sfall
 *    Copyright (C) 2008 - 2021 Timeslip and sfall team
 *
 */

#pragma once

#include "..\FalloutEngine\Fallout2.h"

namespace game
{

class Inventory
{
public:
	static void init();

	// Custom implementation engine function correctFidForRemovedItem_ with the HOOK_INVENWIELD hook
	static long correctFidForRemovedItem(fo::GameObject* critter, fo::GameObject* item, long flags);

	// The function returns the size of the occupied space for the object or critter
	// - difference from the item_c_curr_size_ function returns the size of equipped items for the critter
	// - does not return the size of nested items
	static DWORD __stdcall item_total_size(fo::GameObject* critter);

	// Reimplementation of adjust_fid engine function
	// Differences from vanilla:
	// - doesn't use art_vault_guy_num as default art, uses current critter FID instead
	// - invokes onAdjustFid delegate that allows to hook into FID calculation
	static DWORD __stdcall adjust_fid();
};

}