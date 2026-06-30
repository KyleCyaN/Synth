#include "features.h"
#include "memory.h"
#include "features/RapidFire.h"
#include "features/ESP.h"
#include <limits>

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
#include "features/SilentAimbot.h"
#include "features/VIP.h"

constexpr uint64_t MOD = 0x100000000ULL;

constexpr uint64_t mod_inv_const(uint64_t a) {
    uint64_t t = 0, newt = 1;
    uint64_t r = MOD, newr = a;
    while (newr != 0) {
        const uint64_t q = r / newr;
        uint64_t tmp = newt; newt = t - q * newt; t = tmp;
        tmp = newr; newr = r - q * newr; r = tmp;
    }
    return (t >= MOD) ? t + MOD : t;
}

constexpr uint64_t MASK1 = 55055517ULL;
constexpr uint64_t MASK2 = 909146845ULL;

constexpr uint64_t INV1 = mod_inv_const(MASK1);
constexpr uint64_t INV2 = mod_inv_const(MASK2);

inline uint32_t CryptAmmo(uint32_t v, AmmoSlot slot, AmmoOp op) {
    constexpr uint64_t TABLE[2][2] = {
        { MASK1, INV1 }, // SLOT1: ENC, DEC
        { MASK2, INV2 }  // SLOT2: ENC, DEC
    };

    const uint64_t mul = TABLE[slot][op];
    return static_cast<uint32_t>(
        (static_cast<uint64_t>(v) * mul) & 0xFFFFFFFFULL
    );
}

uint32_t targetAmmo = 999;
uint32_t currentAmmo = 0;

bool isInfiniteHealth = false;
bool isInfiniteAmmo = false;
bool isInfiniteAmmoExpert = false;
bool isRapidFire = false;
bool isRapidFireExpert = false;
bool isAimbot = false;
bool isSilentAimbot = false;
bool isFullCores = false;
bool isESP = false;
bool isBotRoom = false;
bool isGhost = false;
bool isFury = false;
bool isShowEnemy = true;
bool isShowFriend = false;
bool isBattlePass = false;
bool isVIP = false;
bool isAssassination = false;
bool isRadar = false;
bool isAccuracy = false;
bool isMoveSpeed = false;
bool isRecoilless = false;
bool isFastRespawn = false;
bool isForceRespawn = false;

template<auto On, auto Off>
void Toggle(bool& cur, bool& last) {
    if (cur != last) {
        last = cur;
        cur ? On() : Off();
    }
}

#define FEATURE(name) \
    static bool _##name{}; \
    Toggle<name::Install, name::Uninstall>(is##name, _##name)

namespace Features {

    void FeaturesByMemory() {
        Aimbot::isEnabled = isAimbot;
        SilentAimbot::isEnabled = isSilentAimbot;
        TeleportToHead::isEnabled = isFury;
        Ghost::isEnabled = isGhost;

        if (isAimbot) {
            Aimbot::Run();
            Aimbot::DrawFovCircle();
        }
        else
            Aimbot::UninstallPitchHook();

        if (isSilentAimbot)
            SilentAimbot::Run();

        if (isESP)
            ESP::Render();

        if (isInfiniteAmmo) {
            const uintptr_t a1 = Memory::ResolveAddress(PRIMARY_CURRENT_AMMO_1_EXPR);
            if (const uintptr_t a2 = Memory::ResolveAddress(PRIMARY_CURRENT_AMMO_2_EXPR); a1 && a2) {
                Memory::WriteInt(a1, CryptAmmo(targetAmmo, SLOT1, ENC));
                Memory::WriteInt(a2, CryptAmmo(targetAmmo, SLOT2, ENC));
                currentAmmo = CryptAmmo(Memory::ReadInt(a1), SLOT1, DEC);
            }
        }

        if (isInfiniteHealth) {
            if (const uintptr_t addr = Memory::ResolveAddress(HEALTH_EXPR)) {
                Memory::WriteFloat(
                    addr,
                    static_cast<float>(std::numeric_limits<int32_t>::max())
                );
            }
        }
    }

    void FeaturesByAssembly() {
        static bool lastInfiniteAmmoExpert = false;
        if (isInfiniteAmmoExpert != lastInfiniteAmmoExpert) {
            lastInfiniteAmmoExpert = isInfiniteAmmoExpert;
            if (isInfiniteAmmoExpert) {
                InfiniteAmmo::SetAmmoValue(666);
                InfiniteAmmo::Install();
            } else {
                InfiniteAmmo::Uninstall();
            }
        }

        if (isFury && Ghost::isEnabled) {
            auto players = GetPlayers();
            TeleportToHead::Run(players);
        }

        FEATURE(SilentAimbot);
        FEATURE(RapidFire);
        FEATURE(BotRoom);
        FEATURE(FullCores);
        FEATURE(Ghost);
        FEATURE(BattlePass);
        FEATURE(VIP);
        FEATURE(Assassination);
        FEATURE(Radar);
        FEATURE(Accuracy);
        FEATURE(MoveSpeed);
        FEATURE(Recoilless);
        FEATURE(FastRespawn);
    }

}