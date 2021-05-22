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

#include "..\main.h"
#include "..\FalloutEngine\Fallout2.h"
#include "..\SimplePatch.h"
#include "..\Translate.h"

#include "LoadGameHook.h"
#include "MainLoopHook.h"

#include "MiscPatches.h"

namespace sfall
{

static char mapName[16]       = {};
static char patchName[33]     = {};
static char versionString[65] = {};
static char messageBuffer[65];

static int* scriptDialog = nullptr;

static const long StatsDisplayTable[14] = {
	fo::Stat::STAT_ac,
	fo::Stat::STAT_dmg_thresh,
	fo::Stat::STAT_dmg_thresh_laser,
	fo::Stat::STAT_dmg_thresh_fire,
	fo::Stat::STAT_dmg_thresh_plasma,
	fo::Stat::STAT_dmg_thresh_explosion,
	fo::Stat::STAT_dmg_thresh_electrical,
	-1,
	fo::Stat::STAT_dmg_resist,
	fo::Stat::STAT_dmg_resist_laser,
	fo::Stat::STAT_dmg_resist_fire,
	fo::Stat::STAT_dmg_resist_plasma,
	fo::Stat::STAT_dmg_resist_explosion,
	fo::Stat::STAT_dmg_resist_electrical
};

static void __declspec(naked) WeaponAnimHook() {
	__asm {
		cmp edx, 11;
		je  c11;
		cmp edx, 15;
		je  c15;
		jmp fo::funcoffs::art_get_code_;
c11:
		mov edx, 16;
		jmp fo::funcoffs::art_get_code_;
c15:
		mov edx, 17;
		jmp fo::funcoffs::art_get_code_;
	}
}

static void __declspec(naked) register_object_take_out_hack() {
	using namespace fo::Fields;
	__asm {
		push ecx;
		push eax;
		mov  ecx, edx;                            // ID1
		mov  edx, [eax + rotation];               // cur_rot
		inc  edx;
		push edx;                                 // ID3
		xor  ebx, ebx;                            // ID2
		mov  edx, [eax + artFid];                 // fid
		and  edx, 0xFFF;                          // Index
		xor  eax, eax;
		inc  eax;                                 // Obj_Type CRITTER
		call fo::funcoffs::art_id_;
		mov  edx, eax;
		xor  ebx, ebx;
		dec  ebx;                                 // delay -1
		pop  eax;                                 // critter
		call fo::funcoffs::register_object_change_fid_;
		pop  ecx;
		xor  eax, eax;
		retn;
	}
}

static void __declspec(naked) gdAddOptionStr_hack() {
	__asm {
		mov  ecx, ds:[FO_VAR_gdNumOptions];
		add  ecx, '1';
		push ecx;
		mov  ecx, 0x4458FA;
		jmp  ecx;
	}
}

static void __declspec(naked) action_use_skill_on_hook_science() {
	using namespace fo;
	__asm {
		cmp esi, ds:[FO_VAR_obj_dude];
		jne end;
		mov eax, KILL_TYPE_robot;
		retn;
end:
		jmp fo::funcoffs::critter_kill_count_type_;
	}
}

static void __declspec(naked) intface_item_reload_hook() {
	__asm {
		push eax;
		mov  eax, dword ptr ds:[FO_VAR_obj_dude];
		call fo::funcoffs::register_clear_;
		xor  edx, edx;       // ANIM_stand
		xor  ebx, ebx;       // delay (unused)
		lea  eax, [edx + 1]; // RB_UNRESERVED
		call fo::funcoffs::register_begin_;
		mov  eax, dword ptr ds:[FO_VAR_obj_dude];
		call fo::funcoffs::register_object_animate_;
		call fo::funcoffs::register_end_;
		pop  eax;
		jmp  fo::funcoffs::gsound_play_sfx_file_;
	}
}

static void __declspec(naked) automap_hack() {
	static const DWORD ScannerHookRet  = 0x41BC1D;
	static const DWORD ScannerHookFail = 0x41BC65;
	using fo::PID_MOTION_SENSOR;
	__asm {
		mov  eax, ds:[FO_VAR_obj_dude];
		mov  edx, PID_MOTION_SENSOR;
		call fo::funcoffs::inven_pid_is_carried_ptr_;
		test eax, eax;
		jz   fail;
		mov  edx, eax;
		jmp  ScannerHookRet;
fail:
		jmp  ScannerHookFail;
	}
}

static bool __fastcall SeeIsFront(fo::GameObject* source, fo::GameObject* target) {
	long dir = source->rotation - fo::func::tile_dir(source->tile, target->tile);
	if (dir < 0) dir = -dir;
	if (dir == 1 || dir == 5) { // peripheral/side vision, reduce the range for seeing through (3x instead of 5x)
		return (fo::func::obj_dist(source, target) <= (fo::func::stat_level(source, fo::STAT_pe) * 3));
	}
	return (dir == 0); // is directly in front
}

static void __declspec(naked) op_obj_can_see_obj_hook() {
	using namespace fo;
	using namespace Fields;
	__asm {
		mov  edi, [esp + 4]; // buf **outObject
		test ebp, ebp; // check only once
		jz   checkSee;
		xor  ebp, ebp; // for only once
		push edx;
		push eax;
		mov  edx, [edi - 8]; // target
		mov  ecx, eax;       // source
		call SeeIsFront;
		xor  ecx, ecx;
		test al, al;
		pop  eax;
		pop  edx;
		jnz  checkSee; // can see
		// vanilla behavior
		push 0x10;
		push edi;
		call fo::funcoffs::make_straight_path_;
		retn 8;
checkSee:
		push eax;                                  // keep source
		push fo::funcoffs::obj_shoot_blocking_at_; // check hex objects func pointer
		push 0x20;                                 // flags, 0x20 = check ShootThru
		push edi;
		call fo::funcoffs::make_straight_path_func_; // overlapping if len(eax) == 0
		pop  ecx;            // source
		mov  edx, [edi - 8]; // target
		mov  ebx, [edi];     // blocking object
		test ebx, ebx;
		jz   isSee;          // no blocking object
		cmp  ebx, edx;
		jne  checkObj;       // object is not equal to target
		retn 8;
isSee:
		mov  [edi], edx;     // fix for target with ShootThru flag
		retn 8;
checkObj:
		mov  eax, [ebx + protoId];
		shr  eax, 24;
		cmp  eax, OBJ_TYPE_CRITTER;
		je   continue; // see through critter
		retn 8;
continue:
		mov  [edi], ecx;                // outObject - ignore source (for cases of overlapping tiles from MultiHex critters)
		mov  [edi - 4], ebx;            // replace source with blocking object
		mov  dword ptr [esp], 0x456BAB; // repeat from the blocking object
		retn 8;
	}
}

static DWORD __fastcall GetWeaponSlotMode(DWORD itemPtr, DWORD mode) {
	int slot = (mode > 0) ? 1 : 0;
	fo::ItemButtonItem* itemButton = &fo::var::itemButtonItems[slot];
	if ((DWORD)itemButton->item == itemPtr) {
		int slotMode = itemButton->mode;
		if (slotMode == 3 || slotMode == 4) {
			mode++;
		}
	}
	return mode;
}

static void __declspec(naked) display_stats_hook() {
	__asm {
		push eax;
		push ecx;
		mov  ecx, ds:[esp + edi + 0xA8 + 0xC]; // get itemPtr
		call GetWeaponSlotMode;                // ecx - itemPtr, edx - mode;
		mov  edx, eax;
		pop  ecx;
		pop  eax;
		jmp  fo::funcoffs::item_w_range_;
	}
}

static void __fastcall SwapHandSlots(fo::GameObject* item, fo::GameObject* &toSlot) {

	if (toSlot && fo::GetItemType(item) != fo::item_type_weapon && fo::GetItemType(toSlot) != fo::item_type_weapon) {
		return;
	}
	fo::ItemButtonItem* leftSlot  = &fo::var::itemButtonItems[0];
	fo::ItemButtonItem* rightSlot = &fo::var::itemButtonItems[1];

	if (toSlot == nullptr) { // copy to slot
		fo::ItemButtonItem* slot;
		fo::ItemButtonItem item[1];
		if ((int)&toSlot == FO_VAR_i_lhand) {
			std::memcpy(item, rightSlot, 0x14);
			item[0].primaryAttack   = fo::AttackType::ATKTYPE_LWEAPON_PRIMARY;
			item[0].secondaryAttack = fo::AttackType::ATKTYPE_LWEAPON_SECONDARY;
			slot = leftSlot; // Rslot > Lslot
		} else {
			std::memcpy(item, leftSlot, 0x14);
			item[0].primaryAttack   = fo::AttackType::ATKTYPE_RWEAPON_PRIMARY;
			item[0].secondaryAttack = fo::AttackType::ATKTYPE_RWEAPON_SECONDARY;
			slot = rightSlot; // Lslot > Rslot;
		}
		std::memcpy(slot, item, 0x14);
	} else { // swap slots
		auto hands = fo::var::itemButtonItems;
		hands[0].primaryAttack   = fo::AttackType::ATKTYPE_RWEAPON_PRIMARY;
		hands[0].secondaryAttack = fo::AttackType::ATKTYPE_RWEAPON_SECONDARY;
		hands[1].primaryAttack   = fo::AttackType::ATKTYPE_LWEAPON_PRIMARY;
		hands[1].secondaryAttack = fo::AttackType::ATKTYPE_LWEAPON_SECONDARY;

		std::memcpy(leftSlot,  &hands[1], 0x14); // Rslot > Lslot
		std::memcpy(rightSlot, &hands[0], 0x14); // Lslot > Rslot
	}
}

static void __declspec(naked) switch_hand_hack() {
	__asm {
		pushfd;
		test ebx, ebx;
		jz   skip;
		cmp  ebx, edx;
		jz   skip;
		push ecx;
		mov  ecx, eax;
		call SwapHandSlots;
		pop  ecx;
skip:
		popfd;
		jz   end;
		retn;
end:
		mov  dword ptr [esp], 0x4715B7;
		retn;
	}
}

static void __declspec(naked) action_use_skill_on_hook() {
	__asm { // eax = dude_obj, edx = target, ebp = party_member
		cmp  eax, edx;
		jnz  end;                     // jump if target != dude_obj
		mov  edx, ebp;
		call fo::funcoffs::obj_dist_; // check distance between dude_obj and party_member
		cmp  eax, 1;                  // if the distance is greater than 1, then reset the register
		jg   skip;
		inc  eax;
		retn;
skip:
		xor  eax, eax;
		retn;
end:
		jmp  fo::funcoffs::obj_dist_;
	}
}

static char electricalMsg[10];

static void __declspec(naked) display_stats_hook_electrical() {
	__asm {
		test edi, edi;
		jz   incNum;
		cmp  edi, 6;
		jz   message;
		jmp  fo::funcoffs::message_search_;
incNum:
		inc  [esp + 0xF0 - 0x20 + 4];           // message number
		inc  [esp + 0xF0 - 0x68 + 4];           // msgfile.number
		jmp  fo::funcoffs::message_search_;
message:
		//mov  ebx, 100;                        // message number from INVENTRY.MSG
		//mov  [esp + 0xF0 - 0x68 + 4], ebx;    // msgfile.number
		//call fo::funcoffs::message_search_;
		//test eax, eax;
		//jnz  skip;
		lea  eax, electricalMsg;
		mov  [esp + 0xF0 - 0x68+0x0C + 4], eax; // msgfile.message
		mov  eax, 1;
//skip:
		retn;
	}
}

static int  displayElectricalStat;
static char hitPointMsg[8];
static const char* hpFmt = "%s %d/%d";

static long __fastcall HealthPointText(fo::GameObject* critter) {

	int maxHP = fo::func::stat_level(critter, fo::STAT_max_hit_points);
	int curHP = fo::func::stat_level(critter, fo::STAT_current_hp);

	if (displayElectricalStat > 1) {
		const char* msg = fo::MessageSearch(&fo::var::inventry_message_file, 7); // default text
		sprintf(messageBuffer, hpFmt, msg, curHP, maxHP);
		return 0;
	} else {
		sprintf(messageBuffer, hpFmt, hitPointMsg, curHP, maxHP);
	}

	int widthText = 0;
	_asm lea  eax, messageBuffer;
	_asm call ds:[FO_VAR_text_width];
	_asm mov  widthText, eax;

	return 145 - widthText; // x position text
}

static long __fastcall CutoffName(const char* name) {
	BYTE j = 0, i = 0;
	do {
		char c = name[i];
		if (!c) break;
		messageBuffer[i] = c;
		if (c == ' ') j = i; // whitespace
	} while (++i < 16);
	if (j && i >= 10) i = j;
	if (i > 10) i = 10;      // max 10 characters
	messageBuffer[i] = '\0';
	return 0;
}

static void __declspec(naked) display_stats_hook_hp() {
	__asm {
		push ecx;
		cmp  displayElectricalStat, 2;
		jge  noName;
		push esi;
		mov  esi, eax;                        // critter
		call fo::funcoffs::critter_name_;
		mov  ecx, eax;
		call CutoffName;
		lea  edx, messageBuffer;              // DisplayText
		mov  al, ds:[FO_VAR_GreenColor];
		mov  ecx, [esp + 4];                  // ToWidth
		push eax;
		mov  eax, edi;                        // ToSurface
		call ds:[FO_VAR_text_to_buf];
		mov  eax, esi;                        // critter
		mov  ebx, 100;                        // TxtWidth
		pop  esi;
noName:
		mov  ecx, eax;                        // critter
		call HealthPointText;
		add  edi, eax;                        // x shift position
		lea  eax, messageBuffer;              // DisplayText
		pop  ecx;                             // ToWidth
		retn;
	}
}

static void __declspec(naked) endgame_movie_hook() {
	__asm {
		cmp  [esp + 16], 0x45C563; // call from op_endgame_movie_
		je   playWalkMovie;
		retn;
playWalkMovie:
		call fo::funcoffs::stat_level_;
		xor  edx, edx;
		add  eax, 10;
		mov  ecx, eax;
		mov  eax, 1500;
		call fo::funcoffs::pause_for_tocks_;
		mov  eax, ecx;
		jmp  fo::funcoffs::gmovie_play_;
	}
}

static void __declspec(naked) ListDrvdStats_hook() {
	static const DWORD ListDrvdStats_Ret = 0x4354D9;
	__asm {
		call fo::IsRadInfluence;
		test eax, eax;
		jnz  influence;
		mov  eax, ds:[FO_VAR_obj_dude];
		jmp  fo::funcoffs::critter_get_rads_;
influence:
		xor  ecx, ecx;
		mov  cl, ds:[FO_VAR_RedColor];
		cmp  dword ptr [esp], 0x4354BE + 5;
		jne  skip;
		mov  cl, 129; // color index for selected
skip:
		add  esp, 4;
		jmp  ListDrvdStats_Ret;
	}
}

static void __declspec(naked) obj_render_outline_hack() {
	__asm {
		test eax, 0xFF00;
		jnz  palColor;
		mov  al, ds:[FO_VAR_GoodColor];
		retn;
palColor:
		mov  al, ah;
		retn;
	}
}

static void __fastcall RemoveAllFloatTextObjects() {
	long textCount = fo::var::text_object_index;
	if (textCount > 0) {
		for (long i = 0; i < textCount; i++)
		{
			fo::func::mem_free(fo::var::text_object_list[i]->unknown10);
			fo::func::mem_free(fo::var::text_object_list[i]);
		}
		fo::var::text_object_index = 0;
	}
}

static void __declspec(naked) obj_move_to_tile_hook() {
	__asm {
		push eax;
		push edx;
		call RemoveAllFloatTextObjects;
		pop  edx;
		pop  eax;
		jmp  fo::funcoffs::map_set_elevation_;
	}
}

static void __declspec(naked) map_check_state_hook() {
	__asm {
		push eax;
		call RemoveAllFloatTextObjects;
		pop  eax;
		jmp  fo::funcoffs::map_load_idx_;
	}
}

static void __declspec(naked) obj_move_to_tile_hook_redraw() {
	__asm {
		mov  MainLoopHook::displayWinUpdateState, 1;
		call fo::funcoffs::tile_set_center_;
		mov  eax, ds:[FO_VAR_display_win];
		jmp  fo::funcoffs::win_draw_; // update black edges after tile_set_center_
	}
}

static void __declspec(naked) map_check_state_hook_redraw() {
	__asm {
		cmp  MainLoopHook::displayWinUpdateState, 0;
		je   obj_move_to_tile_hook_redraw;
		jmp  fo::funcoffs::tile_set_center_;
	}
}

static void AdditionalWeaponAnimsPatch() {
	if (IniReader::GetConfigInt("Misc", "AdditionalWeaponAnims", 0)) {
		dlog("Applying additional weapon animations patch.", DL_INIT);
		SafeWrite8(0x419320, 18); // art_get_code_
		HookCalls(WeaponAnimHook, {
			0x451648, 0x451671, // gsnd_build_character_sfx_name_
			0x4194CC            // art_get_name_
		});
		dlogr(" Done", DL_INIT);
	}
}

static void SkilldexImagesPatch() {
	dlog("Checking for changed skilldex images.", DL_INIT);
	long tmp = IniReader::GetConfigInt("Misc", "Lockpick", 293);
	if (tmp != 293) SafeWrite32(0x518D54, tmp);
	tmp = IniReader::GetConfigInt("Misc", "Steal", 293);
	if (tmp != 293) SafeWrite32(0x518D58, tmp);
	tmp = IniReader::GetConfigInt("Misc", "Traps", 293);
	if (tmp != 293) SafeWrite32(0x518D5C, tmp);
	tmp = IniReader::GetConfigInt("Misc", "FirstAid", 293);
	if (tmp != 293) SafeWrite32(0x518D4C, tmp);
	tmp = IniReader::GetConfigInt("Misc", "Doctor", 293);
	if (tmp != 293) SafeWrite32(0x518D50, tmp);
	tmp = IniReader::GetConfigInt("Misc", "Science", 293);
	if (tmp != 293) SafeWrite32(0x518D60, tmp);
	tmp = IniReader::GetConfigInt("Misc", "Repair", 293);
	if (tmp != 293) SafeWrite32(0x518D64, tmp);
	dlogr(" Done", DL_INIT);
}

static void ScienceOnCrittersPatch() {
	switch (IniReader::GetConfigInt("Misc", "ScienceOnCritters", 0)) {
	case 1:
		HookCall(0x41276E, action_use_skill_on_hook_science);
		break;
	case 2:
		SafeWrite8(0x41276A, CodeType::JumpShort);
		break;
	}
}

static void BoostScriptDialogLimitPatch() {
	const DWORD script_dialog_msgs[] = {
		0x4A50C2, 0x4A5169, 0x4A52FA, 0x4A5302, 0x4A6B86, 0x4A6BE0, 0x4A6C37,
	};

	if (IniReader::GetConfigInt("Misc", "BoostScriptDialogLimit", 0)) {
		const int scriptDialogCount = 10000;
		dlog("Applying script dialog limit patch.", DL_INIT);
		scriptDialog = new int[scriptDialogCount * 2]; // Because the msg structure is 8 bytes, not 4.
		SafeWrite32(0x4A50E3, scriptDialogCount); // scr_init
		SafeWrite32(0x4A519F, scriptDialogCount); // scr_game_init
		SafeWrite32(0x4A534F, scriptDialogCount * 8); // scr_message_free
		SafeWriteBatch<DWORD>((DWORD)scriptDialog, script_dialog_msgs); // scr_get_dialog_msg_file
		dlogr(" Done", DL_INIT);
	}
}

static void NumbersInDialoguePatch() {
	if (IniReader::GetConfigInt("Misc", "NumbersInDialogue", 0)) {
		dlog("Applying numbers in dialogue patch.", DL_INIT);
		SafeWrite32(0x502C32, 0x2000202E);
		SafeWrite8(0x446F3B, 0x35);
		SafeWrite32(0x5029E2, 0x7325202E);
		SafeWrite32(0x446F03, 0x2424448B);        // mov  eax, [esp+0x24]
		SafeWrite8(0x446F07, 0x50);               // push eax
		SafeWrite32(0x446FE0, 0x2824448B);        // mov  eax, [esp+0x28]
		SafeWrite8(0x446FE4, 0x50);               // push eax
		MakeJump(0x4458F5, gdAddOptionStr_hack);
		dlogr(" Done", DL_INIT);
	}
}

static void InstantWeaponEquipPatch() {
	const DWORD PutAwayWeapon[] = {
		0x411EA2, // action_climb_ladder_
		0x412046, // action_use_an_item_on_object_
		0x41224A, // action_get_an_object_
		0x4606A5, // intface_change_fid_animate_
		0x472996, // invenWieldFunc_
	};

	if (IniReader::GetConfigInt("Misc", "InstantWeaponEquip", 0)) {
		//Skip weapon equip/unequip animations
		dlog("Applying instant weapon equip patch.", DL_INIT);
		SafeWriteBatch<BYTE>(CodeType::JumpShort, PutAwayWeapon); // jmps
		BlockCall(0x472AD5); //
		BlockCall(0x472AE0); // invenUnwieldFunc_
		BlockCall(0x472AF0); //
		MakeJump(0x415238, register_object_take_out_hack);
		dlogr(" Done", DL_INIT);
	}
}

static void DontTurnOffSneakIfYouRunPatch() {
	if (IniReader::GetConfigInt("Misc", "DontTurnOffSneakIfYouRun", 0)) {
		dlog("Applying DontTurnOffSneakIfYouRun patch.", DL_INIT);
		SafeWrite8(0x418135, CodeType::JumpShort);
		dlogr(" Done", DL_INIT);
	}
}

static void PlayIdleAnimOnReloadPatch() {
	if (IniReader::GetConfigInt("Misc", "PlayIdleAnimOnReload", 0)) {
		dlog("Applying idle anim on reload patch.", DL_INIT);
		HookCall(0x460B8C, intface_item_reload_hook);
		dlogr(" Done", DL_INIT);
	}
}

static void MotionScannerFlagsPatch() {
	if (long flags = IniReader::GetConfigInt("Misc", "MotionScannerFlags", 1)) {
		dlog("Applying MotionScannerFlags patch.", DL_INIT);
		if (flags & 1) MakeJump(0x41BBE9, automap_hack);
		if (flags & 2) {
			// automap_
			SafeWrite16(0x41BC24, 0x9090);
			BlockCall(0x41BC3C);
			// item_m_use_charged_item_
			SafeWrite8(0x4794B3, 0x5E); // jbe short 0x479512
		}
		dlogr(" Done", DL_INIT);
	}
}

static void __declspec(naked) ResizeEncounterMessagesTable() {
	__asm {
		add  eax, eax; // double the increment
		add  eax, 3000;
		retn;
	}
}

static void EncounterTableSizePatch() {
	const DWORD EncounterTableSize[] = {
		0x4BD1A3, 0x4BD1D9, 0x4BD270, 0x4BD604, 0x4BDA14, 0x4BDA44, 0x4BE707,
		0x4C0815, 0x4C0D4A, 0x4C0FD4,
	};

	int tableSize = IniReader::GetConfigInt("Misc", "EncounterTableSize", 0);
	if (tableSize > 40) {
		dlog("Applying EncounterTableSize patch.", DL_INIT);
		if (tableSize > 50) {
			if (tableSize > 100) tableSize = 100;
			// Increase the count of message lines from 50 to 100 for the encounter tables in worldmap.msg
			MakeCalls(ResizeEncounterMessagesTable, { 0x4C102C, 0x4C0B57 });
		}
		SafeWrite8(0x4BDB17, (BYTE)tableSize);
		DWORD nsize = (tableSize + 1) * 180 + 0x50;
		SafeWriteBatch<DWORD>(nsize, EncounterTableSize);
		dlogr(" Done", DL_INIT);
	}
}

static void DisablePipboyAlarmPatch() {
	if (IniReader::GetConfigInt("Misc", "DisablePipboyAlarm", 0)) {
		dlog("Applying Disable Pip-Boy alarm button patch.", DL_INIT);
		SafeWrite8(0x499518, CodeType::Ret);
		SafeWrite8(0x443601, 0x0);
		dlogr(" Done", DL_INIT);
	}
}

static void ObjCanSeeShootThroughPatch() {
	if (IniReader::GetConfigInt("Misc", "ObjCanSeeObj_ShootThru_Fix", 0)) {
		dlog("Applying obj_can_see_obj fix for seeing through critters and ShootThru objects.", DL_INIT);
		HookCall(0x456BC6, op_obj_can_see_obj_hook);
		dlogr(" Done", DL_INIT);
	}
}

static void OverrideMusicDirPatch() {
	static const char* musicOverridePath = "data\\sound\\music\\";

	if (long overrideMode = IniReader::GetConfigInt("Sound", "OverrideMusicDir", 0)) {
		SafeWriteBatch<DWORD>((DWORD)musicOverridePath, {0x4449C2, 0x4449DB}); // set paths if not present in the cfg
		if (overrideMode == 2) {
			SafeWriteBatch<DWORD>((DWORD)musicOverridePath, {0x518E78, 0x518E7C});
			SafeWrite16(0x44FCF3, 0x40EB); // jmp 0x44FD35 (skip paths initialization)
		}
	}
}

static void DialogueFix() {
	if (IniReader::GetConfigInt("Misc", "DialogueFix", 1)) {
		dlog("Applying dialogue patch.", DL_INIT);
		SafeWrite8(0x446848, 0x31);
		dlogr(" Done", DL_INIT);
	}
}

static void InterfaceWindowPatch() {
	if (IniReader::GetConfigInt("Interface", "RemoveWindowRounding", 1)) {
		SafeWriteBatch<BYTE>(CodeType::JumpShort, {0x4D6EDD, 0x4D6F12});
	}

	dlog("Applying flags patch for interfaces", DL_INIT);

	// Remove MoveOnTop flag for interfaces
	SafeWrite8(0x46ECE9, (*(BYTE*)0x46ECE9) ^ fo::WinFlags::MoveOnTop); // Player Inventory/Loot/UseOn
	SafeWrite8(0x41B966, (*(BYTE*)0x41B966) ^ fo::WinFlags::MoveOnTop); // Automap

	// Set OwnerFlag flag
	SafeWrite8(0x4D5EBF, fo::WinFlags::OwnerFlag); // win_init_ (main win)
	SafeWrite8(0x481CEC, (*(BYTE*)0x481CEC) | fo::WinFlags::OwnerFlag); // _display_win (map win)
	SafeWrite8(0x44E7D2, (*(BYTE*)0x44E7D2) | fo::WinFlags::OwnerFlag); // gmovie_play_ (movie win)

	// Remove OwnerFlag flag
	SafeWrite8(0x4B801B, (*(BYTE*)0x4B801B) ^ fo::WinFlags::OwnerFlag); // createWindow_
	// Remove OwnerFlag and Transparent flags
	SafeWrite8(0x42F869, (*(BYTE*)0x42F869) ^ (fo::WinFlags::Transparent | fo::WinFlags::OwnerFlag)); // addWindow_

	dlogr(" Done", DL_INIT);

	// Disables unused code for the RandX and RandY window structure fields (these fields can now be used for other purposes as well)
	SafeWrite32(0x4D630C, 0x9090C031); // xor eax,eax
	SafeWrite8(0x4D6310, 0x90);
	BlockCall(0x4D6319);
}

static void InventoryCharacterRotationSpeedPatch() {
	long setting = IniReader::GetConfigInt("Misc", "SpeedInventoryPCRotation", 166);
	if (setting != 166 && setting <= 1000) {
		dlog("Applying SpeedInventoryPCRotation patch.", DL_INIT);
		SafeWrite32(0x47066B, setting);
		dlogr(" Done", DL_INIT);
	}
}

static void UIAnimationSpeedPatch() {
	DWORD addrs[] = {
		0x45F9DE, 0x45FB33,
		0x447DF4, 0x447EB6,
		0x499B99, 0x499DA8
	};
	SimplePatch<WORD>(addrs, 2, "Misc", "CombatPanelAnimDelay", 1000, 0, 65535);
	SimplePatch<BYTE>(&addrs[2], 2, "Misc", "DialogPanelAnimDelay", 33, 0, 255);
	SimplePatch<BYTE>(&addrs[4], 2, "Misc", "PipboyTimeAnimDelay", 50, 0, 127);
}

static void MusicInDialoguePatch() {
	if (IniReader::GetConfigInt("Misc", "EnableMusicInDialogue", 0)) {
		dlog("Applying music in dialogue patch.", DL_INIT);
		SafeWrite8(0x44525B, 0);
		//BlockCall(0x450627);
		dlogr(" Done", DL_INIT);
	}
}

static void PipboyAvailableAtStartPatch() {
	switch (IniReader::GetConfigInt("Misc", "PipBoyAvailableAtGameStart", 0)) {
	case 1:
		LoadGameHook::OnBeforeGameStart() += []() {
			fo::var::gmovie_played_list[3] = true; // PipBoy aquiring video
		};
		break;
	case 2:
		SafeWrite8(0x497011, CodeType::JumpShort); // skip the vault suit movie check
		break;
	}
}

static void DisableHorriganPatch() {
	if (IniReader::GetConfigInt("Misc", "DisableHorrigan", 0)) {
		LoadGameHook::OnAfterGameStarted() += []() {
			fo::var::Meet_Frank_Horrigan = true;
		};
	}
}

static void DisplaySecondWeaponRangePatch() {
	// Display the range of the second attack mode in the inventory when you switch weapon modes in active item slots
	//if (IniReader::GetConfigInt("Misc", "DisplaySecondWeaponRange", 1)) {
		dlog("Applying display second weapon range patch.", DL_INIT);
		HookCall(0x472201, display_stats_hook);
		dlogr(" Done", DL_INIT);
	//}
}

static void DisplayElectricalStatPatch() {
	displayElectricalStat = IniReader::GetConfigInt("Misc", "DisplayElectricalResist", 0);
	if (displayElectricalStat) {
		dlog("Applying display of the electrical resist stat of armor patch.", DL_INIT);
		Translate::Get("sfall", "DisplayStatHP", "HP:", hitPointMsg, 8); // short variant
		Translate::Get("sfall", "DisplayStatEL", " Electr.", electricalMsg, 10);
		HookCall(0x471E55, display_stats_hook_hp);
		HookCall(0x471FA7, display_stats_hook_electrical);
		SafeWrite32(0x471D72, (DWORD)&StatsDisplayTable);
		SafeWrite32(0x471D8B, (DWORD)&StatsDisplayTable[7]);
		dlogr(" Done", DL_INIT);
	}
}

static void KeepWeaponSelectModePatch() {
	//if (IniReader::GetConfigInt("Misc", "KeepWeaponSelectMode", 1)) {
		dlog("Applying keep weapon select mode patch.", DL_INIT);
		MakeCall(0x4714EC, switch_hand_hack, 1);
		dlogr(" Done", DL_INIT);
	//}
}

static void PartyMemberSkillPatch() {
	// Fixed getting distance from source to target when using skills
	// Note: this will cause the party member to apply his/her skill when you use First Aid/Doctor skill on the player, but only if
	// the player is standing next to the party member. Because the related engine function is not fully implemented, enabling
	// this option without a global script that overrides First Aid/Doctor functions has very limited usefulness
	if (IniReader::GetConfigInt("Misc", "PartyMemberSkillFix", 0) != 0) {
		dlog("Applying party member using First Aid/Doctor skill patch.", DL_INIT);
		HookCall(0x412836, action_use_skill_on_hook);
		dlogr(" Done", DL_INIT);
	}
	// Small code patch for HOOK_USESKILLON (change obj_dude to source)
	SafeWrite32(0x4128F3, 0x90909090);
	SafeWrite16(0x4128F7, 0xFE39); // cmp esi, _obj_dude -> cmp esi, edi
}

#pragma pack(push, 1)
struct CodeData {
	DWORD dd = 0x0024548D;
	BYTE  db = 0x90;
} patchData;
#pragma pack(pop)

static void SkipLoadingGameSettingsPatch() {
	int skipLoading = IniReader::GetConfigInt("Misc", "SkipLoadingGameSettings", -1);
	if (skipLoading) {
		dlog("Applying skip loading game settings from a saved game patch.", DL_INIT);
		BlockCall(0x493421);
		SafeWrite8(0x4935A8, 0x1F);
		SafeWrite32(0x4935AB, 0x90901B75);

		if (skipLoading == 2) SafeWriteBatch<CodeData>(patchData, {0x49341C, 0x49343B});
		SafeWriteBatch<CodeData>(patchData, {
			0x493450, 0x493465, 0x49347A, 0x49348F, 0x4934A4, 0x4934B9, 0x4934CE,
			0x4934E3, 0x4934F8, 0x49350D, 0x493522, 0x493547, 0x493558, 0x493569,
			0x49357A
		});
		dlogr(" Done", DL_INIT);
	}
}

static void UseWalkDistancePatch() {
	int distance = IniReader::GetConfigInt("Misc", "UseWalkDistance", 3) + 2;
	if (distance > 1 && distance < 5) {
		dlog("Applying walk distance for using objects patch.", DL_INIT);
		SafeWriteBatch<BYTE>(distance, {0x411FF0, 0x4121C4, 0x412475, 0x412906}); // default is 5
		dlogr(" Done", DL_INIT);
	}
}

static void F1EngineBehaviorPatch() {
	if (IniReader::GetConfigInt("Misc", "Fallout1Behavior", 0)) {
		dlog("Applying Fallout 1 engine behavior patch.", DL_INIT);
		BlockCall(0x4A4343); // disable playing the final movie/credits after the endgame slideshow
		SafeWrite8(0x477C71, CodeType::JumpShort); // disable halving the weight for power armor items
		HookCall(0x43F872, endgame_movie_hook); // play movie 10 or 11 based on the player's gender before the credits
		dlogr(" Done", DL_INIT);
	}
}

static void __declspec(naked) op_display_msg_hook() {
	__asm {
		cmp  dword ptr ds:FO_VAR_debug_func, 0;
		jne  debug;
		retn;
debug:
		jmp  fo::funcoffs::config_get_value_;
	}
}

static void EngineOptimizationPatches() {
	// Speed up display_msg script function
	HookCall(0x455404, op_display_msg_hook);

	// Remove redundant/duplicate code
	BlockCall(0x45EBBF); // intface_redraw_
	BlockCall(0x4A4859); // exec_script_proc_
	SafeMemSet(0x455189, CodeType::Nop, 11); // op_create_object_sid_

	// Improve performance of the data conversion of script interpreter
	// mov eax, [edx+eax]; bswap eax; ret;
	SafeWrite32(0x4672A4, 0x0F02048B);
	SafeWrite16(0x4672A8, 0xC3C8);
	// mov eax, [edx+eax]; bswap eax;
	SafeWrite32(0x4673E5, 0x0F02048B);
	SafeWrite8(0x4673E9, 0xC8);
	// mov ax, [eax]; rol ax, 8; ret;
	SafeWrite32(0x467292, 0x66008B66);
	SafeWrite32(0x467296, 0xC308C0C1);
}

void MiscPatches::init() {

	EngineOptimizationPatches();

	if (IniReader::GetConfigString("Misc", "StartingMap", "", mapName, 16)) {
		dlog("Applying starting map patch.", DL_INIT);
		SafeWrite32(0x480AAA, (DWORD)&mapName);
		dlogr(" Done", DL_INIT);
	}

	if (IniReader::GetConfigString("Misc", "VersionString", "", versionString, 65)) {
		dlog("Applying version string patch.", DL_INIT);
		SafeWrite32(0x4B4588, (DWORD)&versionString);
		dlogr(" Done", DL_INIT);
	}

	if (IniReader::GetConfigString("Misc", "PatchFile", "", patchName, 33)) {
		dlog("Applying patch file patch.", DL_INIT);
		SafeWrite32(0x444323, (DWORD)&patchName);
		dlogr(" Done", DL_INIT);
	}

	if (IniReader::GetConfigInt("System", "SingleCore", 1)) {
		dlog("Applying single core patch.", DL_INIT);
		HANDLE process = GetCurrentProcess();
		SetProcessAffinityMask(process, 1);
		CloseHandle(process);
		dlogr(" Done", DL_INIT);
	}

	if (IniReader::GetConfigInt("System", "OverrideArtCacheSize", 0)) {
		dlog("Applying override art cache size patch.", DL_INIT);
		SafeWrite32(0x418867, 0x90909090);
		SafeWrite32(0x418872, 256);
		dlogr(" Done", DL_INIT);
	}

	int time = IniReader::GetConfigInt("Misc", "CorpseDeleteTime", 6); // time in days
	if (time != 6) {
		dlog("Applying corpse deletion time patch.", DL_INIT);
		if (time <= 0) {
			time = 12; // hours
		} else if (time > 13) {
			time = 13 * 24;
		} else {
			time *= 24;
		}
		SafeWrite32(0x483348, time);
		dlogr(" Done", DL_INIT);
	}

	// Set idle function
	fo::var::idle_func = reinterpret_cast<void*>(Sleep);
	SafeWrite16(0x4C9F12, 0x7D6A); // push 125 (ms)

	SafeWrite8(0x4810AB, CodeType::JumpShort); // Disable selfrun

	BlockCall(0x4425E6); // Patch out ereg call

	SimplePatch<DWORD>(0x440C2A, "Misc", "SpecialDeathGVAR", fo::GVAR_MODOC_SHITTY_DEATH);

	// Remove hardcoding for maps with IDs 19 and 37
	if (IniReader::GetConfigInt("Misc", "DisableSpecialMapIDs", 0)) {
		dlog("Applying disable handling special maps patch.", DL_INIT);
		SafeWriteBatch<BYTE>(0, {0x4836D6, 0x4836DB});
		dlogr(" Done", DL_INIT);
	}

	// Remove hardcoding for city areas 45 and 46 (AREA_FAKE_VAULT_13)
	if (IniReader::GetConfigInt("Misc", "DisableSpecialAreas", 0)) {
		dlog("Applying disable handling special areas patch.", DL_INIT);
		SafeWrite8(0x4C0576, CodeType::JumpShort);
		dlogr(" Done", DL_INIT);
	}

	// Highlight "Radiated" in red color when the player is under the influence of negative effects of radiation
	HookCalls(ListDrvdStats_hook, { 0x43549C, 0x4354BE });

	// Increase the max text width of the information card on the character screen
	SafeWriteBatch<BYTE>(150, {0x43ACD5, 0x43DD37}); // 136, 133

	// Increase the max text width of the player name on the character screen
	SafeWriteBatch<BYTE>(127, {0x435160, 0x435189}); // 100

	// Allow setting custom colors from the game palette for object outlines
	MakeCall(0x48EE00, obj_render_outline_hack);

	// Remove floating text messages after moving to another map or elevation
	HookCall(0x48A94B, obj_move_to_tile_hook);
	HookCall(0x4836BB, map_check_state_hook);

	// Redraw the screen to update black edges of the map (HRP bug)
	// https://github.com/phobos2077/sfall/issues/282
	HookCall(0x48A954, obj_move_to_tile_hook_redraw);
	HookCall(0x483726, map_check_state_hook_redraw);

	F1EngineBehaviorPatch();
	DialogueFix();
	AdditionalWeaponAnimsPatch();
	PlayIdleAnimOnReloadPatch();

	SkilldexImagesPatch();
	InterfaceWindowPatch();

	ScienceOnCrittersPatch();
	InventoryCharacterRotationSpeedPatch();

	OverrideMusicDirPatch();
	BoostScriptDialogLimitPatch();
	MotionScannerFlagsPatch();
	EncounterTableSizePatch();

	DisablePipboyAlarmPatch();

	ObjCanSeeShootThroughPatch();
	UIAnimationSpeedPatch();
	MusicInDialoguePatch();
	DontTurnOffSneakIfYouRunPatch();

	InstantWeaponEquipPatch();
	NumbersInDialoguePatch();
	PipboyAvailableAtStartPatch();
	DisableHorriganPatch();

	DisplaySecondWeaponRangePatch();
	DisplayElectricalStatPatch();
	KeepWeaponSelectModePatch();

	PartyMemberSkillPatch();

	SkipLoadingGameSettingsPatch();

	UseWalkDistancePatch();
}

void MiscPatches::exit() {
	if (scriptDialog) delete[] scriptDialog;
}

}
