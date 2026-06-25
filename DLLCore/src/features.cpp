#include "features.h"
#include "memory.h"
#include "features/RapidFire.h"
#include "features/ESP.h"
#include <stdexcept>

#include "features/Accuracy.h"
#include "features/Aimbot.h"
#include "features/Assassination.h"
#include "features/BattlePass.h"
#include "features/BotLobby.h"
#include "features/FastRespawn.h"
#include "features/FullCores.h"
#include "features/TeleportToHead.h"
#include "features/Ghost.h"
#include "features/InfiniteAmmo.h"
#include "features/MoveSpeed.h"
#include "features/Players.h"
#include "features/Radar.h"
#include "features/Recoilless.h"
#include "features/VIP.h"

int targetAmmo = 999;
int currentAmmo = 0;

bool isInfiniteHealth = false;
bool isInfiniteAmmo = false;
bool isInfiniteAmmoExpert = false;
bool isRapidFire = false;
bool isAimbot = false;
bool isFullCores = false;
bool isESP = false;
bool isBotRoom = false;
bool isGhost = false;
bool isFury = false;
bool g_showEnemy = true;
bool g_showFriend = false;
bool isBattlePass = false;
bool isVIP = false;
bool isAssassination = false;
bool isRadar = false;
bool isAccuracy = false;
bool isMoveSpeed = false;
bool isRecoilless = false;
bool isFastRespawn = false;

static uint64_t mod_inv(uint64_t a)
{
    int64_t t = 0, newt = 1;
    int64_t r = 0x100000000LL;
    int64_t newr = a;
    while (newr != 0)
    {
        const int64_t q = r / newr;
        int64_t tmp = newt;
        newt = t - q * newt;
        t = tmp;
        tmp = newr;
        newr = r - q * newr;
        r = tmp;
    }
    if (r != 1) throw std::runtime_error("Inverse does not exist");
    if (t < 0) t += 0x100000000LL;
    return static_cast<uint64_t>(t);
}

uint32_t DecryptAmmo1(uint32_t enc)
{
    constexpr uint64_t mask = 55055517ULL;
    static uint64_t inv = mod_inv(mask);
    return static_cast<uint32_t>((static_cast<uint64_t>(enc) * inv) & 0xFFFFFFFFULL);
}

uint32_t DecryptAmmo2(uint32_t enc)
{
    constexpr uint64_t mask = 909146845ULL;
    static uint64_t inv = mod_inv(mask);
    return static_cast<uint32_t>((static_cast<uint64_t>(enc) * inv) & 0xFFFFFFFFULL);
}

uint32_t EncryptAmmo1(uint32_t raw)
{
    constexpr uint64_t mask = 55055517ULL;
    return static_cast<uint32_t>((static_cast<uint64_t>(raw) * mask) & 0xFFFFFFFFULL);
}

uint32_t EncryptAmmo2(uint32_t raw)
{
    constexpr uint64_t mask = 909146845ULL;
    return static_cast<uint32_t>((static_cast<uint64_t>(raw) * mask) & 0xFFFFFFFFULL);
}

namespace Features
{
    void FeaturesTick()
    {
        Aimbot::isEnabled = isAimbot;
        TeleportToHead::isEnabled = isFury;
        Ghost::isEnabled = isGhost;

        if (isAimbot)
            Aimbot::Run();
        else
            Aimbot::UninstallPitchHook();

        if (isESP)
            ESP::Render();

        FeaturesAsm();

        if (isInfiniteAmmo)
        {
            const uintptr_t a1 = Memory::ResolveAddress(PRIMARY_CURRENT_AMMO_1_EXPR);
            if (const uintptr_t a2 = Memory::ResolveAddress(PRIMARY_CURRENT_AMMO_2_EXPR); a1 && a2)
            {
                Memory::WriteInt(a1, EncryptAmmo1(targetAmmo));
                Memory::WriteInt(a2, EncryptAmmo2(targetAmmo));
                currentAmmo = DecryptAmmo1(Memory::ReadInt(a1));
            }
        }
        if (isInfiniteHealth)
        {
            if (const uintptr_t addr = Memory::ResolveAddress(HEALTH_EXPR))
                Memory::WriteFloat(addr, 999999999.0f);
        }
    }

    void FeaturesAsm()
    {
        static bool lastInfiniteAmmo = false;
        if (isInfiniteAmmoExpert != lastInfiniteAmmo)
        {
            lastInfiniteAmmo = isInfiniteAmmoExpert;
            if (isInfiniteAmmoExpert)
            {
                InfiniteAmmo::SetAmmoValue(666);
                InfiniteAmmo::Install();
            }
            else
            {
                InfiniteAmmo::Uninstall();
            }
        }

        static bool lastRapidFire = false;

        if (isRapidFire != lastRapidFire)
        {
            lastRapidFire = isRapidFire;
            if (isRapidFire)
            {
                RapidFire::Install();
            }
            else
            {
                RapidFire::Uninstall();
            }
        }

        static bool lastBotRoom = false;
        if (isBotRoom != lastBotRoom)
        {
            lastBotRoom = isBotRoom;
            if (isBotRoom)
            {
                BotRoom::Install();
            }
            else
            {
                BotRoom::Uninstall();
            }
        }

        static bool lastFullCores = false;
        if (isFullCores != lastFullCores)
        {
            lastFullCores = isFullCores;
            if (isFullCores)
            {
                FullCores::Install();
            }
            else
            {
                FullCores::Uninstall();
            }
        }

        static bool lastGhost = false;
        if (isGhost != lastGhost)
        {
            lastGhost = isGhost;
            if (isGhost)
            {
                Ghost::Install();
            }
            else
            {
                Ghost::Uninstall();
            }
        }

        if (isFury && Ghost::isEnabled)
        {
            std::vector<PlayerInfo> players = GetPlayers();
            TeleportToHead::Run(players);
        }

        static bool lastBattlePass = false;
        if (isBattlePass != lastBattlePass)
        {
            lastBattlePass = isBattlePass;
            if (isBattlePass)
            {
                BattlePass::Install();
            }
            else
            {
                BattlePass::Uninstall();
            }
        }

        static bool lastVIP = false;
        if (isVIP != lastVIP)
        {
            lastVIP = isVIP;
            if (isVIP)
            {
                VIP::Install();
            }
            else
            {
                VIP::Uninstall();
            }
        }

        static bool lastAssassination = false;
        if (isAssassination != lastAssassination)
        {
            lastAssassination = isAssassination;
            if (isAssassination)
            {
                Assassination::Install();
            }
            else
            {
                Assassination::Uninstall();
            }
        }

        static bool lastRadar = false;
        if (isRadar != lastRadar)
        {
            lastRadar = isRadar;
            if (isRadar)
            {
                Radar::Install();
            }
            else
            {
                Radar::Uninstall();
            }
        }

        static bool lastAccuracy = false;
        if (isAccuracy != lastAccuracy)
        {
            lastAccuracy = isAccuracy;
            if (isAccuracy)
            {
                Accuracy::Install();
            }
            else
            {
                Accuracy::Uninstall();
            }
        }

        static bool lastMoveSpeed = false;
        if (isMoveSpeed != lastMoveSpeed)
        {
            lastMoveSpeed = isMoveSpeed;
            if (isMoveSpeed)
            {
                MoveSpeed::Install();
            }
            else
            {
                MoveSpeed::Uninstall();
            }
        }

        static bool lastRecoilless = false;
        if (isRecoilless != lastRecoilless)
        {
            lastRecoilless = isRecoilless;
            if (isRecoilless)
            {
                Recoilless::Install();
            }
            else
            {
                Recoilless::Uninstall();
            }
        }

        static bool lastFastRespawn = false;
        if (isFastRespawn != lastFastRespawn)
        {
            lastFastRespawn = isFastRespawn;
            if (isFastRespawn)
            {
                FastRespawn::Install();
            }
            else
            {
                FastRespawn::Uninstall();
            }
        }
    }
}
