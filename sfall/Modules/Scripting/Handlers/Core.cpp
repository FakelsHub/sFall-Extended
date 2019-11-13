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
#include "..\..\..\Version.h"
#include "..\..\KillCounter.h"
#include "..\..\HookScripts.h"
#include "..\..\ScriptExtender.h"
#include "..\Arrays.h"
#include "..\OpcodeContext.h"

#include "Core.h"

namespace sfall
{
namespace script
{

void __declspec(naked) op_set_global_script_repeat() {
	__asm {
		push ecx;
		mov  ecx, eax;
		_GET_ARG_INT(end);
		mov  edx, eax;              // frames
		call SetGlobalScriptRepeat; // ecx - script
end:
		pop ecx;
		retn;
	}
}

void __declspec(naked) op_set_global_script_type() {
	__asm {
		push ecx;
		mov  ecx, eax;
		_GET_ARG_INT(end);
		mov  edx, eax;            // type
		call SetGlobalScriptType; // ecx - script
end:
		pop  ecx;
		retn;
	}
}

void __declspec(naked) op_available_global_script_types() {
	__asm {
		push ecx;
		mov  edx, availableGlobalScriptTypes;
		_RET_VAL_INT(ecx);
		pop  ecx;
		retn;
	}
}

void sf_set_sfall_global(OpcodeContext& ctx) {
	if (ctx.arg(0).isString()) {
		if (SetGlobalVar(ctx.arg(0).strValue(), ctx.arg(1).rawValue())) {
			ctx.printOpcodeError("%s() - the name of the global variable must consist of 8 characters.", ctx.getOpcodeName());
		}
	} else {
		SetGlobalVarInt(ctx.arg(0).rawValue(), ctx.arg(1).rawValue());
	}
}

static long GetGlobalVarNameString(OpcodeContext& ctx) {
	const char* var = ctx.arg(0).strValue();
	if (strlen(var) != 8) {
		ctx.printOpcodeError("%s() - the name of the global variable must consist of 8 characters.", ctx.getOpcodeName());
		return 0;
	}
	return GetGlobalVarInternal(*(__int64*)var);
}

static void GetGlobalVar(OpcodeContext& ctx, DataType type) {
	long result;
	if (ctx.arg(0).isString()) {
		result = GetGlobalVarNameString(ctx);
	} else {
		result = GetGlobalVarInt(ctx.arg(0).rawValue());
	}
	ctx.setReturn(result, type);
}

void sf_get_sfall_global_int(OpcodeContext& ctx) {
	GetGlobalVar(ctx, DataType::INT);
}

void sf_get_sfall_global_float(OpcodeContext& ctx) {
	GetGlobalVar(ctx, DataType::FLOAT);
}

void __declspec(naked) op_get_sfall_arg() {
	__asm {
		push ecx;
		push eax;
		call GetHSArg;
		mov  edx, eax;
		pop  eax;
		_RET_VAL_INT(ecx);
		pop  ecx;
		retn;
	}
}

void sf_get_sfall_args(OpcodeContext& ctx) {
	DWORD argCount = GetHSArgCount();
	DWORD id = TempArray(argCount, 0);
	DWORD* args = GetHSArgs();
	for (DWORD i = 0; i < argCount; i++) {
		arrays[id].val[i].set(*(long*)&args[i]);
	}
	ctx.setReturn(id);
}

void sf_set_sfall_arg(OpcodeContext& ctx) {
	SetHSArg(ctx.arg(0).rawValue(), ctx.arg(1).rawValue());
}

void __declspec(naked) op_set_sfall_return() {
	__asm {
		push ecx;
		_GET_ARG_INT(end);
		push eax;
		call SetHSReturn;
end:
		pop  ecx;
		retn;
	}
}

void __declspec(naked) op_init_hook() {
	__asm {
		push ecx;
		mov  edx, initingHookScripts;
		_RET_VAL_INT(ecx);
		pop  ecx;
		retn;
	}
}

void __declspec(naked) op_set_self() {
	__asm {
		push ecx;
		mov  ecx, eax;
		_GET_ARG_INT(end);
		mov  edx, eax;      // object
		call SetSelfObject; // ecx - script
end:
		pop  ecx;
		retn;
	}
}

// used for both register_hook and register_hook_proc
void sf_register_hook(OpcodeContext& ctx) {
	bool specReg = false;
	int proc;
	switch (ctx.opcode()) {
	case 0x27d :
		specReg = true;
	case 0x262 :
		proc = ctx.arg(1).rawValue();
		if (proc < 0 || (specReg && proc == 0)) return;
		break;
	default:
		proc = -1;
	}
	int id = ctx.arg(0).rawValue();
	RegisterHook(ctx.program(), id, proc, specReg);
}

void sf_add_timer_event(OpcodeContext& ctx) {
	ScriptExtender::AddTimerEventScripts(ctx.program(), ctx.arg(0).rawValue(), ctx.arg(1).rawValue());
}

void sf_remove_timer_event(OpcodeContext& ctx) {
	if (ctx.numArgs() > 0) {
		ScriptExtender::RemoveTimerEventScripts(ctx.program(), ctx.arg(0).rawValue());
	} else {
		ScriptExtender::RemoveTimerEventScripts(ctx.program()); // remove all
	}
}

void sf_reg_anim_callback(OpcodeContext& ctx) {
	fo::func::register_object_call(
		reinterpret_cast<long*>(ctx.program()),
		reinterpret_cast<long*>(ctx.arg(0).rawValue()),// callback procedure
		reinterpret_cast<void*>(fo::funcoffs::executeProcedure_),
		-1
	);
}

void sf_sfall_ver_major(OpcodeContext& ctx) {
	ctx.setReturn(VERSION_MAJOR);
}

void sf_sfall_ver_minor(OpcodeContext& ctx) {
	ctx.setReturn(VERSION_MINOR);
}

void sf_sfall_ver_build(OpcodeContext& ctx) {
	ctx.setReturn(VERSION_BUILD);
}

}
}
