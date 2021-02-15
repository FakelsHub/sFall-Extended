/*
 *    sfall
 *    Copyright (C) 2008 - 2021 Timeslip and sfall team
 *
 */

#include "..\FalloutEngine\Fallout2.h"

#include "..\Modules\HookScripts\MiscHs.h"

#include "objects.h"

namespace game
{

namespace sf = sfall;

// Implementation of is_within_perception_ engine function with the HOOK_WITHINPERCEPTION hook 
long Objects::is_within_perception(fo::GameObject* watcher, fo::GameObject* target, long hookType) {
	return sf::PerceptionRangeHook_Invoke(watcher, target, hookType, fo::func::is_within_perception(watcher, target));
}

void Objects::init() {

}

}