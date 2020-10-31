/*
 *    sfall
 *    Copyright (C) 2008, 2009, 2010, 2012  The sfall team
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

#include <stdio.h>

#include "..\main.h"
#include "..\FalloutEngine\Fallout2.h"
#include "..\Logging.h"
#include "LoadGameHook.h"

#include "Criticals.h"

namespace sfall
{

static std::string critTableFile(".\\");

const DWORD Criticals::critTableCount = 2 * 19 + 1; // Number of species in new critical table

static DWORD mode;

static const char* critNames[] = {
	"DamageMultiplier",
	"EffectFlags",
	"StatCheck",
	"StatMod",
	"FailureEffect",
	"Message",
	"FailMessage",
};

static fo::CritInfo baseCritTable[Criticals::critTableCount][9][6] = {0}; // Loaded base table from CriticalOverrides.ini (with bug fixes and default engine values)
static fo::CritInfo critTable[Criticals::critTableCount][9][6];
static fo::CritInfo (*playerCrit)[9][6];

static bool Inited = false;

static const char* errorTable = "\nError: %s - function requires enabling OverrideCriticalTable in ddraw.ini.";

void Criticals::SetCriticalTable(DWORD critter, DWORD bodypart, DWORD slot, DWORD element, DWORD value) {
	if (!Inited) {
		fo::func::debug_printf(errorTable, "set_critical_table()");
		return;
	}
	critTable[critter][bodypart][slot].values[element] = value;
}

DWORD Criticals::GetCriticalTable(DWORD critter, DWORD bodypart, DWORD slot, DWORD element) {
	if (!Inited) {
		fo::func::debug_printf(errorTable, "get_critical_table()");
		return 0;
	}
	return critTable[critter][bodypart][slot].values[element];
}

void Criticals::ResetCriticalTable(DWORD critter, DWORD bodypart, DWORD slot, DWORD element) {
	if (!Inited) {
		fo::func::debug_printf(errorTable, "reset_critical_table()");
		return;
	}
	critTable[critter][bodypart][slot].values[element] = baseCritTable[critter][bodypart][slot].values[element];
}

static int CritTableLoad() {
	if (mode == 1) {
		dlog("\n  Setting up critical hit table using CriticalOverrides.ini file (old fmt)", DL_CRITICALS);
		if (GetFileAttributes(critTableFile.c_str()) == INVALID_FILE_ATTRIBUTES) return 1;
		char section[16];
		for (DWORD critter = 0; critter < 20; critter++) {
			for (DWORD part = 0; part < 9; part++) {
				for (DWORD crit = 0; crit < 6; crit++) {
					sprintf_s(section, "c_%02d_%d_%d", critter, part, crit);
					DWORD newCritter = (critter == 19) ? 38 : critter;
					fo::CritInfo& newEffect = baseCritTable[newCritter][part][crit];
					fo::CritInfo& defaultEffect = fo::var::crit_succ_eff[critter][part][crit];
					for (int i = 0; i < 7; i++) {
						newEffect.values[i] = iniGetInt(section, critNames[i], defaultEffect.values[i], critTableFile.c_str());
						if (isDebug) {
							char logmsg[256];
							if (newEffect.values[i] != defaultEffect.values[i]) {
								sprintf_s(logmsg, "  Entry %s value %d changed from %d to %d", section, i, defaultEffect.values[i], newEffect.values[i]);
								dlogr(logmsg, DL_CRITICALS);
							}
						}
					}
				}
			}
		}
	} else {
		if (mode != 4) dlog("\n  Setting up critical hit table using RP fixes", DL_CRITICALS);
		constexpr int size = 6 * 9 * sizeof(fo::CritInfo);
		constexpr int sizeF = 19 * size;
		memcpy(baseCritTable, fo::var::crit_succ_eff, sizeF);
		//memset(&baseCritTable[19], 0, sizeF);
		memcpy(&baseCritTable[38], fo::var::pc_crit_succ_eff, size); // PC crit table

		if (mode == 3) {
			dlog(" and CriticalOverrides.ini file", DL_CRITICALS);
			if (GetFileAttributes(critTableFile.c_str()) == INVALID_FILE_ATTRIBUTES) return 1;
			char buf[32], buf2[32], buf3[32];
			for (int critter = 0; critter < Criticals::critTableCount; critter++) {
				sprintf_s(buf, "c_%02d", critter);
				int all;
				if (!(all = iniGetInt(buf, "Enabled", 0, critTableFile.c_str()))) continue;
				for (int part = 0; part < 9; part++) {
					if (all < 2) {
						sprintf_s(buf2, "Part_%d", part);
						if (!iniGetInt(buf, buf2, 0, critTableFile.c_str())) continue;
					}

					sprintf_s(buf2, "c_%02d_%d", critter, part);
					for (int crit = 0; crit < 6; crit++) {
						fo::CritInfo& effect = baseCritTable[critter][part][crit];
						for (int i = 0; i < 7; i++) {
							sprintf_s(buf3, "e%d_%s", crit, critNames[i]);
							if (i == 1) { // EffectFlags
								auto valStr = GetIniList(buf2, buf3, "", 128, ',', critTableFile.c_str());
								int size = valStr.size();
								if (size == 0) continue;
								int value = 0;
								for (int j = 0; j < size; j++) {
									try {
										value |= std::stoi(valStr[j], nullptr, 0);
									} catch (...) {
										char msgbuff[128];
										sprintf_s(msgbuff, "Incorrect value of the EffectFlags parameter in the section [%s] of the table.", buf2);
										MessageBoxA(0, msgbuff, "Critical table file", MB_TASKMODAL | MB_ICONERROR);
										return -1;
									}
								}
								effect.values[i] = value;
							} else {
								effect.values[i] = iniGetInt(buf2, buf3, effect.values[i], critTableFile.c_str());
							}
						}
					}
				}
			}
		}
	}
	return 0;
}

enum BodyPart {
	Head,
	ArmLeft,
	ArmRight,
	Torso,
	LegRight,
	LegLeft,
	Eyes,
	Groin,
	Uncalled
};

enum CritParam {
	DmgMult,
	Flags,
	StatCheck,
	StatMod,
	FlagsFail,
	Message,
	MsgFail
};

#define SetEntry(critter, bodypart, effect, param, value) fo::var::crit_succ_eff[critter][bodypart][effect].values[param] = value

static void CriticalTableOverride() {
	dlog("Initializing critical table override...", DL_INIT);
	playerCrit = &critTable[38];
	SafeWrite32(0x423F96, (DWORD)playerCrit);
	SafeWrite32(0x423FB3, (DWORD)critTable);

	if (mode == 2 || mode == 3) { // bug fixes
		// children
		SetEntry(2, LegRight, 1, FlagsFail, 0);
		SetEntry(2, LegRight, 1, Message,   5216);
		SetEntry(2, LegRight, 1, MsgFail,   5000);
		// children
		SetEntry(2, LegRight, 2, FlagsFail, 0);
		SetEntry(2, LegRight, 2, Message,   5216);
		SetEntry(2, LegRight, 2, MsgFail,   5000);
		// children
		SetEntry(2, LegLeft,  1, FlagsFail, 0);
		SetEntry(2, LegLeft,  1, Message,   5216);
		SetEntry(2, LegLeft,  1, MsgFail,   5000);
		// children
		SetEntry(2, LegLeft,  2, FlagsFail, 0);
		SetEntry(2, LegLeft,  2, Message,   5216);
		SetEntry(2, LegLeft,  2, MsgFail,   5000);

		// super mutant
		SetEntry(3, LegLeft,  1, MsgFail,   5306);

		// ghoul
		SetEntry(4, Head,     4, StatCheck, -1);

		// brahmin
		SetEntry(5, Head,     4, StatCheck, -1);

		// radscorpion
		SetEntry(6, LegRight, 1, FlagsFail, fo::DAM_KNOCKED_DOWN);
		// radscorpion
		SetEntry(6, LegLeft,  1, FlagsFail, fo::DAM_KNOCKED_DOWN);
		// radscorpion
		SetEntry(6, LegLeft,  2, MsgFail,   5608);

		// centaur
		SetEntry(9, Torso,    3, FlagsFail, fo::DAM_KNOCKED_DOWN);

		// deathclaw
		SetEntry(13, LegLeft, 1, FlagsFail, fo::DAM_CRIP_LEG_LEFT);
		SetEntry(13, LegLeft, 2, FlagsFail, fo::DAM_CRIP_LEG_LEFT);
		SetEntry(13, LegLeft, 3, FlagsFail, fo::DAM_CRIP_LEG_LEFT);
		SetEntry(13, LegLeft, 4, FlagsFail, fo::DAM_CRIP_LEG_LEFT);
		SetEntry(13, LegLeft, 5, FlagsFail, fo::DAM_CRIP_LEG_LEFT);

		// big boss
		SetEntry(18, Head,     0, Message,  5001);
		SetEntry(18, Head,     1, Message,  5001);
		SetEntry(18, Head,     2, Message,  5001);
		SetEntry(18, Head,     3, Message,  7105);
		SetEntry(18, Head,     4, Message,  7101);
		SetEntry(18, Head,     4, MsgFail,  7104);
		SetEntry(18, Head,     5, Message,  7101);
		// big boss
		SetEntry(18, ArmLeft,  0, Message,  5008);
		SetEntry(18, ArmLeft,  1, Message,  5008);
		SetEntry(18, ArmLeft,  2, Message,  5009);
		SetEntry(18, ArmLeft,  3, Message,  5009);
		SetEntry(18, ArmLeft,  4, Message,  7102);
		SetEntry(18, ArmLeft,  5, Message,  7102);
		// big boss
		SetEntry(18, ArmRight, 0, Message,  5008);
		SetEntry(18, ArmRight, 1, Message,  5008);
		SetEntry(18, ArmRight, 2, Message,  5009);
		SetEntry(18, ArmRight, 3, Message,  5009);
		SetEntry(18, ArmRight, 4, Message,  7102);
		SetEntry(18, ArmRight, 5, Message,  7102);
		// big boss
		SetEntry(18, Torso,    4, Message,  7101);
		SetEntry(18, Torso,    5, Message,  7101);
		// big boss
		SetEntry(18, LegRight, 0, Message,  5023);
		SetEntry(18, LegRight, 1, Message,  7101);
		SetEntry(18, LegRight, 1, MsgFail,  7103);
		SetEntry(18, LegRight, 2, Message,  7101);
		SetEntry(18, LegRight, 2, MsgFail,  7103);
		SetEntry(18, LegRight, 3, Message,  7103);
		SetEntry(18, LegRight, 4, Message,  7103);
		SetEntry(18, LegRight, 5, Message,  7103);
		// big boss
		SetEntry(18, LegLeft,  0, Message,  5023);
		SetEntry(18, LegLeft,  1, Message,  7101);
		SetEntry(18, LegLeft,  1, MsgFail,  7103);
		SetEntry(18, LegLeft,  2, Message,  7101);
		SetEntry(18, LegLeft,  2, MsgFail,  7103);
		SetEntry(18, LegLeft,  3, Message,  7103);
		SetEntry(18, LegLeft,  4, Message,  7103);
		SetEntry(18, LegLeft,  5, Message,  7103);
		// big boss
		SetEntry(18, Eyes,     0, Message,  5027);
		SetEntry(18, Eyes,     1, Message,  5027);
		SetEntry(18, Eyes,     2, Message,  5027);
//		SetEntry(18, Eyes,     2, MsgFail,     0);
		SetEntry(18, Eyes,     3, Message,  5027);
		SetEntry(18, Eyes,     4, Message,  7104);
		SetEntry(18, Eyes,     5, Message,  7104);
		// big boss
		SetEntry(18, Groin,    0, Message,  5033);
		SetEntry(18, Groin,    1, Message,  5027);
		SetEntry(18, Groin,    1, MsgFail,  7101);
		SetEntry(18, Groin,    2, Message,  7101);
		SetEntry(18, Groin,    3, Message,  7101);
		SetEntry(18, Groin,    4, Message,  7101);
		SetEntry(18, Groin,    5, Message,  7101);
	}

	if (CritTableLoad()) {
		dlogr(". Failed to initialize critical hit table from file.", DL_INIT);
	} else {
		dlogr(". Completed applying critical hit table.", DL_INIT);
	}
	Inited = true;
}
#undef SetEntry

static void RemoveCriticalTimeLimitsPatch() {
	if (GetConfigInt("Misc", "RemoveCriticalTimelimits", 0)) {
		dlog("Removing critical time limits.", DL_INIT);
		SafeWrite8(0x424118, CodeType::JumpShort); // jump to 0x424131
		SafeWriteBatch<WORD>(0x9090, { 0x4A3052, 0x4A3093 });
		dlogr(" Done", DL_INIT);
	}
}

void Criticals::init() {
	mode = GetConfigInt("Misc", "OverrideCriticalTable", 2);
	if (mode < 0 || mode > 4) mode = 0;
	if (mode) {
		critTableFile += GetConfigString("Misc", "OverrideCriticalFile", "CriticalOverrides.ini", MAX_PATH);
		CriticalTableOverride();
		LoadGameHook::OnBeforeGameStart() += []() {
			memcpy(critTable, baseCritTable, sizeof(critTable)); // Apply loaded critical table
		};
	}

	RemoveCriticalTimeLimitsPatch();
}

}
