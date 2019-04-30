/*
 *    sfall
 *    Copyright (C) 2008, 2009, 2010  The sfall team
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

#include "..\main.h"
#include "..\FalloutEngine\Structs.h"
#include "..\Delegate.h"

#include "Module.h"

namespace sfall
{

class ScriptExtender : public Module {
public:
	const char* name() { return "ScriptExtender"; }
	void init();

	// Called before map exit (before map_exit_p_proc handlers in normal scripts)
	static Delegate<>& OnMapExit();
};

#pragma pack(push, 8)
struct GlobalVar {
	__int64 id;
	__int32 val;
	__int32 unused;
};
#pragma pack(pop)

typedef struct {
	fo::Program* ptr = nullptr;
	int procLookup[fo::ScriptProc::count];
	char initialized;
} ScriptProgram;

void _fastcall SetGlobalScriptRepeat(fo::Program* script, int frames);
void _fastcall SetGlobalScriptType(fo::Program* script, int type);
bool _stdcall IsGameScript(const char* filename);

void _stdcall RunGlobalScriptsAtProc(DWORD procId);

bool LoadGlobals(HANDLE h);
void SaveGlobals(HANDLE h);

int GetNumGlobals();
void GetGlobals(GlobalVar* globals);
void SetGlobals(GlobalVar* globals);
long SetGlobalVar(const char* var, int val);
void SetGlobalVarInt(DWORD var, int val);

long GetGlobalVar(const char* var);
long GetGlobalVarInt(DWORD var);
long GetGlobalVarInternal(__int64 val);

void _fastcall SetSelfObject(fo::Program* script, fo::GameObject* obj);

void _stdcall RegAnimCombatCheck(DWORD newValue);

bool _stdcall ScriptHasLoaded(fo::Program* script);

// loads script from .int file into a sScriptProgram struct, filling script pointer and proc lookup table
// prog - reference to program structure
// fileName - the script file name without extension (if fullPath is false) or a full file path (if fullPath is true)
// fullPath - controls how fileName is used (see above)
void LoadScriptProgram(ScriptProgram &prog, const char* fileName, bool fullPath = false);

// init program after load, needs to be called once
void InitScriptProgram(ScriptProgram &prog);

// execute script by specific proc name
void RunScriptProc(ScriptProgram* prog, const char* procName);

// execute script proc by procId from define.h
void RunScriptProc(ScriptProgram* prog, long procId);

void AddProgramToMap(ScriptProgram &prog);
ScriptProgram* GetGlobalScriptProgram(fo::Program* scriptPtr);

// variables
static char regAnimCombatCheck = 1;
extern DWORD isGlobalScriptLoading;
extern DWORD availableGlobalScriptTypes;
extern bool alwaysFindScripts;

}
