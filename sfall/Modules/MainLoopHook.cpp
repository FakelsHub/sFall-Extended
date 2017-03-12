#include "..\FalloutEngine\Fallout2.h"
#include "..\SafeWrite.h"

#include "MainLoopHook.h"

namespace sfall 
{

static Delegate<> onMainLoop;
static Delegate<> onCombatLoop;

void __stdcall MainGameLoopHook2() {
	onMainLoop.invoke();
}

void __stdcall CombatLoopHook2() {
	onCombatLoop.invoke();
}

static void __declspec(naked) MainGameLoopHook() {
	__asm {
		push ebx;
		push ecx;
		push edx;
		call fo::funcoffs::get_input_
		push eax;
		call MainGameLoopHook2;
		pop eax;
		pop edx;
		pop ecx;
		pop ebx;
		retn;
	}
}

static void __declspec(naked) CombatLoopHook() {
	__asm {
		pushad;
		call CombatLoopHook2;
		popad;
		jmp  fo::funcoffs::get_input_
	}
}

void MainLoopHook::init() {
	HookCall(0x480E7B, MainGameLoopHook); //hook the main game loop
	HookCall(0x422845, CombatLoopHook); //hook the combat loop
}

Delegate<>& MainLoopHook::OnMainLoop() {
	return onMainLoop;
}

Delegate<>& MainLoopHook::OnCombatLoop() {
	return onCombatLoop;
}

}