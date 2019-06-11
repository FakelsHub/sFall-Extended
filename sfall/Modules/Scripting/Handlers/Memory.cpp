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
#include "..\..\ScriptExtender.h"

#include "Memory.h"

namespace sfall
{
namespace script
{

void __declspec(naked) op_read_byte() {
	__asm {
		push ebx;
		push ecx;
		push edx;
		mov ecx, eax;
		call fo::funcoffs::interpretPopShort_;
		mov edx, eax;
		mov eax, ecx;
		call fo::funcoffs::interpretPopLong_;
		cmp dx, VAR_TYPE_INT;
		jnz error;
		movzx edx, byte ptr ds:[eax];
		jmp result;
error:
		mov edx, 0;
result:
		mov eax, ecx;
		call fo::funcoffs::interpretPushLong_;
		mov edx, VAR_TYPE_INT;
		mov eax, ecx;
		call fo::funcoffs::interpretPushShort_;
		pop edx;
		pop ecx;
		pop ebx;
		retn;
	}
}

void __declspec(naked) op_read_short() {
	__asm {
		push ebx;
		push ecx;
		push edx;
		mov ecx, eax;
		call fo::funcoffs::interpretPopShort_;
		mov edx, eax;
		mov eax, ecx;
		call fo::funcoffs::interpretPopLong_;
		cmp dx, VAR_TYPE_INT;
		jnz error;
		movzx edx, word ptr ds:[eax];
		jmp result;
error:
		mov edx, 0;
result:
		mov eax, ecx;
		call fo::funcoffs::interpretPushLong_;
		mov edx, VAR_TYPE_INT;
		mov eax, ecx;
		call fo::funcoffs::interpretPushShort_;
		pop edx;
		pop ecx;
		pop ebx;
		retn;
	}
}

void __declspec(naked) op_read_int() {
	__asm {
		push ebx;
		push ecx;
		push edx;
		mov ecx, eax;
		call fo::funcoffs::interpretPopShort_;
		mov edx, eax;
		mov eax, ecx;
		call fo::funcoffs::interpretPopLong_;
		cmp dx, VAR_TYPE_INT;
		jnz error;
		mov edx, dword ptr ds:[eax];
		jmp result;
error:
		mov edx, 0;
result:
		mov eax, ecx;
		call fo::funcoffs::interpretPushLong_;
		mov edx, VAR_TYPE_INT;
		mov eax, ecx;
		call fo::funcoffs::interpretPushShort_;
		pop edx;
		pop ecx;
		pop ebx;
		retn;
	}
}

void __declspec(naked) op_read_string() {
	__asm {
		push ebx;
		push ecx;
		push edx;
		mov ecx, eax;
		call fo::funcoffs::interpretPopShort_;
		mov edx, eax;
		mov eax, ecx;
		call fo::funcoffs::interpretPopLong_;
		cmp dx, VAR_TYPE_INT;
		jnz error;
		mov edx, eax;
		jmp result;
error:
		mov edx, 0;
result:
		mov eax, ecx;
		call fo::funcoffs::interpretPushLong_;
		mov edx, VAR_TYPE_STR;
		mov eax, ecx;
		call fo::funcoffs::interpretPushShort_;
		pop edx;
		pop ecx;
		pop ebx;
		retn;
	}
}

void __declspec(naked) op_write_byte() {
	__asm {
		pushad
		mov ecx, eax;
		call fo::funcoffs::interpretPopShort_;
		mov esi, eax;
		mov eax, ecx;
		call fo::funcoffs::interpretPopLong_;
		mov edx, eax;
		mov eax, ecx;
		call fo::funcoffs::interpretPopShort_;
		mov edi, eax;
		mov eax, ecx;
		call fo::funcoffs::interpretPopLong_;
		cmp di, VAR_TYPE_INT;
		jnz end;
		cmp si, VAR_TYPE_INT;
		jnz end;
		//mov byte ptr ds:[eax], dl;
		and edx, 0xff;
		push edx;
		push eax;
		call SafeWrite8;
end:
		popad;
		retn;
	}
}

void __declspec(naked) op_write_short() {
	__asm {
		pushad;
		mov ecx, eax;
		call fo::funcoffs::interpretPopShort_;
		mov esi, eax;
		mov eax, ecx;
		call fo::funcoffs::interpretPopLong_;
		mov edx, eax;
		mov eax, ecx;
		call fo::funcoffs::interpretPopShort_;
		mov edi, eax;
		mov eax, ecx;
		call fo::funcoffs::interpretPopLong_;
		cmp di, VAR_TYPE_INT;
		jnz end;
		cmp si, VAR_TYPE_INT;
		jnz end;
		//mov word ptr ds:[eax], dx;
		and edx, 0xffff;
		push edx;
		push eax;
		call SafeWrite16;
end:
		popad;
		retn;
	}
}

void __declspec(naked) op_write_int() {
	__asm {
		pushad
		mov ecx, eax;
		call fo::funcoffs::interpretPopShort_;
		mov esi, eax;
		mov eax, ecx;
		call fo::funcoffs::interpretPopLong_;
		mov edx, eax;
		mov eax, ecx;
		call fo::funcoffs::interpretPopShort_;
		mov edi, eax;
		mov eax, ecx;
		call fo::funcoffs::interpretPopLong_;
		cmp di, VAR_TYPE_INT;
		jnz end;
		cmp si, VAR_TYPE_INT;
		jnz end;
		//mov dword ptr ds:[eax], edx;
		push edx;
		push eax;
		call SafeWrite32;
end:
		popad
		retn;
	}
}

static void _stdcall WriteStringInternal(const char* str, char* addr) {
	bool hitnull = false;
	while (*str) {
		if (!*addr) hitnull = true;
		if (hitnull && addr[1]) break;
		*addr = *str;
		addr++;
		str++;
	}
	*addr = 0;
}

void __declspec(naked) op_write_string() {
	__asm {
		pushad;
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
		cmp si, VAR_TYPE_STR2;
		jz next;
		cmp si, VAR_TYPE_STR;
		jnz end;
next:
		mov ebx, edi;
		mov edx, esi;
		mov esi, eax;
		mov eax, ecx;
		call fo::funcoffs::interpretGetString_;
		push esi;
		push eax;
		call WriteStringInternal;
		jmp end;
end:
		popad;
		retn;
	}
}

static void _stdcall CallOffsetInternal(DWORD func, DWORD script) {
	func = (func >> 2) - 0x1d2;
	bool ret = func >= 5;
	int argcount = func % 5;
	DWORD args[5];
	DWORD illegalarg = 0;
	for (int i = argcount * 4; i >= 0; i -= 4) {
		__asm {
			mov eax, script;
			call fo::funcoffs::interpretPopShort_;
			cmp ax, VAR_TYPE_INT;
			jz legal;
			inc illegalarg;
legal:
			mov eax, script;
			call fo::funcoffs::interpretPopLong_;
			lea ecx, args;
			add ecx, i;
			mov [ecx], eax;
		}
	}

	if (illegalarg) {
		args[0] = 0;
	} else {
		__asm {
			mov eax, args[4];
			mov edx, args[8];
			mov ebx, args[12];
			mov ecx, args[16];
			mov edi, args[0];
			call edi;
			mov args[0], eax;
		}
	}
	if (ret) {
		__asm {
			mov eax, script;
			mov edx, args[0];
			call fo::funcoffs::interpretPushLong_;
			mov eax, script;
			mov edx, VAR_TYPE_INT;
			call fo::funcoffs::interpretPushShort_;
		}
	}
}

void __declspec(naked) op_call_offset() {
	__asm {
		pushad;
		push eax;
		push edx;
		call CallOffsetInternal;
		popad;
		retn;
	}
}

}
}