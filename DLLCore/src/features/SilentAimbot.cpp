#include "SilentAimbot.h"
#include "memory.h"
#include "features.h"
#include "Bones.h"
#include "imgui.h"
#include "ObstacleDetection.h"
#include "Players.h"

#include <Windows.h>
#include <cmath>
#include <cstdint>

#include "utils/utils.h"

static constexpr uintptr_t HOOK_OFFSET = 0x2BA214;
static constexpr size_t HOOK_SIZE = 9;

static uintptr_t g_ModuleBase = 0;
static bool g_Installed = false;
static uintptr_t g_HookAddr = 0;
static BYTE g_OrigBytes[HOOK_SIZE] = {0};
static LPVOID g_Shellcode = nullptr;

float SilentAimbot::g_silentFov = 300.0f;

volatile LONG g_HookCallCount = 0;
volatile LONG g_HookRedirectCount = 0;

bool SilentAimbot::isEnabled = false;
float SilentAimbot::g_targetHeadPos[3] = {0.0f, 0.0f, 0.0f};
float SilentAimbot::g_localPos[3] = {0.0f, 0.0f, 0.0f};
bool SilentAimbot::g_hasTarget = false;

void SilentAimbot::SetTargetPos(const float targetPos[3]) {
    g_targetHeadPos[0] = targetPos[0];
    g_targetHeadPos[1] = targetPos[1];
    g_targetHeadPos[2] = targetPos[2];
    g_hasTarget = true;
}

void SilentAimbot::ClearTarget() {
    if (g_hasTarget) { g_hasTarget = false; }
}

void SilentAimbot::SetLocalPos(const float localPos[3]) {
    g_localPos[0] = localPos[0];
    g_localPos[1] = localPos[1];
    g_localPos[2] = localPos[2];
}

void SilentAimbot::Install() {
    if (g_Installed) { return; }

    g_ModuleBase = reinterpret_cast<uintptr_t>(
        GetModuleHandleA("WindowsEntryPoint.Windows_W10.exe"));
    if (!g_ModuleBase) { return; }

    {
        const BYTE pattern[] = {
            0x41, 0x8B, 0xF1, // mov esi, r9d
            0x4D, 0x8B, 0xF0, // mov r14, r8
            0x4C, 0x8B, 0xFA // mov r15, rdx
        };
        g_HookAddr = Memory::FindPattern(
            "WindowsEntryPoint.Windows_W10.exe", pattern, "xxxxxxxxx");
    }

    if (!g_HookAddr) {
        g_HookAddr = g_ModuleBase + HOOK_OFFSET;

        const BYTE expected[9] = {
            0x41, 0x8B, 0xF1, // mov esi, r9d
            0x4D, 0x8B, 0xF0, // mov r14, r8
            0x4C, 0x8B, 0xFA // mov r15, rdx
        };
        const auto actual = reinterpret_cast<const BYTE *>(g_HookAddr);

        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery(reinterpret_cast<LPCVOID>(g_HookAddr), &mbi, sizeof(mbi)) ||
            mbi.State != MEM_COMMIT ||
            !(mbi.Protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) {
            g_HookAddr = 0;
            return;
        }

        if (memcmp(actual, expected, 9) != 0) {
            g_HookAddr = 0;
            return;
        }
    }

    g_Shellcode = Memory::AllocateNear(g_HookAddr);
    if (!g_Shellcode) { return; }

    auto code = static_cast<BYTE *>(g_Shellcode);
    size_t i = 0;

    // push r10
    code[i++] = 0x41;
    code[i++] = 0x52;

    {
        const auto addr = reinterpret_cast<uintptr_t>(&SilentAimbot::g_hasTarget);
        code[i++] = 0x49;
        code[i++] = 0xBA;
        memcpy(&code[i], &addr, 8);
        i += 8;
        code[i++] = 0x41;
        code[i++] = 0x80;
        code[i++] = 0x3A;
        code[i++] = 0x00;
        code[i++] = 0x74;
    }
    const size_t je1 = i;
    i += 1;

    {
        const auto addr = reinterpret_cast<uintptr_t>(&SilentAimbot::isEnabled);
        code[i++] = 0x49;
        code[i++] = 0xBA;
        memcpy(&code[i], &addr, 8);
        i += 8;
        code[i++] = 0x41;
        code[i++] = 0x80;
        code[i++] = 0x3A;
        code[i++] = 0x00;
        code[i++] = 0x74;
    }
    const size_t je2 = i;
    i += 1;

    {
        const auto addr = reinterpret_cast<uintptr_t>(&g_HookRedirectCount);
        code[i++] = 0x49;
        code[i++] = 0xBA;
        memcpy(&code[i], &addr, 8);
        i += 8;
        code[i++] = 0x41;
        code[i++] = 0xFF;
        code[i++] = 0x02;
    }

    {
        const auto targetAddr = reinterpret_cast<uintptr_t>(&SilentAimbot::g_targetHeadPos);
        code[i++] = 0x49;
        code[i++] = 0xB8;
        memcpy(&code[i], &targetAddr, 8);
        i += 8;
    }

    const size_t skipLabel = i;

    code[i++] = 0x41;
    code[i++] = 0x8B;
    code[i++] = 0xF1;

    code[i++] = 0x4D;
    code[i++] = 0x8B;
    code[i++] = 0xF0;

    code[i++] = 0x4C;
    code[i++] = 0x8B;
    code[i++] = 0xFA;

    code[i++] = 0x41;
    code[i++] = 0x5A;

    const uintptr_t retAddr = g_HookAddr + HOOK_SIZE;
    code[i++] = 0xE9;
    const auto jmpRel = static_cast<int32_t>(
        retAddr - (reinterpret_cast<uintptr_t>(code + i) + 4));
    memcpy(&code[i], &jmpRel, 4);
    i += 4;

    code[je1] = static_cast<BYTE>(skipLabel - (je1 + 1));
    code[je2] = static_cast<BYTE>(skipLabel - (je2 + 1));

    DWORD oldProt;
    VirtualProtect(reinterpret_cast<LPVOID>(g_HookAddr), HOOK_SIZE,
                   PAGE_EXECUTE_READWRITE, &oldProt);
    memcpy(g_OrigBytes, reinterpret_cast<LPVOID>(g_HookAddr), HOOK_SIZE);

    BYTE jmp[9] = {0xE9, 0x00, 0x00, 0x00, 0x00, 0x90, 0x90, 0x90, 0x90};
    const auto rel = static_cast<int32_t>(
        reinterpret_cast<uintptr_t>(g_Shellcode) - (g_HookAddr + 5));
    memcpy(&jmp[1], &rel, 4);
    memcpy(reinterpret_cast<LPVOID>(g_HookAddr), jmp, 9);
    VirtualProtect(reinterpret_cast<LPVOID>(g_HookAddr), HOOK_SIZE, oldProt, &oldProt);

    g_Installed = true;
    isEnabled = true;
}

void SilentAimbot::Uninstall() {
    if (!g_Installed) { return; }

    DWORD oldProt;
    VirtualProtect(reinterpret_cast<LPVOID>(g_HookAddr), HOOK_SIZE,
                   PAGE_EXECUTE_READWRITE, &oldProt);
    memcpy(reinterpret_cast<LPVOID>(g_HookAddr), g_OrigBytes, HOOK_SIZE);
    VirtualProtect(reinterpret_cast<LPVOID>(g_HookAddr), HOOK_SIZE, oldProt, &oldProt);

    if (g_Shellcode) {
        VirtualFree(g_Shellcode, 0, MEM_RELEASE);
        g_Shellcode = nullptr;
    }

    g_Installed = false;
    isEnabled = false;
}

void SilentAimbot::Run() {
    if (!isEnabled) {
        ClearTarget();
        return;
    }

    int localTeam = 0;
    if (uintptr_t localTeamAddr = Memory::ResolveAddress(TEAM_FRIEND_EXPR))
        localTeam = *reinterpret_cast<int *>(localTeamAddr);

    float localPos[3] = {0.0f, 0.0f, 0.0f};
    if (uintptr_t localPosAddr = Memory::ResolveAddress(LOCAL_PLAYER_POSITION_EXPR)) {
        localPos[0] = Memory::ReadFloat(localPosAddr);
        localPos[1] = Memory::ReadFloat(localPosAddr + 4);
        localPos[2] = Memory::ReadFloat(localPosAddr + 8);
    }
    SetLocalPos(localPos);

    std::vector<PlayerInfo> players = GetPlayers();
    if (players.empty()) {
        ClearTarget();
        return;
    }

    std::vector<BoneInfo> allBones;
    CollectAllBones(allBones);
    DistributeBonesToPlayers(players, allBones);

    ImGuiIO &io = ImGui::GetIO();
    const float screenCenterX = io.DisplaySize.x * 0.5f;
    const float screenCenterY = io.DisplaySize.y * 0.5f;

    float m[4][4];
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            m[i][j] = ViewMatrix[i * 4 + j];

    float fovY = 2.0f * atanf(1.0f / m[1][1]) * (180.0f / PI);
    if (fovY < 10.0f || fovY > 120.0f) fovY = 60.0f;

    const PlayerInfo *best = nullptr;
    BoneInfo bestHead;
    float bestScreenDist = g_silentFov;

    for (const auto &p: players) {
        if (p.HP <= 34.0f) continue;
        if (p.Team == localTeam) continue;

        BoneInfo head;
        if (!GetPlayerHeadBone(p, head)) continue;

        float sx, sy;
        if (!WorldToScreen(head.x, head.y, head.z, sx, sy)) continue;

        const float dx = sx - screenCenterX;
        const float dy = sy - screenCenterY;
        const float screenDist = sqrtf(dx * dx + dy * dy);

        if (screenDist < bestScreenDist) {
            bestScreenDist = screenDist;
            best = &p;
            bestHead = head;
        }
    }

    const float targetHeadPos[3] = {bestHead.x, bestHead.y, bestHead.z};
    if (!ObstacleDetection::IsTargetVisible(localPos, targetHeadPos)) {
        ClearTarget();
        return;
    }

    if (!best) {
        ClearTarget();
        return;
    }

    SetTargetPos(targetHeadPos);
}
