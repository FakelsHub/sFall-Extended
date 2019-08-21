/*
 *    sfall
 *    Copyright (C) 2008-2016  The sfall team
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

#include "..\..\..\FalloutEngine\Fallout2.h"
#include "..\..\..\SafeWrite.h"
#include "..\..\LoadGameHook.h"
#include "..\..\ScriptExtender.h"
#include "..\..\Worldmap.h"
#include "..\Arrays.h"
#include "..\OpcodeContext.h"

#include "Worldmap.h"

namespace sfall
{
namespace script
{

static DWORD ForceEncounterMapID = -1;
static DWORD ForceEncounterFlags;

DWORD ForceEncounterRestore() {
	long long data = 0x672E043D83; // cmp ds:_Meet_Frank_Horrigan
	SafeWriteBytes(0x4C06D1, (BYTE*)&data, 5);
	ForceEncounterFlags = 0;
	DWORD mapID = ForceEncounterMapID;
	ForceEncounterMapID = -1;
	return mapID;
}

static void __declspec(naked) wmRndEncounterOccurred_hack() {
	__asm {
		test ForceEncounterFlags, 0x1; // _NoCar flag
		jnz  noCar;
		cmp  ds:[FO_VAR_Move_on_Car], 0;
		jz   noCar;
		mov  edx, FO_VAR_carCurrentArea;
		mov  eax, ForceEncounterMapID;
		call fo::funcoffs::wmMatchAreaContainingMapIdx_;
noCar:
		//push ecx;
		// TODO: implement a blinking red icon
		call ForceEncounterRestore;
		//pop  ecx;
		push 0x4C0721; // return addr
		jmp  fo::funcoffs::map_load_idx_; // eax - mapId
	}
}

void sf_force_encounter(OpcodeContext& cxt) {
	if (ForceEncounterFlags & (1 << 31)) return; // wait prev. encounter

	DWORD mapID = cxt.arg(0).rawValue();
	if (mapID < 0) {
		cxt.printOpcodeError("%s() - invalid map number.", cxt.getOpcodeName());
		return;
	}
	if (ForceEncounterMapID == -1) MakeJump(0x4C06D1, wmRndEncounterOccurred_hack);

	ForceEncounterMapID = mapID;
	DWORD flags = 0;
	if (cxt.numArgs() > 1) {
		flags = cxt.arg(1).rawValue();
		if (flags & 2) { // _Lock flag
			flags |= (1 << 31); // set 31-bit
		} else {
			flags &= ~(1 << 31);
		}
	}
	ForceEncounterFlags = flags;
}

// world_map_functions
void __declspec(naked) op_in_world_map() {
	__asm {
		push ecx;
		push edx;
		push eax;
		call InWorldMap;
		mov  edx, eax;
		pop  eax;
		_RET_VAL_INT(ecx);
		pop  edx;
		pop  ecx;
		retn;
	}
}

void __declspec(naked) op_get_game_mode() {
	__asm {
		push ecx;
		push edx;
		push eax;
		call GetLoopFlags;
		mov  edx, eax;
		pop  eax;
		_RET_VAL_INT(ecx);
		pop  edx;
		pop  ecx;
		retn;
	}
}

void __declspec(naked) op_get_world_map_x_pos() {
	__asm {
		push edx;
		push ecx;
		mov  edx, ds:[FO_VAR_world_xpos];
		_RET_VAL_INT(ecx);
		pop  ecx;
		pop  edx;
		retn;
	}
}

void __declspec(naked) op_get_world_map_y_pos() {
	__asm {
		push edx;
		push ecx;
		mov  edx, ds:[FO_VAR_world_ypos];
		_RET_VAL_INT(ecx);
		pop  ecx;
		pop  edx;
		retn;
	}
}

void __declspec(naked) op_set_world_map_pos() {
	__asm {
		push ebx;
		push ecx;
		push edx;
		push edi;
		push esi;
		mov ecx, eax;
		call fo::funcoffs::interpretPopShort_;
		mov esi, eax;
		mov eax, ecx;
		call fo::funcoffs::interpretPopLong_;
		mov edi, eax;
		mov eax, ecx;
		call fo::funcoffs::interpretPopShort_;
		mov edx, eax;
		mov eax, ecx;
		call fo::funcoffs::interpretPopLong_;
		cmp dx, VAR_TYPE_INT;
		jnz end;
		cmp si, VAR_TYPE_INT;
		jnz end;
		mov ds:[FO_VAR_world_xpos], eax;
		mov ds:[FO_VAR_world_ypos], edi;
end:
		pop esi;
		pop edi;
		pop edx;
		pop ecx;
		pop ebx;
		retn;
	}
}

void __declspec(naked) op_set_map_time_multi() {
	__asm {
		push ebx;
		push ecx;
		push edx;
		mov ecx, eax;
		call fo::funcoffs::interpretPopShort_;
		mov edx, eax;
		mov eax, ecx;
		call fo::funcoffs::interpretPopLong_;
		cmp dx, VAR_TYPE_FLOAT;
		jz paramWasFloat;
		cmp dx, VAR_TYPE_INT;
		jnz fail;
		push eax;
		fild dword ptr [esp];
		fstp dword ptr [esp];
		jmp end;
paramWasFloat:
		push eax;
end:
		call SetMapMulti;
fail:
		pop edx;
		pop ecx;
		pop ebx;
		retn;
	}
}

void sf_set_car_intface_art(OpcodeContext& ctx) {
	Worldmap::SetCarInterfaceArt(ctx.arg(0).asInt());
}

void sf_set_map_enter_position(OpcodeContext& ctx) {
	int tile = ctx.arg(0).asInt();
	int elev = ctx.arg(1).asInt();
	int rot = ctx.arg(2).asInt();

	if (tile > -1 && tile < 40000) {
		fo::var::tile = tile;
	}
	if (elev > -1 && elev < 3) {
		fo::var::elevation = elev;
	}
	if (rot > -1 && rot < 6) {
		fo::var::rotation = rot;
	}
}

void sf_get_map_enter_position(OpcodeContext& ctx) {
	DWORD id = TempArray(3, 0);
	arrays[id].val[0].set((long)fo::var::tile);
	arrays[id].val[1].set((long)fo::var::elevation);
	arrays[id].val[2].set((long)fo::var::rotation);
	ctx.setReturn(id, DataType::INT);
}

void sf_set_rest_heal_time(OpcodeContext& ctx) {
	Worldmap::SetRestHealTime(ctx.arg(0).asInt());
}

void sf_set_rest_mode(OpcodeContext& ctx) {
	Worldmap::SetRestMode(ctx.arg(0).asInt());
}

void sf_set_rest_on_map(OpcodeContext& ctx) {
	long mapId = ctx.arg(0).asInt();
	if (mapId < 0) {
		ctx.printOpcodeError("%s() - invalid map number argument.", ctx.getMetaruleName());
		return;
	}
	long elev = ctx.arg(1).asInt();
	if (elev < -1 || elev > 2) {
		ctx.printOpcodeError("%s() - invalid map elevation argument.", ctx.getMetaruleName());
	} else {
		Worldmap::SetRestMapLevel(mapId, elev, ctx.arg(2).asBool());
	}
}

void sf_get_rest_on_map(OpcodeContext& ctx) {
	long elev = ctx.arg(1).asInt();
	if (elev < 0 || elev > 2) {
		ctx.printOpcodeError("%s() - invalid map elevation argument.", ctx.getMetaruleName());
	} else {
		ctx.setReturn(Worldmap::GetRestMapLevel(elev, ctx.arg(0).asInt()));
	}
}

}
}
