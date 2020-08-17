#include "..\..\FalloutEngine\Fallout2.h"

#include "Common.h"

namespace sfall
{

constexpr int maxArgs  = 16;
constexpr int maxDepth = 8;

struct {
	DWORD hookID;
	DWORD argCount;
	DWORD cArg;
	DWORD cRet;
	DWORD cRetTmp;
	DWORD oldArgs[maxArgs];
	DWORD oldRets[maxRets];
} savedArgs[maxDepth];

static DWORD callDepth;
static DWORD currentRunHook = -1;

DWORD args[maxArgs]; // current hook arguments
DWORD rets[maxRets]; // current hook return values

DWORD argCount;
DWORD cArg;    // how many arguments were taken by current hook script
DWORD cRet;    // how many return values were set by current hook script
DWORD cRetTmp; // how many return values were set by specific hook script (when using register_hook)

std::vector<HookScript> hooks[numHooks];

void LoadHookScript(const char* name, int id) {
	//if (id >= numHooks || IsGameScript(name)) return;
	char filePath[MAX_PATH];

	bool customPath = !HookScripts::hookScriptPathFmt.empty();
	if (!customPath) {
		sprintf(filePath, "scripts\\%s.int", name);
	} else {
		sprintf_s(filePath, HookScripts::hookScriptPathFmt.c_str(), name);
		name = filePath;
	}

	bool hookIsLoaded = LoadHookScriptFile(filePath, name, id, customPath);
	if (hookIsLoaded || (HookScripts::injectAllHooks && id != HOOK_SUBCOMBATDAMAGE)) {
		HookScripts::InjectingHook(id); // inject hook to engine code

		if (!hookIsLoaded) return;
		HookFile hookFile = { id, filePath, name };
		HookScripts::hookScriptFilesList.push_back(hookFile);
	}
}

bool LoadHookScriptFile(std::string filePath, const char* name, int id, bool fullPath) {
	ScriptProgram prog;
	if (fo::func::db_access(filePath.c_str())) {
		dlog(">" + filePath, DL_HOOK);
		LoadScriptProgram(prog, name, fullPath);
		if (prog.ptr) {
			dlogr(" Done", DL_HOOK);
			HookScript hook;
			hook.prog = prog;
			hook.callback = -1;
			hook.isGlobalScript = 0;
			hooks[id].push_back(hook);
			ScriptExtender::AddProgramToMap(prog);
		} else {
			dlogr(" Error!", DL_HOOK);
		}
	}
	return (prog.ptr != nullptr);
}

// List of hooks that are not allowed to be called recursively
static bool CheckRecursiveHooks(DWORD hook) {
	if (hook == currentRunHook) {
		switch (hook) {
		case HOOK_SETGLOBALVAR:
		case HOOK_SETLIGHTING:
			return true;
		}
	}
	return false;
}

void __stdcall BeginHook() {
	if (callDepth && callDepth <= maxDepth) {
		// save all values of the current hook if another hook was called during the execution of the current hook
		int cDepth = callDepth - 1;
		savedArgs[cDepth].hookID = currentRunHook;
		savedArgs[cDepth].argCount = argCount;                                     // number of arguments of the current hook
		savedArgs[cDepth].cArg = cArg;                                             // current count of taken arguments
		savedArgs[cDepth].cRet = cRet;                                             // number of return values for the current hook
		savedArgs[cDepth].cRetTmp = cRetTmp;
		memcpy(&savedArgs[cDepth].oldArgs, args, argCount * sizeof(DWORD));        // values of the arguments
		if (cRet) memcpy(&savedArgs[cDepth].oldRets, rets, cRet * sizeof(DWORD));  // return values

		///devlog_f("\nSaved cArgs/cRet: %d / %d(%d)\n", DL_HOOK, savedArgs[cDepth].argCount, savedArgs[cDepth].cRet, cRetTmp);
		///for (unsigned int i = 0; i < maxArgs; i++)
		///	devlog_f("Saved Args/Rets: %d / %d\n", DL_HOOK, savedArgs[cDepth].oldArgs[i], ((i < maxRets) ? savedArgs[cDepth].oldRets[i] : -1));
	}
	callDepth++;

	devlog_f("Begin running hook, current depth: %d, current executable hook: %d\n", DL_HOOK, callDepth, currentRunHook);
}

static void __stdcall RunSpecificHookScript(HookScript *hook) {
	cArg = 0;
	cRetTmp = 0;
	if (hook->callback != -1) {
		fo::func::executeProcedure(hook->prog.ptr, hook->callback);
	} else {
		hook->callback = RunScriptStartProc(&hook->prog); // run start
	}
}

void __stdcall RunHookScript(DWORD hook) {
	cRet = 0;
	if (!hooks[hook].empty()) {
		if (callDepth > 1) {
			if (CheckRecursiveHooks(hook) || callDepth > 8) {
				fo::func::debug_printf("\n[SFALL] The hook ID: %d cannot be executed.", hook);
				dlog_f("The hook %d cannot be executed due to exceeded depth limit or recursive calls\n", DL_MAIN, hook);
				return;
			}
		}
		currentRunHook = hook;
		size_t hooksCount = hooks[hook].size();
		if (isDebug) dlogh("Running hook %d, which has %0d entries attached, depth: %d\n", hook, hooksCount, callDepth);
		for (int i = hooksCount - 1; i >= 0; i--) {
			RunSpecificHookScript(&hooks[hook][i]);

			///devlog_f("> Hook: %d, script entry: %d done\n", DL_HOOK, hook, i);
			///devlog_f("> Check cArg/cRet: %d / %d(%d)\n", DL_HOOK, cArg, cRet, cRetTmp);
			///for (unsigned int i = 0; i < maxArgs; i++)
			///	devlog_f("> Check Args/Rets: %d / %d\n", DL_HOOK, args[i], ((i < maxRets) ? rets[i] : -1));
		}
	} else {
		cArg = 0; // for what purpose is it here?

		devlog_f(">>> Try running hook ID: %d\n", DL_HOOK, hook);
	}
}

void __stdcall EndHook() {
	devlog_f("Ending running hook %d, current depth: %d\n", DL_HOOK, currentRunHook, callDepth);

	callDepth--;
	if (callDepth) {
		if (callDepth <= maxDepth) {
			// restore all saved values of the previous hook
			int cDepth = callDepth - 1;
			currentRunHook = savedArgs[cDepth].hookID;
			argCount = savedArgs[cDepth].argCount;
			cArg = savedArgs[cDepth].cArg;
			cRet = savedArgs[cDepth].cRet;
			cRetTmp = savedArgs[cDepth].cRetTmp;  // also restore current count of the number of return values
			memcpy(args, &savedArgs[cDepth].oldArgs, argCount * sizeof(DWORD));
			if (cRet) memcpy(rets, &savedArgs[cDepth].oldRets, cRet * sizeof(DWORD));

			///devlog_f("Restored cArgs/cRet: %d / %d(%d)\n", DL_HOOK, argCount, cRet, cRetTmp);
			///for (unsigned int i = 0; i < maxArgs; i++)
			///	devlog_f("Restored Args/Rets: %d / %d\n", DL_HOOK, args[i], ((i < maxRets) ? rets[i] : -1));
		}
	} else {
		currentRunHook = -1;
	}
}

}
