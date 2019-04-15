#include "..\main.h"
#include "..\FalloutEngine\Fallout2.h"
#include "LoadGameHook.h"

#include "Objects.h"

namespace sfall
{

static int unjamTimeState;
static int maxCountProto = 512;

long Objects::uniqueID = UniqueID::Start; // saving to sfallgv.sav

// Assigns a new unique identifier to an object if it has not been previously assigned
// the identifier is saved with the object in the saved game and this can used in various script
// player have ID = 18000, all party members have ID = 18000 + its pid (number file of prototype)
long Objects::SetObjectUniqueID(fo::GameObject* obj) {
	long id = obj->id;
	if (id > UniqueID::Start || obj == fo::var::obj_dude || (id >= 18000 && id < 83536)) return id; // 65535 maximum possible number of prototypes

	if ((DWORD)uniqueID >= UniqueID::End) uniqueID = UniqueID::Start;
	obj->id = ++uniqueID;
	return uniqueID;
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

static void __declspec(naked) item_identical_hack() {
	using namespace fo::Fields;
	__asm {
		mov  eax, [edi]; // item id
		cmp  eax, Start; // Unique ID
		jg   notIdentical;
		mov  eax, [esi + scriptId];
		cmp  eax, ebx;
notIdentical:
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
		mov  ecx, maxCountProto;
		cmp  ecx, 4096;
		jae  skip;
		cmp  eax, ecx;
		jb   end;
		add  ecx, 256;
		mov  maxCountProto, ecx;
skip:
		cmp  eax, ecx;
end:
		retn;
	}
}

void Objects::LoadProtoAutoMaxLimit() {
	if (maxCountProto != -1) {
		MakeCall(0x4A21B2, proto_ptr_hack);
	}
}

void Objects::init() {
	LoadGameHook::OnGameReset() += []() {
		RestoreObjUnjamAllLocks();
	};

	int maxlimit = GetConfigInt("Misc", "LoadProtoMaxLimit", -1);
	if (maxlimit != -1) {
		maxCountProto = -1;
		if (maxlimit >= 512) {
			if (maxlimit > 4096) maxlimit = 4096;
			SafeWrite32(0x4A21B3, maxlimit);
		}
	}

	HookCall(0x4A38A5, new_obj_id_hook);
	SafeWrite8(0x4A38B3, 0x90); // fix increment ID

	if (GetConfigInt("Debugging", "PlaceUniqueItemIntoStack", 0) == 0) {
		MakeCall(0x477A0E, item_identical_hack); // don't put to item stack
	}
}

}
