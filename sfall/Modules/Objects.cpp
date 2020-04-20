/*
 *    sfall
 *    Copyright (C) 2008-2018  The sfall team
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

#include "..\main.h"
#include "..\FalloutEngine\Fallout2.h"
#include "LoadGameHook.h"

#include "Objects.h"

namespace sfall
{

static int unjamTimeState;
static int maxCountLoadProto = 512;

char Objects::sfallProcessSeenState = 0; // set 1-bit: allowed, 2-bit: run process

long Objects::uniqueID = UniqueID::Start; // current counter id, saving to sfallgv.sav

// Assigns a new unique identifier to an object if it has not been previously assigned
// the identifier is saved with the object in the saved game and this can used in various script
// player ID = 18000, all party members have ID = 18000 + its pid (file number of prototype)
long __fastcall Objects::SetObjectUniqueID(fo::GameObject* obj) {
	long id = obj->id;
	if (id > UniqueID::Start || (id >= fo::PLAYER_ID && id < 83536)) return id; // 65535 maximum possible number of prototypes

	if ((DWORD)uniqueID >= (DWORD)UniqueID::End) uniqueID = UniqueID::Start;
	obj->id = ++uniqueID;
	return uniqueID;
}

// Assigns a unique ID in the negative range (0xFFFFFFF6 - 0x8FFFFFF7)
long __fastcall Objects::SetSpecialID(fo::GameObject* obj) {
	long id = obj->id;
	if (id <= -10 || id > UniqueID::Start) return id;

	if ((DWORD)uniqueID >= (DWORD)UniqueID::End) uniqueID = UniqueID::Start;
	id = -9 - (++uniqueID - UniqueID::Start);
	obj->id = id;
	return id;
}

void Objects::SetNewEngineID(fo::GameObject* obj) {
	if (obj->id > UniqueID::Start) return;
	obj->id = fo::func::new_obj_id();
}

static void __declspec(naked) item_identical_hack() {
	using namespace fo::Fields;
	__asm {
		mov  ecx, [edi]; // item id
		cmp  ecx, Start; // start unique ID
		jg   notIdentical;
		mov  eax, [esi + scriptId];
		cmp  eax, ebx;
notIdentical:
		retn; // if ZF == 0 then item is not identical
	}
}

static void __declspec(naked) new_obj_id_hook() {
	__asm {
		mov  eax, 83535;
		cmp  dword ptr ds:[FO_VAR_cur_id], eax;
		jle  pickNewID;
		retn;
pickNewID: // skip PM range (18000 - 83535)
		mov  ds:[FO_VAR_cur_id], eax;
		jmp  fo::funcoffs::new_obj_id_;
	}
}

// Reassigns object IDs to all critters upon first loading a map
static void map_fix_critter_id() {
	long npcStartID = 4096;
	fo::GameObject* obj = fo::func::obj_find_first();
	while (obj) {
		if (obj->Type() == fo::OBJ_TYPE_CRITTER && obj->id < fo::PLAYER_ID) {
			obj->id = npcStartID++;
		}
		obj = fo::func::obj_find_next();
	}
}

static void __declspec(naked) map_load_file_hack() {
	__asm {
		jz   mapVirgin;
		retn;
mapVirgin:
		test eax, eax;
		jl   skip; // check map index > -1
		call fo::funcoffs::wmMapIsSaveable_;
		test eax, eax;
		jnz  saveable;
		retn;
saveable:
		call map_fix_critter_id;
skip:
		xor  eax, eax; // set ZF
		retn;
	}
}

static void __declspec(naked) queue_add_hack() {
	using namespace fo;
	using namespace Fields;
	__asm {
		// engine code
		mov  [edx + 8], edi; // queue.object
		mov  [edx], esi;     // queue.time
		//---
		cmp  ds:[FO_VAR_loadingGame], 1; // don't change the object ID when loading a saved game (e.g. fix: NPC turns into a container)
		je   skip;
		test edi, edi;
		jnz  fix;
skip:
		retn;
fix:
		mov  eax, [edi + protoId];
		and  eax, 0x0F000000;
		jnz  notItem; // object is not an item?
		push ecx;
		push edx;
		mov  ecx, edi;
		call Objects::SetSpecialID;
		pop  edx;
		pop  ecx;
		retn;
notItem:
		cmp  ecx, script_timer_event; // QueueType
		je   end;
		cmp  eax, OBJ_TYPE_CRITTER << 24;
		jne  end;
		push ecx;
		push edx;
		mov  ecx, edi;
		call Objects::SetObjectUniqueID;
		pop  edx;
		pop  ecx;
end:
		xor  edi, edi; // fix: don't set "Used" flag for non-item objects
		retn;
	}
}

void Objects::SetAutoUnjamLockTime(DWORD time) {
	if (!unjamTimeState) BlockCall(0x4A364A); // disable auto unjam at midnight

	if (time > 0) {
		SafeWrite8(0x4831D9, (BYTE)time);
		if (unjamTimeState == 2) {
			SafeWrite8(0x4831DA, 0x7C);
		}
		unjamTimeState = 1;
	} else {
		SafeWrite8(0x4831DA, 0xEB); // disable auto unjam
		unjamTimeState = 2;
	}
}

void RestoreObjUnjamAllLocks() {
	if (unjamTimeState) {
		SafeWrite8(0x4A364A, 0xE8);
		SafeWrite32(0x4A364B, 0xFFFF9E69);
		SafeWrite8(0x4831DA, 0x7C);
		SafeWrite8(0x4831D9, 24);
		unjamTimeState = 0;
	}
}

static void __declspec(naked) proto_ptr_hack() {
	__asm {
		mov  ecx, maxCountLoadProto;
		cmp  ecx, 4096;
		jae  skip;
		cmp  eax, ecx;
		jb   end;
		add  ecx, 256;
		mov  maxCountLoadProto, ecx;
skip:
		cmp  eax, ecx;
end:
		retn;
	}
}

void Objects::LoadProtoAutoMaxLimit() {
	MakeCall(0x4A21B2, proto_ptr_hack);
}

// Places the PID_CORPSE_BLOOD object to the lower layer of objects on the tile
static void __declspec(naked) obj_insert_hack() {
	using namespace fo;
	using namespace Fields;
	__asm {
		// engine code
		mov  edi, [ebx]; // tableNodes.objectPtr
		mov  [esp + 0x38 - 0x1C + 4], esi; // 0
		test edi, edi;   // objectPtr
		jnz  insert;
		retn;
insert: //----------
		mov  esi, [ecx]; // esi - inserted object
		mov  edi, [edi]; // object placed on tile
		xor  edx, edx;
		cmp  dword ptr [esi + protoId], PID_CORPSE_BLOOD;
		je   fix;
		cmp  dword ptr [edi + protoId], PID_CORPSE_BLOOD;
		jne  skip;
		sete dl; // set to 1 if PID_CORPSE_BLOOD already located on the map
fix:
		mov  esi, [esi + elevation];
		cmp  [edi + elevation], esi;
		jne  skip;
		xor  edi, edi;
		test dl, dl;
		jz   skip;
		mov  ebx, [ebx]; // tableNodes.objectPtr
		add  ebx, 4;     // tableNodes.nextObjectPtr
		mov  edi, [ebx]; // tableNodes.objectPtr
skip:
		retn;
	}
}

// Alternative implementation of the engine function obj_process_seen_ (with the "Line of Sight" function)
void __fastcall Objects::sf_obj_process_seen(long tileIndex) {
	long sIndex = tileIndex - 400;
	if (sIndex < 0) sIndex = 0;
	long eIndex = tileIndex + 400;
	if (eIndex > 5000) eIndex = 5000;

	// clear seen check region
	long s_seen = FO_VAR_obj_seen_check + sIndex;
	long e_seen = FO_VAR_obj_seen_check + eIndex;
	size_t size = e_seen - s_seen;
	std::memset((void*)s_seen, 0, size);

	for (long index = sIndex; index <= eIndex; index += 25) {
		if (index >= 0 && index < 5001) {
			((char*)FO_VAR_obj_seen_check)[index] = -1;
			if (index >= 1) {
				((char*)FO_VAR_obj_seen_check)[index - 1] = -1;
				if (index != 1) {
					((char*)FO_VAR_obj_seen_check)[index - 2] = -1;
					if (index <= 4999) {
						((char*)FO_VAR_obj_seen_check)[index + 1] = -1;
						if (index != 4999) ((char*)FO_VAR_obj_seen_check)[index + 2] = -1;
					}
				}
			}
		}
	}
	long tileTable = (sIndex > 0) ? sIndex * 8 : 0;
	do {
		if (((char*)FO_VAR_obj_seen_check)[sIndex]) {
			long dist = 0;
			do {
				long tile = tileTable + dist;
				if (tile >= 40000) return;
				DWORD* objptr = fo::var::objectTable[tile];
				while (objptr)
				{
					fo::GameObject* obj = (fo::GameObject*)*objptr;
					if (obj->elevation == fo::var::obj_dude->elevation) {
						fo::GameObject* object = nullptr;
						// check the line of sight from obj_dude to tile object, only critters and items
						if (obj->Type() <= fo::ObjType::OBJ_TYPE_CRITTER) object = fo::LineOfSight(obj);
						if (object == nullptr) obj->flags |= fo::ObjectFlag::Seen;
					}
					objptr = (DWORD*)*++objptr;
				}
			} while (++dist < 8);
		}
		tileTable += 8;
	} while (++sIndex <= eIndex);
}

static void __declspec(naked) obj_process_seen_hack() {
	using namespace fo;
	using namespace ObjectFlag;
	using namespace Fields;
	__asm {
		jnz  end; // obj.elev != dude.elev
		mov  ebx, edx;
		mov  edx, [edx + protoId];
		shr  edx, 24;
		and  dl,  0x0F;
		cmp  dl,  OBJ_TYPE_CRITTER;
		ja   skip;
		push ecx;
		push eax;
		mov  ecx, ebx;
		call LineOfSight;
		test eax, eax;
		pop  eax;
		pop  ecx;
		jnz  end;
skip:
		or   [ebx + flags + 3], Seen >> 24;
end:
		retn;
	}
}

static void __declspec(naked) map_place_dude_and_mouse_hook() {
	__asm {
		cmp dword ptr [esp + 0x0C + 4], 0x483082;
		jnz skip;
		or  Objects::sfallProcessSeenState, 2; // set 2-bit
skip:
		jmp fo::funcoffs::dude_stand_;
	}
}

void Objects::init() {
	LoadGameHook::OnGameReset() += []() {
		RestoreObjUnjamAllLocks();
		sfallProcessSeenState = 0;
	};

	LoadGameHook::OnBeforeGameStart() += []() {
		if (hrpIsEnabled) {
			if (hrpVersionValid) {
				bool hrpFogIsEnabled = *(DWORD*)HRPAddress(0x100683D4) != 0;
				sfallProcessSeenState = !hrpFogIsEnabled;
			}
		} else {
			sfallProcessSeenState = 1;
		}
	};

	HookCall(0x4A38A5, new_obj_id_hook);
	SafeWrite8(0x4A38B3, 0x90); // fix ID increment

	MakeCall(0x477A0E, item_identical_hack); // don't put item with unique ID to items stack

	// Fix mapper bug by reassigning object IDs to critters (for unvisited maps)
	MakeCall(0x482E6B, map_load_file_hack);
	SafeWrite8(0x482E71, 0x85); // jz > jnz
	// Additionally fix object IDs for queued events
	MakeCall(0x4A25BA, queue_add_hack);

	// Place some objects on the tile to the lower z-layer
	MakeCall(0x48D918, obj_insert_hack, 1);

	// Adds the "Line of Sight" function for items and critters object
	MakeCall(0x48C889, obj_process_seen_hack, 1);
	if (hrpIsEnabled == false || hrpVersionValid) {
		HookCall(0x4841D6, map_place_dude_and_mouse_hook); // enable seen process
	}
}

}
