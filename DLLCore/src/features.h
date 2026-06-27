#pragma once
#include <cstdint>
#include <string>
#include <windows.h>

#define LOCAL_PLAYER_BASE "WindowsEntryPoint.Windows_W10.exe+01CF36B8"
#define CORE_BASE "WindowsEntryPoint.Windows_W10.exe+01CF3BB8"
#define MULTI_PLAYER_BASE "WindowsEntryPoint.Windows_W10.exe+01CF37E0"
#define MULTI_COLLISION_BASE "WindowsEntryPoint.Windows_W10.exe+01CF3740"

#define HEALTH_EXPR LOCAL_PLAYER_BASE "->0x308->0x90->0x1A8->0x28"

//MOUSE STATUS
#define MOUSE_STATUS_EXPR "WindowsEntryPoint.Windows_W10.exe+01CF5D08->0x268"

//WEAPONS
#define PRIMARY_CURRENT_AMMO_1_EXPR LOCAL_PLAYER_BASE "->0x308->0x140->0x8->0x70->0x0"
#define PRIMARY_CURRENT_AMMO_2_EXPR LOCAL_PLAYER_BASE "->0x308->0x140->0x8->0x68"
#define PRIMARY_TOTAL_AMMO_1_EXPR LOCAL_PLAYER_BASE "->0x308->0x140->0x8->0x80->0x0"
#define PRIMARY_TOTAL_AMMO_2_EXPR LOCAL_PLAYER_BASE "->0x308->0x140->0x8->0x78"
#define SECONDARY_CURRENT_AMMO_1_EXPR LOCAL_PLAYER_BASE "->0x308->0x140->0x28->0x70"
#define SECONDARY_CURRENT_AMMO_2_EXPR LOCAL_PLAYER_BASE "->0x308->0x140->0x28->0x68"
#define SECONDARY_TOTAL_AMMO_1_EXPR LOCAL_PLAYER_BASE "->0x308->0x140->0x28->0x80->0x0"
#define SECONDARY_TOTAL_AMMO_2_EXPR LOCAL_PLAYER_BASE "->0x308->0x140->0x28->0x78"
#define FLASH_BANG_1_EXPR LOCAL_PLAYER_BASE "->0x308->0x1B8->0x300->0x140"
#define FLASH_BANG_2_EXPR LOCAL_PLAYER_BASE "->0x308->0x1B8->0xE0"
#define CONCUSSION_GRENADE_1_EXPR LOCAL_PLAYER_BASE "->0x308->0x1B8->0x40->0x0"
#define CONCUSSION_GRENADE_2_EXPR LOCAL_PLAYER_BASE "->0x308->0x1B8->0x38"
#define IMPACT_GRENADE_1_EXPR LOCAL_PLAYER_BASE "->0x308->0x1B8->0x50->0x68"
#define IMPACT_GRENADE_2_EXPR LOCAL_PLAYER_BASE "->0x308->0x1B8->0x118"
#define STICKY_MINE_1_EXPR LOCAL_PLAYER_BASE "->0x308->0x1B8->0x158->0x0"
#define STICKY_MINE_2_EXPR LOCAL_PLAYER_BASE "->0x308->0x1B8->0x150"

//CORES
#define CORE_HIGH_POWER_EXPR CORE_BASE "->0x380->0xF30"
#define CORE_HAIR_TRIGGER_EXPR CORE_BASE "->0x380->0xEB8"
#define CORE_AP_EXPR CORE_BASE "->0x380->0xE40"
#define CORE_SCORCHER_DAGAME_EXPR CORE_BASE "->0x380->0x750"
#define CORE_SCORCHER_KEEP_TIME_EXPR CORE_BASE "->0x380->0x754"
#define CORE_SIXTH_SENSE_RANGE_EXPR CORE_BASE "->0x380->0x2E8"
#define CORE_SEER_KEEP_TIME_EXPR CORE_BASE "->0x380->0x558"
#define CORE_SEER_COLD_TIME_EXPR CORE_BASE "->0x380->0x55C"


#define ENEMY_LIST_EXPR MULTI_PLAYER_BASE "->0x60->0x338"
#define ENEMY_BONE_ENTITY_EXPR MULTI_COLLISION_BASE "->0x78->0x18"

#define VIEW_MATRIX_EXPR LOCAL_PLAYER_BASE "->0x308->0x90->0xB8->0x118->0x190->0x144"
#define LOCAL_PLAYER_POSITION_EXPR LOCAL_PLAYER_BASE "->0x308->0x90->0x160->0x48->0x8->0x58"
#define FPS_YAW_EXPR LOCAL_PLAYER_BASE "->0x308->0x90->0x6C"
#define FPS_PITCH_EXPR LOCAL_PLAYER_BASE "->0x308->0x90->0x160->0x38->0x630"
#define TEAM_FRIEND_EXPR LOCAL_PLAYER_BASE "->0x308->0x90->0xA0"

extern uint32_t targetAmmo;
extern uint32_t currentAmmo;

extern bool isInfiniteHealth;
extern bool isInfiniteAmmo;
extern bool isInfiniteAmmoExpert;
extern bool isRapidFire;
extern bool isAimbot;
extern bool isESP;
extern bool isBotRoom;
extern bool isFullCores;
extern bool isGhost;
extern bool isFury;
extern bool g_showEnemy;
extern bool g_showFriend;
extern bool isBattlePass;
extern bool isVIP;
extern bool isAssassination;
extern bool isRadar;
extern bool isAccuracy;
extern bool isMoveSpeed;
extern bool isRecoilless;
extern bool isFastRespawn;

extern uintptr_t g_ModuleBase;
extern DWORD g_GlobalPointerOffset;

enum AmmoSlot { SLOT1 = 0, SLOT2 = 1 };
enum AmmoOp  { ENC = 0, DEC = 1 };

uint32_t CryptAmmo(uint32_t value, AmmoSlot slot, AmmoOp op);

namespace Features
{
	void FeaturesByMemory();
	void FeaturesByAssembly();
}
