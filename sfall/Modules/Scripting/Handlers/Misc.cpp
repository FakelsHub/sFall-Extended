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

#include <cstring>

#include "..\..\..\FalloutEngine\AsmMacros.h"
#include "..\..\..\FalloutEngine\Fallout2.h"

#include "..\..\..\Utils.h"
#include "..\..\Criticals.h"
#include "..\..\HeroAppearance.h"
//#include "..\..\MiscPatches.h"
#include "..\..\Movies.h"
#include "..\..\PartyControl.h"
#include "..\..\PlayerModel.h"
#include "..\..\ScriptExtender.h"
#include "..\..\Sound.h"
#include "..\..\Stats.h"

#include "..\Arrays.h"

#include "Misc.h"

namespace sfall
{
namespace script
{

const char* stringTooLong = "%s() - the string exceeds maximum length of 64 characters.";

void __declspec(naked) op_stop_game() {
	__asm {
		jmp fo::funcoffs::map_disable_bk_processes_;
	}
}

void __declspec(naked) op_resume_game() {
	__asm {
		jmp fo::funcoffs::map_enable_bk_processes_;
	}
}

void op_set_dm_model(OpcodeContext& ctx) {
	auto model = ctx.arg(0).strValue();
	if (strlen(model) > 64) {
		ctx.printOpcodeError(stringTooLong, ctx.getOpcodeName());
		return;
	}
	strcpy(defaultMaleModelName, model);
}

void op_set_df_model(OpcodeContext& ctx) {
	auto model = ctx.arg(0).strValue();
	if (strlen(model) > 64) {
		ctx.printOpcodeError(stringTooLong, ctx.getOpcodeName());
		return;
	}
	strcpy(defaultFemaleModelName, model);
}

void op_set_movie_path(OpcodeContext& ctx) {
	long movieID = ctx.arg(1).rawValue();
	if (movieID < 0 || movieID >= MaxMovies) return;
	auto fileName = ctx.arg(0).strValue();
	if (strlen(fileName) > 64) {
		ctx.printOpcodeError(stringTooLong, ctx.getOpcodeName());
		return;
	}
	strcpy(&MoviePaths[movieID * 65], fileName);
}

void op_get_year(OpcodeContext& ctx) {
	int year = 0;
	__asm {
		xor eax, eax;
		xor edx, edx;
		lea ebx, year;
		call fo::funcoffs::game_time_date_;
	}
	ctx.setReturn(year);
}

void __declspec(naked) op_eax_available() {
	__asm {
		xor  edx, edx
		_J_RET_VAL_TYPE(VAR_TYPE_INT);
//		retn;
	}
}

static const char* nameNPCToInc;
static long pidNPCToInc;
static bool onceNpcLoop;

static void __cdecl IncNPCLevel(const char* fmt, const char* name) {
	fo::GameObject* mObj;
	__asm {
		push edx;
		mov  eax, [ebp + 0x150 - 0x1C + 16]; // ebp <- esp
		mov  edx, [eax];
		mov  mObj, edx;
	}

	if ((pidNPCToInc && (mObj && mObj->protoId == pidNPCToInc)) || (!pidNPCToInc && !_stricmp(name, nameNPCToInc))) {
		fo::func::debug_printf(fmt, name);

		SafeWrite32(0x495C50, 0x01FB840F); // Want to keep this check intact. (restore)

		SafeMemSet(0x495C77, CodeType::Nop, 6);   // Check that the player is high enough for the npc to consider this level
		//SafeMemSet(0x495C8C, CodeType::Nop, 6); // Check that the npc isn't already at its maximum level
		SafeMemSet(0x495CEC, CodeType::Nop, 6);   // Check that the npc hasn't already levelled up recently
		if (!npcAutoLevelEnabled) {
			SafeWrite8(0x495CFB, CodeType::JumpShort); // Disable random element
		}
		__asm mov [ebp + 0x150 - 0x28 + 16], 255; // set counter for exit loop
	} else {
		if (!onceNpcLoop) {
			SafeWrite32(0x495C50, 0x01FCE9); // set goto next member
			onceNpcLoop = true;
		}
	}
	__asm pop edx;
}

void op_inc_npc_level(OpcodeContext& ctx) {
	nameNPCToInc = ctx.arg(0).asString();
	pidNPCToInc = ctx.arg(0).asInt(); // set to 0 if passing npc name
	if (pidNPCToInc == 0 && nameNPCToInc[0] == 0) return;

	MakeCall(0x495BF1, IncNPCLevel);  // Replace the debug output
	__asm call fo::funcoffs::partyMemberIncLevels_;
	onceNpcLoop = false;

	// restore code
	SafeWrite32(0x495C50, 0x01FB840F);
	__int64 data = 0x01D48C0F;
	SafeWriteBytes(0x495C77, (BYTE*)&data, 6);
	//SafeWrite16(0x495C8C, 0x8D0F);
	//SafeWrite32(0x495C8E, 0x000001BF);
	data = 0x0130850F;
	SafeWriteBytes(0x495CEC, (BYTE*)&data, 6);
	if (!npcAutoLevelEnabled) {
		SafeWrite8(0x495CFB, CodeType::JumpZ);
	}
}

void op_get_npc_level(OpcodeContext& ctx) {
	int level = -1;
	DWORD findPid = ctx.arg(0).asInt(); // set to 0 if passing npc name
	const char *critterName, *name = ctx.arg(0).asString();

	if (findPid || name[0] != 0) {
		DWORD pid = 0;
		DWORD* members = fo::var::partyMemberList;
		for (DWORD i = 0; i < fo::var::partyMemberCount; i++) {
			if (!findPid) {
				__asm {
					mov  eax, members;
					mov  eax, [eax];
					call fo::funcoffs::critter_name_;
					mov  critterName, eax;
				}
				if (!_stricmp(name, critterName)) { // found npc
					pid = ((fo::GameObject*)*members)->protoId;
					break;
				}
			} else {
				DWORD _pid = ((fo::GameObject*)*members)->protoId;
				if (findPid == _pid) {
					pid = _pid;
					break;
				}
			}
			members += 4;
		}
		if (pid) {
			DWORD* pids = fo::var::partyMemberPidList;
			for (DWORD j = 0; j < fo::var::partyMemberMaxCount; j++) {
				if (pids[j] == pid) {
					level = fo::var::partyMemberLevelUpInfoList[j * 3];
					break;
				}
			}
		}
	}
	ctx.setReturn(level);
}

static bool IsSpecialIni(const char* str, const char* end) {
	const char* pos = strfind(str, &::sfall::ddrawIni[2]);
	if (pos && pos < end) return true;
	pos = strfind(str, "f2_res.ini");
	if (pos && pos < end) return true;
	return false;
}

static int ParseIniSetting(const char* iniString, const char* &key, char section[], char file[]) {
	key = strstr(iniString, "|");
	if (!key) return -1;

	DWORD filelen = (DWORD)key - (DWORD)iniString;
	if (ScriptExtender::iniConfigFolder.empty() && filelen >= 64) return -1;
	const char* fileEnd = key;

	key = strstr(key + 1, "|");
	if (!key) return -1;

	DWORD seclen = (DWORD)key - ((DWORD)iniString + filelen + 1);
	if (seclen > 32) return -1;

	file[0] = '.';
	file[1] = '\\';

	if (!ScriptExtender::iniConfigFolder.empty() && !IsSpecialIni(iniString, fileEnd)) {
		size_t len = ScriptExtender::iniConfigFolder.length(); // limit up to 62 characters
		memcpy(&file[2], ScriptExtender::iniConfigFolder.c_str(), len);
		memcpy(&file[2 + len], iniString, filelen); // copy path and file
		file[2 + len + filelen] = 0;
		if (GetFileAttributesA(file) & FILE_ATTRIBUTE_DIRECTORY) goto defRoot; // also file not found
	} else {
defRoot:
		memcpy(&file[2], iniString, filelen);
		file[2 + filelen] = 0;
	}
	memcpy(section, &iniString[filelen + 1], seclen);
	section[seclen] = 0;

	key++;
	return 1;
}

static DWORD GetIniSetting(const char* str, bool isString) {
	const char* key;
	char section[33], file[128];

	if (ParseIniSetting(str, key, section, file) < 0) {
		return -1;
	}
	if (isString) {
		ScriptExtender::gTextBuffer[0] = 0;
		iniGetString(section, key, "", ScriptExtender::gTextBuffer, 256, file);
		return (DWORD)&ScriptExtender::gTextBuffer[0];
	} else {
		return iniGetInt(section, key, -1, file);
	}
}

void op_get_ini_setting(OpcodeContext& ctx) {
	ctx.setReturn(GetIniSetting(ctx.arg(0).strValue(), false));
}

void op_get_ini_string(OpcodeContext& ctx) {
	DWORD result = GetIniSetting(ctx.arg(0).strValue(), true);
	ctx.setReturn(result, (result != -1) ? DataType::STR : DataType::INT);
}

void __declspec(naked) op_get_uptime() {
	__asm {
		mov  esi, ecx;
		call GetTickCount;
		mov  edx, eax;
		mov  eax, ebx;
		_RET_VAL_INT;
		mov  ecx, esi;
		retn;
	}
}

void __declspec(naked) op_set_car_current_town() {
	__asm {
		_GET_ARG_INT(end);
		mov  ds:[FO_VAR_carCurrentArea], eax;
end:
		retn;
	}
}

void __declspec(naked) op_set_hp_per_level_mod() {
	__asm {
		mov  esi, ecx;
		_GET_ARG_INT(end);
		push eax; // allowed -/+127
		push 0x4AFBC1;
		call SafeWrite8;
end:
		mov  ecx, esi;
		retn;
	}
}

static const char* valueOutRange = "%s() - argument values out of range.";

void op_set_critical_table(OpcodeContext& ctx) {
	DWORD critter = ctx.arg(0).rawValue(),
		bodypart  = ctx.arg(1).rawValue(),
		slot      = ctx.arg(2).rawValue(),
		element   = ctx.arg(3).rawValue();

	if (critter >= Criticals::critTableCount || bodypart >= 9 || slot >= 6 || element >= 7) {
		ctx.printOpcodeError(valueOutRange, ctx.getOpcodeName());
	} else {
		Criticals::SetCriticalTable(critter, bodypart, slot, element, ctx.arg(4).rawValue());
	}
}

void op_get_critical_table(OpcodeContext& ctx) {
	DWORD critter = ctx.arg(0).rawValue(),
		bodypart  = ctx.arg(1).rawValue(),
		slot      = ctx.arg(2).rawValue(),
		element   = ctx.arg(3).rawValue();

	if (critter >= Criticals::critTableCount || bodypart >= 9 || slot >= 6 || element >= 7) {
		ctx.printOpcodeError(valueOutRange, ctx.getOpcodeName());
	} else {
		ctx.setReturn(Criticals::GetCriticalTable(critter, bodypart, slot, element));
	}
}

void op_reset_critical_table(OpcodeContext& ctx) {
	DWORD critter = ctx.arg(0).rawValue(),
		bodypart  = ctx.arg(1).rawValue(),
		slot      = ctx.arg(2).rawValue(),
		element   = ctx.arg(3).rawValue();

	if (critter >= Criticals::critTableCount || bodypart >= 9 || slot >= 6 || element >= 7) {
		ctx.printOpcodeError(valueOutRange, ctx.getOpcodeName());
	} else {
		Criticals::ResetCriticalTable(critter, bodypart, slot, element);
	}
}

void __declspec(naked) op_set_unspent_ap_bonus() {
	__asm {
		_GET_ARG_INT(end);
		mov  Stats::standardApAcBonus, eax;
end:
		retn;
	}
}

void __declspec(naked) op_get_unspent_ap_bonus() {
	__asm {
		mov  edx, Stats::standardApAcBonus;
		_J_RET_VAL_TYPE(VAR_TYPE_INT);
//		retn;
	}
}

void __declspec(naked) op_set_unspent_ap_perk_bonus() {
	__asm {
		_GET_ARG_INT(end);
		mov  Stats::extraApAcBonus, eax;
end:
		retn;
	}
}

void __declspec(naked) op_get_unspent_ap_perk_bonus() {
	__asm {
		mov  edx, Stats::extraApAcBonus;
		_J_RET_VAL_TYPE(VAR_TYPE_INT);
//		retn;
	}
}

void op_set_palette(OpcodeContext& ctx) {
	const char* palette = ctx.arg(0).strValue();
	__asm {
		mov  eax, palette;
		call fo::funcoffs::loadColorTable_;
		mov  eax, FO_VAR_cmap;
		call fo::funcoffs::palette_set_to_;
	}
}

//numbers subgame functions
void __declspec(naked) op_nb_create_char() {
	__asm retn;
}

void __declspec(naked) op_hero_select_win() { // for opening the appearance selection window
	__asm {
		mov  esi, ecx;
		_GET_ARG_INT(fail);
		push eax;
		call HeroSelectWindow;
fail:
		mov  ecx, esi;
		retn;
	}
}

void __declspec(naked) op_set_hero_style() { // for setting the hero style/appearance takes an 1 int
	__asm {
		mov  esi, ecx;
		_GET_ARG_INT(fail);
		push eax;
		call SetHeroStyle;
fail:
		mov  ecx, esi;
		retn;
	}
}

void __declspec(naked) op_set_hero_race() { // for setting the hero race takes an 1 int
	__asm {
		mov  esi, ecx;
		_GET_ARG_INT(fail);
		push eax;
		call SetHeroRace;
fail:
		mov  ecx, esi;
		retn;
	}
}

void __declspec(naked) op_get_light_level() {
	__asm {
		mov  edx, ds:[FO_VAR_ambient_light];
		_J_RET_VAL_TYPE(VAR_TYPE_INT);
//		retn;
	}
}

void __declspec(naked) op_refresh_pc_art() {
	__asm {
		mov  esi, ecx;
		call RefreshPCArt;
		mov  ecx, esi;
		retn;
	}
}

void op_play_sfall_sound(OpcodeContext& ctx) {
	long soundID = 0;
	long mode = ctx.arg(1).rawValue();
	if (mode >= 0) soundID = Sound::PlaySfallSound(ctx.arg(0).strValue(), mode);
	ctx.setReturn(soundID);
}

void __declspec(naked) op_stop_sfall_sound() {
	__asm {
		mov  esi, ecx;
		_GET_ARG_INT(end);
		push eax;
		call Sound::StopSfallSound;
end:
		mov  ecx, esi;
		retn;
	}
}

// TODO: It seems that this function does not work...
void __declspec(naked) op_get_tile_fid() {
	__asm {
		push ecx;
		_GET_ARG_INT(fail); // get tile value
		mov  esi, ebx;      // keep script
		sub  esp, 8;        // x/y buf
		lea  edx, [esp];
		lea  ebx, [esp + 4];
		call fo::funcoffs::tile_coord_;
		pop  eax; // x
		pop  edx; // y
		call fo::funcoffs::square_num_;
		mov  edx, ds:[FO_VAR_square];
		movzx edx, word ptr ds:[edx + eax * 4];
		mov  ebx, esi; // script
end:
		mov  eax, ebx;
		_RET_VAL_INT;
		pop  ecx;
		retn;
fail:
		xor  edx, edx; // return 0
		jmp  end;
	}
}

void __declspec(naked) op_modified_ini() {
	__asm {
		mov  edx, modifiedIni;
		_J_RET_VAL_TYPE(VAR_TYPE_INT);
//		retn;
	}
}

void __declspec(naked) op_mark_movie_played() {
	__asm {
		_GET_ARG_INT(end);
		test eax, eax;
		jl   end;
		cmp  eax, 17;
		jge  end;
		mov  byte ptr ds:[eax + FO_VAR_gmovie_played_list], 1;
end:
		retn;
	}
}

void __declspec(naked) op_tile_under_cursor() {
	__asm {
		mov  esi, ebx;
		sub  esp, 8;
		lea  edx, [esp];
		lea  eax, [esp + 4];
		call fo::funcoffs::mouse_get_position_;
		pop  edx;
		pop  eax;
		call fo::funcoffs::tile_num_; // ebx - unused
		mov  edx, eax; // tile
		mov  ebx, esi;
		mov  eax, esi;
		_J_RET_VAL_TYPE(VAR_TYPE_INT);
//		retn;
	}
}

void __declspec(naked) op_gdialog_get_barter_mod() {
	__asm {
		mov  edx, dword ptr ds:[FO_VAR_gdBarterMod];
		_J_RET_VAL_TYPE(VAR_TYPE_INT);
//		retn;
	}
}

void op_sneak_success(OpcodeContext& ctx) {
	ctx.setReturn(fo::func::is_pc_sneak_working());
}

void op_tile_light(OpcodeContext& ctx) {
	int lightLevel = fo::func::light_get_tile(ctx.arg(0).rawValue(), ctx.arg(1).rawValue());
	ctx.setReturn(lightLevel);
}

void mf_exec_map_update_scripts(OpcodeContext& ctx) {
	__asm call fo::funcoffs::scr_exec_map_update_scripts_
}

void mf_set_ini_setting(OpcodeContext& ctx) {
	const ScriptValue &argVal = ctx.arg(1);

	const char* saveValue;
	if (argVal.isInt()) {
		_itoa_s(argVal.rawValue(), ScriptExtender::gTextBuffer, 10);
		saveValue = ScriptExtender::gTextBuffer;
	} else {
		saveValue = argVal.strValue();
	}
	const char* key;
	char section[33], file[128];
	int result = ParseIniSetting(ctx.arg(0).strValue(), key, section, file);
	if (result > 0) {
		result = WritePrivateProfileStringA(section, key, saveValue, file);
	}

	switch (result) {
	case 0:
		ctx.printOpcodeError("%s() - value save error.", ctx.getMetaruleName());
		break;
	case -1:
		ctx.printOpcodeError("%s() - invalid setting argument.", ctx.getMetaruleName());
		break;
	default:
		return;
	}
	ctx.setReturn(-1);
}

static std::string GetIniFilePath(const ScriptValue &arg) {
	std::string fileName(".\\");
	if (ScriptExtender::iniConfigFolder.empty()) {
		fileName += arg.strValue();
	} else {
		fileName += ScriptExtender::iniConfigFolder;
		fileName += arg.strValue();
		if (GetFileAttributesA(fileName.c_str()) & FILE_ATTRIBUTE_DIRECTORY) {
			auto str = arg.strValue();
			for (size_t i = 2; ; i++, str++) {
				//if (*str == '.') str += (str[1] == '.') ? 3 : 2; // skip '.\' or '..\'
				fileName[i] = *str;
				if (!*str) break;
			}
		}
	}
	return fileName;
}

void mf_get_ini_sections(OpcodeContext& ctx) {
	if (!GetPrivateProfileSectionNamesA(ScriptExtender::gTextBuffer, ScriptExtender::TextBufferSize(), GetIniFilePath(ctx.arg(0)).c_str())) {
		ctx.setReturn(CreateTempArray(0, 0));
		return;
	}
	std::vector<char*> sections;
	char* section = ScriptExtender::gTextBuffer;
	while (*section != 0) {
		sections.push_back(section); // position
		section += std::strlen(section) + 1;
	}
	size_t sz = sections.size();
	int arrayId = CreateTempArray(sz, 0);
	auto& arr = arrays[arrayId];

	for (size_t i = 0; i < sz; ++i) {
		size_t j = i + 1;
		int len = (j < sz) ? sections[j] - sections[i] - 1 : -1;
		arr.val[i].set(sections[i], len); // copy string from buffer
	}
	ctx.setReturn(arrayId);
}

void mf_get_ini_section(OpcodeContext& ctx) {
	auto section = ctx.arg(1).strValue();
	int arrayId = CreateTempArray(-1, 0); // associative

	if (GetPrivateProfileSectionA(section, ScriptExtender::gTextBuffer, ScriptExtender::TextBufferSize(), GetIniFilePath(ctx.arg(0)).c_str())) {
		auto& arr = arrays[arrayId];
		char *key = ScriptExtender::gTextBuffer, *val = nullptr;
		while (*key != 0) {
			char* val = std::strpbrk(key, "=");
			if (val != nullptr) {
				*val = '\0';
				val += 1;

				setArray(arrayId, ScriptValue(key), ScriptValue(val), false);

				key = val + std::strlen(val) + 1;
			} else {
				key += std::strlen(key) + 1;
			}
		}
	}
	ctx.setReturn(arrayId);
}

void mf_npc_engine_level_up(OpcodeContext& ctx) {
	if (ctx.arg(0).asBool()) {
		if (!npcEngineLevelUp) SafeWrite16(0x4AFC1C, 0x840F); // enable
		npcEngineLevelUp = true;
	} else {
		if (npcEngineLevelUp) SafeWrite16(0x4AFC1C, 0xE990);
		npcEngineLevelUp = false;
	}
}

}
}
