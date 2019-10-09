
#include "main.h"

namespace sfall
{

enum CodeType : BYTE {
	Call = 0xE8,
	Jump = 0xE9,
	Nop =  0x90
};

#ifndef NDEBUG
std::multimap<long, long> writeAddress;

/* Checking for conflicts requires that all options in ddraw.ini be enabled */
void PrintAddrList() {
	bool level = (GetPrivateProfileIntA("Debugging", "Enable", 0, ::sfall::ddrawIni) > 1);
	unsigned long pa = 0, pl = 0;
	for (const auto &wa : writeAddress) {
		unsigned long diff = (pa) ? (wa.first - pa) : -1; // length between two addresses
		if (diff == 0) {
			dlog_f("0x%x L:%d [Overwriting]\n", DL_MAIN, wa.first, wa.second);
		} else if (diff < pl) {
			dlog_f("0x%x L:%d [Conflict] with 0x%x L:%d\n", DL_MAIN, wa.first, wa.second, pa, pl);
		} else if (level && diff == pl) {
			dlog_f("0x%x L:%d [Warning] PL:%d\n", DL_MAIN, wa.first, wa.second, pl);
		} else if (level) {
			dlog_f("0x%x L:%d\n", DL_MAIN, wa.first, wa.second);
		}
		pa = wa.first;
		pl = wa.second;
	}
}

void CheckConflict(DWORD addr, long len) {
	if (writeAddress.find(addr) !=  writeAddress.cend()) {
			char buf[64];
			sprintf_s(buf, "Memory writing conflict at address 0x%x.", addr);
			//dlogr(buf, DL_MAIN);
			MessageBoxA(0, buf, "", MB_TASKMODAL);
	}
	writeAddress.emplace(addr, len);
}

void AddrAddToList(DWORD addr, long len) {
	writeAddress.emplace(addr, len);
}
#else
void CheckConflict(DWORD addr, long len) {}
void AddrAddToList(DWORD addr, long len) {}
#endif

static void _stdcall SafeWriteFunc(BYTE code, DWORD addr, void* func) {
	DWORD oldProtect, data = (DWORD)func - (addr + 5);

	VirtualProtect((void *)addr, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
	*((BYTE*)addr) = code;
	*((DWORD*)(addr + 1)) = data;
	VirtualProtect((void *)addr, 5, oldProtect, &oldProtect);

	CheckConflict(addr, 5);
}

static _declspec(noinline) void _stdcall SafeWriteFunc(BYTE code, DWORD addr, void* func, DWORD len) {
	DWORD oldProtect,
		protectLen = len + 5,
		addrMem = addr + 5,
		data = (DWORD)func - addrMem;

	VirtualProtect((void *)addr, protectLen, PAGE_EXECUTE_READWRITE, &oldProtect);
	*((BYTE*)addr) = code;
	*((DWORD*)(addr + 1)) = data;

	for (unsigned int i = 0; i < len; i++) {
		*((BYTE*)(addrMem + i)) = CodeType::Nop;
	}
	VirtualProtect((void *)addr, protectLen, oldProtect, &oldProtect);

	CheckConflict(addr, protectLen);
}

void SafeWriteBytes(DWORD addr, BYTE* data, int count) {
	DWORD oldProtect;

	VirtualProtect((void *)addr, count, PAGE_EXECUTE_READWRITE, &oldProtect);
	memcpy((void*)addr, data, count);
	VirtualProtect((void *)addr, count, oldProtect, &oldProtect);

	AddrAddToList(addr, count);
}

void _stdcall SafeWrite8(DWORD addr, BYTE data) {
	DWORD oldProtect;

	VirtualProtect((void *)addr, 1, PAGE_EXECUTE_READWRITE, &oldProtect);
	*((BYTE*)addr) = data;
	VirtualProtect((void *)addr, 1, oldProtect, &oldProtect);

	AddrAddToList(addr, 1);
}

void _stdcall SafeWrite16(DWORD addr, WORD data) {
	DWORD oldProtect;

	VirtualProtect((void *)addr, 2, PAGE_EXECUTE_READWRITE, &oldProtect);
	*((WORD*)addr) = data;
	VirtualProtect((void *)addr, 2, oldProtect, &oldProtect);

	AddrAddToList(addr, 2);
}

void _stdcall SafeWrite32(DWORD addr, DWORD data) {
	DWORD oldProtect;

	VirtualProtect((void *)addr, 4, PAGE_EXECUTE_READWRITE, &oldProtect);
	*((DWORD*)addr) = data;
	VirtualProtect((void *)addr, 4, oldProtect, &oldProtect);

	AddrAddToList(addr, 4);
}

void _stdcall SafeWriteStr(DWORD addr, const char* data) {
	DWORD oldProtect;
	long len = strlen(data) + 1;

	VirtualProtect((void *)addr, len, PAGE_EXECUTE_READWRITE, &oldProtect);
	strcpy((char *)addr, data);
	VirtualProtect((void *)addr, len, oldProtect, &oldProtect);

	AddrAddToList(addr, len);
}

void HookCall(DWORD addr, void* func) {
	SafeWrite32(addr + 1, (DWORD)func - (addr + 5));

	CheckConflict(addr, 1);
}

void MakeCall(DWORD addr, void* func) {
	SafeWriteFunc(CodeType::Call, addr, func);
}

void MakeCall(DWORD addr, void* func, int len) {
	SafeWriteFunc(CodeType::Call, addr, func, len);
}

void MakeJump(DWORD addr, void* func) {
	SafeWriteFunc(CodeType::Jump, addr, func);
}

void MakeJump(DWORD addr, void* func, int len) {
	SafeWriteFunc(CodeType::Jump, addr, func, len);
}

void HookCalls(void* func, std::initializer_list<DWORD> addrs) {
	for (auto& addr : addrs) {
		HookCall(addr, func);
	}
}

void MakeCalls(void* func, std::initializer_list<DWORD> addrs) {
	for (auto& addr : addrs) {
		MakeCall(addr, func);
	}
}

void SafeMemSet(DWORD addr, BYTE val, int len) {
	DWORD oldProtect;

	VirtualProtect((void *)addr, len, PAGE_EXECUTE_READWRITE, &oldProtect);
	memset((void*)addr, val, len);
	VirtualProtect((void *)addr, len, oldProtect, &oldProtect);

	AddrAddToList(addr, len);
}

void BlockCall(DWORD addr) {
	DWORD oldProtect;

	VirtualProtect((void *)addr, 4, PAGE_EXECUTE_READWRITE, &oldProtect);
	*(DWORD*)addr = 0x00441F0F; // long NOP (0F1F4400-XX)
	VirtualProtect((void *)addr, 4, oldProtect, &oldProtect);

	CheckConflict(addr, 5);
}

}
