#include "TeleportToHead.h"
#include "Ghost.h"
#include "features.h"
#include "memory.h"
#include <cmath>

#include "Bones.h"

bool  TeleportToHead::isEnabled = false;
float  TeleportToHead::g_teleportToHeadTargetPos[3] = {0, 0, 0};
bool  TeleportToHead::g_teleportToHeadActive = false;
int  TeleportToHead::g_teleportToHeadTargetIndex = -1;

bool  TeleportToHead::InitFunctions() { return true; }

static uintptr_t GetPointer3()
{
    uintptr_t addr = Memory::ResolveAddress(FPS_YAW_EXPR);
    return addr ? (addr - 0x6C) : 0;
}

static int GetLocalTeam()
{
    const uintptr_t addr = Memory::ResolveAddress(TEAM_FRIEND_EXPR);
    return addr ? *reinterpret_cast<int *>(addr) : -1;
}

void TeleportToHead::Run(std::vector<PlayerInfo>& players)
{
    g_teleportToHeadActive = false;
    g_teleportToHeadTargetIndex = -1;

    if (!isEnabled || !Ghost::isEnabled) return;

    static bool initialized = false;
    if (!initialized)
    {
        InitFunctions();
        initialized = true;
    }

    std::vector<BoneInfo> allBones;
    CollectAllBones(allBones);
    DistributeBonesToPlayers(players, allBones);

    uintptr_t pointer3 = GetPointer3();
    if (!pointer3) return;
    int localTeam = GetLocalTeam();

    const PlayerInfo* best = nullptr;
    BoneInfo bestHead;
    float bestDist = FLT_MAX;

    for (const auto& p : players)
    {
        if (p.HP <= 34.0f) continue;
        if (p.Team == localTeam) continue;

        BoneInfo head;
        bool found = false;
        for (const auto& bone : p.bones)
        {
            if (bone.boneName == "Bip01_Head")
            {
                head = bone;
                found = true;
                break;
            }
        }
        if (!found) continue;

        float dx = head.x - Memory::ReadFloat(pointer3 + 0x58);
        float dy = head.y - Memory::ReadFloat(pointer3 + 0x5C);
        float dz = head.z - Memory::ReadFloat(pointer3 + 0x60);
        float dist = dx * dx + dy * dy + dz * dz;

        if (dist < bestDist)
        {
            bestDist = dist;
            best = &p;
            bestHead = head;
        }
    }

    if (!best) return;

    Memory::WriteFloat(pointer3 + 0x58, bestHead.x);
    Memory::WriteFloat(pointer3 + 0x5C, bestHead.y - 0.3f);
    Memory::WriteFloat(pointer3 + 0x60, bestHead.z - 1.5f);

    g_teleportToHeadTargetPos[0] = bestHead.x;
    g_teleportToHeadTargetPos[1] = bestHead.y - 0.3f;
    g_teleportToHeadTargetPos[2] = bestHead.z - 1.5f;
    g_teleportToHeadActive = true;
}

void  TeleportToHead::Install()
{
}

void  TeleportToHead::Uninstall() { g_teleportToHeadActive = false; }
