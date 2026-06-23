#pragma once
#include <cstdint>
#include <string>
#include <windows.h>

#define HEALTH_EXPR "WindowsEntryPoint.Windows_W10.exe+01C77388->0x308->0x90->0x1A8->0x28"
#define PRIMARY_CURRENT_AMMO_1_EXPR "WindowsEntryPoint.Windows_W10.exe+01C77388->0x308->0x140->0x8->0x70->0x0"
#define PRIMARY_CURRENT_AMMO_2_EXPR "WindowsEntryPoint.Windows_W10.exe+01C77388->0x308->0x140->0x8->0x68"
#define PRIMARY_TOTAL_AMMO_1_EXPR "WindowsEntryPoint.Windows_W10.exe+01C77388->0x308->0x140->0x8->0x80->0x0"
#define PRIMARY_TOTAL_AMMO_2_EXPR "WindowsEntryPoint.Windows_W10.exe+01C77388->0x308->0x140->0x8->0x78"
#define SECONDARY_CURRENT_AMMO_1_EXPR "WindowsEntryPoint.Windows_W10.exe+01C77388->0x308->0x140->0x28->0x70"
#define SECONDARY_CURRENT_AMMO_2_EXPR "WindowsEntryPoint.Windows_W10.exe+01C77388->0x308->0x140->0x28->0x68"
#define SECONDARY_TOTAL_AMMO_1_EXPR "WindowsEntryPoint.Windows_W10.exe+01C77388->0x308->0x140->0x28->0x80->0x0"
#define SECONDARY_TOTAL_AMMO_2_EXPR "WindowsEntryPoint.Windows_W10.exe+01C77388->0x308->0x140->0x28->0x78"
#define FLASH_BANG_1_EXPR "WindowsEntryPoint.Windows_W10.exe+01C77388->0x308->0x1B8->0x300->0x140"
#define FLASH_BANG_2_EXPR "WindowsEntryPoint.Windows_W10.exe+01C77388->0x308->0x1B8->0xE0"
#define CONCUSSION_GRENADE_1_EXPR "WindowsEntryPoint.Windows_W10.exe+01C77388->0x308->0x1B8->0x40->0x0"
#define CONCUSSION_GRENADE_2_EXPR "WindowsEntryPoint.Windows_W10.exe+01C77388->0x308->0x1B8->0x38"
#define IMPACT_GRENADE_1_EXPR "WindowsEntryPoint.Windows_W10.exe+01C77388->0x308->0x1B8->0x50->0x68"
#define IMPACT_GRENADE_2_EXPR "WindowsEntryPoint.Windows_W10.exe+01C77388->0x308->0x1B8->0x118"
#define STICKY_MINE_1_EXPR "WindowsEntryPoint.Windows_W10.exe+01C77388->0x308->0x1B8->0x158->0x0"
#define STICKY_MINE_2_EXPR "WindowsEntryPoint.Windows_W10.exe+01C77388->0x308->0x1B8->0x150"
#define CORE_HIGH_POWER_EXPR "WindowsEntryPoint.Windows_W10.exe+01C749B8->0x380->0xF30"
#define CORE_HAIR_TRIGGER_EXPR "WindowsEntryPoint.Windows_W10.exe+01C749B8->0x380->0xEB8"
#define CORE_AP_EXPR "WindowsEntryPoint.Windows_W10.exe+01C749B8->0x380->0xE40"
#define CORE_SCORCHER_DAGAME_EXPR "WindowsEntryPoint.Windows_W10.exe+01C749B8->0x380->0x750"
#define CORE_SCORCHER_KEEP_TIME_EXPR "WindowsEntryPoint.Windows_W10.exe+01C749B8->0x380->0x754"
#define CORE_SIXTH_SENSE_RANGE_EXPR "WindowsEntryPoint.Windows_W10.exe+01C749B8->0x380->0x2E8"
#define CORE_SEER_KEEP_TIME_EXPR "WindowsEntryPoint.Windows_W10.exe+01C749B8->0x380->0x558"
#define CORE_SEER_COLD_TIME_EXPR "WindowsEntryPoint.Windows_W10.exe+01C749B8->0x380->0x55C"
#define ENEMY_LIST_EXPR "WindowsEntryPoint.Windows_W10.exe+1C77500->0x60->0x338"
#define ENEMY_POS_X_EXPR "WindowsEntryPoint.Windows_W10.exe+01C74B98->0x0->0x0->0x338->0x58->0x58"
#define ENEMY_POS_Y_EXPR "WindowsEntryPoint.Windows_W10.exe+01C74B98->0x0->0x0->0x338->0x58->0x5C"
#define ENEMY_POS_Z_EXPR "WindowsEntryPoint.Windows_W10.exe+01C74B98->0x0->0x0->0x338->0x58->0x60"
#define ENEMY_HEALTH_EXPR "WindowsEntryPoint.Windows_W10.exe+01C74B98->0x0->0x0->0x338->0x58->0x1A8->0x28"
#define ENEMY_BONE_ENTITY_EXPR "WindowsEntryPoint.Windows_W10.exe+01C77448->0x78->0x18"
#define ENEMY_BONE_HEAD_X_EXPR "WindowsEntryPoint.Windows_W10.exe+01C77448->0x78->0x18->0x130->0x108->0x10->0x64"
#define ENEMY_BONE_HEAD_Y_EXPR "WindowsEntryPoint.Windows_W10.exe+01C77448->0x78->0x18->0x130->0x108->0x10->0x68"
#define ENEMY_BONE_HEAD_Z_EXPR "WindowsEntryPoint.Windows_W10.exe+01C77448->0x78->0x18->0x130->0x108->0x10->0x6C"
#define PLAYER_TEAM_EXPR "WindowsEntryPoint.Windows_W10.exe+01C775E8"
#define VIEW_MATRIX_EXPR "WindowsEntryPoint.Windows_W10.exe+01C77388->0x308->0x90->0xB8->0x118->0x190->0x144"
#define LOCAL_PLAYER_POSITION_EXPR "WindowsEntryPoint.Windows_W10.exe+01C77388->0x308->0x90->0x160->0x48->0x8->0x58"
#define FPS_YAW_EXPR "WindowsEntryPoint.Windows_W10.exe+01C77388->0x308->0x90->0x6C"
#define FPS_PITCH_EXPR "WindowsEntryPoint.Windows_W10.exe+01C77388->0x308->0x90->0x160->0x38->0x630"
#define TEAM_FRIEND_EXPR "WindowsEntryPoint.Windows_W10.exe+01C77388->0x308->0x90->0xA0"

extern int targetAmmo;
extern int currentAmmo;

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

uint32_t DecryptAmmo1(uint32_t encryptedValue);
uint32_t DecryptAmmo2(uint32_t encryptedValue);
uint32_t EncryptAmmo1(uint32_t raw);
uint32_t EncryptAmmo2(uint32_t raw);

namespace Features
{
	void FeaturesTick();
	void FeaturesAsm();
}
