/*
* sfall
* Copyright (C) 2008-2016 The sfall team
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <Windows.h>

#include "Enums.h"

namespace fo
{

/******************************************************************************/
/* FALLOUT2.EXE structs should be placed here  */
/******************************************************************************/

// TODO: make consistent naming for all FO structs

struct GameObject;
struct Program;
struct ScriptInstance;

/*   26 */
#pragma pack(push, 1)
struct TInvenRec {
	GameObject *object;
	long count;
};
#pragma pack(pop)

// Game objects (items, critters, etc.), including those stored in inventories.
#pragma pack(push, 1)
struct GameObject {
	long id;
	long tile;
	long x;
	long y;
	long sx;
	long sy;
	long frm;
	long rotation;
	long artFid;
	long flags;
	long elevation;
	long invenSize;
	long invenMax;
	TInvenRec *invenTable;
	union {
		struct {
			char gap_38[4];
			// for weapons - ammo in magazine, for ammo - amount of ammo in last ammo pack
			long charges;
			// current type of ammo loaded in magazine
			long ammoPid;
			char gap_44[32];
		} item;
		struct {
			long reaction;
			// 1 - combat, 2 - enemies out of sight, 4 - running away
			long combatState;
			// aka action points
			long movePoints;
			long damageFlags;
			long damageLastTurn;
			long aiPacket;
			long teamNum;
			long whoHitMe;
			long health;
			long rads;
			long poison;
		} critter;
	};
	long pid;
	long cid;
	long lightDistance;
	long lightIntensity;
	char outline[4];
	long scriptId;
	GameObject* owner;
	long scriptIndex;

	inline char type() {
		return pid >> 24;
	}
};
#pragma pack(pop)

// Results of compute_attack_() function.
#pragma pack(push, 1)
struct ComputeAttackResult {
	GameObject* attacker;
	long hitMode;
	GameObject* weapon;
	long field_C;
	long attackerDamage;
	long attackerFlags;
	long numRounds;
	long message;
	GameObject* target;
	long targetTile;
	long bodyPart;
	long damage;
	long flags;
	long knockbackValue;
	GameObject* mainTarget;
	long numExtras;
	GameObject* extraTarget[6];
	long extraBodyPart[6];
	long extraDamage[6];
	long extraFlags[6];
	long extraKnockbackValue[6];
};
#pragma pack(pop)

// Script instance attached to an object or tile (spatial script).
#pragma pack(push, 1)
struct ScriptInstance {
	long id;
	long next;
	// first 3 bits - elevation, rest - tile number
	long elevationAndTile;
	long spatialRadius;
	long flags;
	long scriptIdx;
	Program *program;
	long selfObjectId;
	long localVarOffset;
	long numLocalVars;
	long returnValue;
	long action;
	long fixedParam;
	GameObject *selfObject;
	GameObject *sourceObject;
	GameObject *targetObject;
	long actionNum;
	long scriptOverrides;
	char gap_48[4];
	long howMuch;
	char gap_50[4];
	long procedureTable[28];
};
#pragma pack(pop)


/*   25 */
#pragma pack(push, 1)
struct Program {
	const char* fileName;
	long *codeStackPtr;
	char gap_8[8];
	long *codePtr;
	long field_14;
	char gap_18[4];
	long *dStackPtr;
	long *aStackPtr;
	long *dStackOffs;
	long *aStackOffs;
	char gap_2C[4];
	long *stringRefPtr;
	char gap_34[4];
	long *procTablePtr;
};
#pragma pack(pop)

struct ItemButtonItem {
	GameObject* item;
	long flags;
	long primaryAttack;
	long secondaryAttack;
	long mode;
	long fid;
};

// When gained, the perk increases Stat by StatMod, which may be negative. All other perk effects come from being
// specifically checked for by scripts or the engine. If a primary stat requirement is negative, that stat must be
// below the value specified (e.g., -7 indicates a stat must be less than 7). Operator is only non-zero when there
// are two skill requirements. If set to 1, only one of those requirements must be met; if set to 2, both must be met.
struct PerkInfo {
	const char* name;
	const char* description;
	long image;
	long ranks;
	long levelMin;
	long stat;
	long statMod;
	long skill1;
	long skill1Min;
	long skillOperator;
	long skill2;
	long skill2Min;
	long strengthMin;
	long perceptionMin;
	long enduranceMin;
	long charismaMin;
	long intelligenceMin;
	long agilityMin;
	long luckMin;
};

struct DbFile {
	long fileType;
	void* handle;
};

struct ElevatorExit {
	long id;
	long elevation;
	long tile;
};

#pragma pack(push, 1)
struct Frame {
	long id;			//0x00
	long unused;		//0x04
	short frames;		//0x08
	short xshift[6];		//0x0a
	short yshift[6];		//0x16
	long framestart[6];//0x22
	long size;			//0x3a
	short width;			//0x3e
	short height;		//0x40
	long frmSize;		//0x42
	short xoffset;		//0x46
	short yoffset;		//0x48
	BYTE pixels[80 * 36];	//0x4a
};
#pragma pack(pop)

struct MessageNode {
	long number;
	long flags;
	char* audio;
	char* message;

	MessageNode() {
		number = 0;
		flags = 0;
		audio = nullptr;
		message = nullptr;
	}
};

//for holding msg array
typedef struct MessageList {
	long numMsgs;
	MessageNode *nodes;

	MessageList() {
		nodes = nullptr;
		numMsgs = 0;
	}
} MessageList;


struct Art {
	long flags;
	char path[16];
	char* names;
	long d18;
	long total;
};

struct CritInfo {
	union {
		struct {
			// This is divided by 2, so a value of 3 does 1.5x damage, and 8 does 4x damage.
			long damageMult;
			// This is a flag bit field (DAM_*) controlling what effects the critical causes.
			long effectFlags;
			// This makes a check against a (SPECIAL) stat. Values of 2 (endurance), 5 (agility), and 6 (luck) are used, but other stats will probably work as well. A value of -1 indicates that no check is to be made.
			long statCheck;
			// Affects the outcome of the stat check, if one is made. Positive values make it easier to pass the check, and negative ones make it harder.
			long statMod;
			// Another bit field, using the same values as EffectFlags. If the stat check is failed, these are applied in addition to the earlier ones.
			long failureEffect;
			// The message to show when this critical occurs, taken from combat.msg .
			long message;
			// Shown instead of Message if the stat check is failed.
			long failMessage;
		};
		long values[7];
	};
};

#pragma pack(push, 1)
struct SkillInfo
{
	const char* name;
	const char* description;
	long attr;
	long image;
	long base;
	long statMulti;
	long statA;
	long statB;
	long skillPointMulti;
	// Default experience for using the skill: 25 for Lockpick, Steal, Traps, and First Aid, 50 for Doctor, and 100 for Outdoorsman.
	long experience;
	// 1 for Lockpick, Steal, Traps; 0 otherwise
	long f;
};
#pragma pack(pop)

struct StatInfo {
	const char* dame;
	const char* description;
	long image;
	long minValue;
	long maxValue;
	long defaultValue;
};

struct TraitInfo {
	const char* name;
	const char* description;
	long image;
};

//fallout2 path node structure
struct PathNode {
	char* path;
	void* pDat;
	long isDat;
	PathNode* next;
};

struct PremadeChar {
	char path[20];
	DWORD fid;
	char unknown[20];
};

// In-memory PROTO structure, not the same as PRO file format.
struct Proto {
	long pid;
	long messageNum;
	long fid;
	// range 0-8 in hexes
	long lightDistance;
	// range 0 - 65536
	long lightIntensity;
	union {
		struct Tile {
			long scriptId;
			Material material;
		} tile;

		struct Item {
			long flags;
			long flagsExt;
			// 0x0Y00XXXX: Y - script type (0=s_system, 1=s_spatial, 2=s_time, 3=s_item, 4=s_critter); XXXX - number in scripts.lst. -1 means no script.
			long scriptId;
			ItemType type;

			union {
				struct Weapon {
					long animationCode;
					long minDamage;
					long maxDamage;
					long damageType;
					long maxRange[2];
					long projectilePid;
					long minStrength;
					long movePointCost[2];
					long critFailTable;
					long perk;
					long burstRounds;
					long caliber;
					long ammoPid;
					long maxAmmo;
					// shot sound ID
					long soundId;
					long gap_68;
				} weapon;

				struct Ammo {
					long caliber;
					long packSize;
					long acAdjust;
					long drAdjust;
					long damageMult;
					long damageDiv;
					char gap_3c[48];
				} ammo;

				struct Armor {
					long armorClass;
					// for each DamageType
					long damageResistance[7];
					// for each DamageType
					long damageThreshold[7];
					long perk;
					long maleFid;
					long femaleFid;
				} armor;

				struct Container {
					// container size capacity (not weight)
					long maxSize;
					// 1 - has use animation, 0 - no animation
					long openFlags;
				} container;

				struct Drug {
					long stats[3];
					long immediateEffect[3];
					struct DelayedEffect {
						// delay for the effect
						long duration;
						// effect amount for each stat
						long effect[3];
					} delayed[2];
					long addictionRate;
					long addictionEffect;
					long addictionOnset;
					char gap_68[4];
				} drug;

				struct Misc {
					long powerPid;
					long powerType;
					long maxCharges;
				} misc;

				struct Key {
					long keyCode;
				} key;
			};
			Material material; // should be at 0x6C
			long size;
			long weight;
			long cost;
			long inventoryFid;
			BYTE soundId;
		} item;

		struct Critter {
			typedef struct {
				long strength;
				long perception;
				long endurance;
				long charisma;
				long intelligence;
				long agility;
				long luck;
				long health;
				// max move points (action points)
				long movePoints;
				long armorClass;
				// not used by engine
				long unarmedDamage;
				long meleeDamage;
				long carryWeight;
				long sequence;
				long healingRate;
				long criticalChance;
				long betterCriticals;
				// for each DamageType
				long damageThreshold[7];
				// for each DamageType
				long damageResistance[7];
				long radiationResistance;
				long poisonResistance;
				long age;
				long gender;
			} Stats;

			long flags;
			long flagsExt;
			long scriptId;
			long critterFlags;

			Stats base;
			Stats bonus;

			long skills[SKILL_count];

			long bodyType;
			long experience;
			long killType;
			long damageType;
			long headFid;
			long aiPacket;
			long teamNum;
		} critter;

		struct Scenery {
			long flags;
			long flagsExt;
			long scriptId;
			ScenerySubType type;
			union {
				struct Door {
					long openFlags;
					long keyCode;
				} door;
				struct Stairs {
					long elevationAndTile;
					long mapId;
				} stairs;
				struct Elevator {
					long id;
					long level;
				} elevator;
			};
			Material material;
			char gap_30[4];
			BYTE soundId;
		} scenery;

		struct Wall {
			long flags;
			long flagsExt;
			long scriptId;
			Material material;
		} wall;

		struct Misc {
			long flags;
			long flagsExt;
		} misc;
	};
};

struct ScriptListInfoItem {
	char fileName[16];
	long numLocalVars;
};

//for holding window info
struct Window {
	long ref;
	long flags;
	RECT wRect;
	long width;
	long height;
	long clearColour;
	long unknown2;
	long unknown3;
	BYTE *surface; // bytes frame data ref to palette
	long buttonListP;
	long unknown5;//buttonptr?
	long unknown6;
	long unknown7;
	long drawFuncP;
};

}