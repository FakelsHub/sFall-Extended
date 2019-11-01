/*
 *    sfall
 *    Copyright (C) 2011  Timeslip
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "..\Delegate.h"
#include "Module.h"

namespace sfall
{

class Inventory : public Module {
public:
	const char* name() { return "Inventory"; }
	void init();

	// Called after game calculated dude FID for displaying on inventory screen
	static Delegate<DWORD>& OnAdjustFid();

	static DWORD __stdcall adjust_fid_replacement();

	static long GetInvenApCost();
	static void __fastcall SetInvenApCost(int cost);
};

DWORD _stdcall sf_item_total_size(fo::GameObject* critter);

}
