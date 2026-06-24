#include "Aimbot.h"

#include <algorithm>
#include "features.h"
#include "memory.h"
#include <cmath>
#include "Bones.h"
#include "imgui.h"
#include "Players.h"

extern float ViewMatrix[16];
extern bool GetViewMatrix();

bool Aimbot::isEnabled = false;
float Aimbot::smooth = 0.25f;
float Aimbot::g_targetPitchDelta = 0.0f;
bool Aimbot::g_hasTarget = false;
float Aimbot::g_debugTargetPos[3] = {0, 0, 0};
bool Aimbot::g_debugHasTarget = false;
float Aimbot::g_debugTargetScreenPos[2] = {0, 0};
float Aimbot::g_aimFov = 200.0f;

static LPVOID AllocateNear(const uintptr_t target) {
    for (int i = 0; i < 524288; ++i) {
        const int dir = (i % 2 == 0) ? 1 : -1;
        uintptr_t addr = target + dir * (i / 2) * 0x1000;
        addr &= ~static_cast<uintptr_t>(0xFFF);
        if (LPVOID p = VirtualAlloc(reinterpret_cast<LPVOID>(addr), 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE)) {
            if (const auto rel = static_cast<int64_t>(reinterpret_cast<uintptr_t>(p) - (target + 5)); rel >= INT32_MIN && rel <= INT32_MAX)
                return p;
            VirtualFree(p, 0, MEM_RELEASE);
        }
    }
    return nullptr;
}

#define OFFSET_CONTROLLER_YAW     0x6C
#define OFFSET_CONTROLLER_PITCH   0x630

static uintptr_t g_gameBase = 0;

static bool     g_pitchHookInstalled = false;
static uintptr_t g_pitchHookAddr = 0;
static BYTE     g_pitchOrigBytes[8] = {0};
static LPVOID   g_pitchShellcode = nullptr;

bool Aimbot::InitGameFunctions() {
    g_gameBase = reinterpret_cast<uintptr_t>(GetModuleHandleA("WindowsEntryPoint.Windows_W10.exe"));
    return (g_gameBase != 0);
}

static uintptr_t GetYawObject() {
    uintptr_t addr = Memory::ResolveAddress(FPS_YAW_EXPR);
    return addr ? (addr - OFFSET_CONTROLLER_YAW) : 0;
}

static uintptr_t GetPitchObject() {
    uintptr_t addr = Memory::ResolveAddress(FPS_PITCH_EXPR);
    return addr ? (addr - OFFSET_CONTROLLER_PITCH) : 0;
}

static bool IsAimingDownSight() {
    if (!g_gameBase) return false;
    uintptr_t addr = g_gameBase + 0x01CF5D08;
    uintptr_t ptr1 = 0;
    if (!Memory::SafeReadPtr(addr, ptr1) || !ptr1) return false;
    const int value = static_cast<int> (Memory::ReadInt(ptr1 + 0x278));
    return (value != 0);
}

static bool WorldToScreen(float x, float y, float z, float& sx, float& sy) {
    float m[4][4];
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            m[i][j] = ViewMatrix[i * 4 + j];
    float w = m[0][3] * x + m[1][3] * y + m[2][3] * z + m[3][3];
    if (w < 0.001f) return false;
    float cx = m[0][0] * x + m[1][0] * y + m[2][0] * z + m[3][0];
    float cy = m[0][1] * x + m[1][1] * y + m[2][1] * z + m[3][1];
    ImGuiIO& io = ImGui::GetIO();
    sx = (io.DisplaySize.x * 0.5f) * (1.0f + cx / w);
    sy = (io.DisplaySize.y * 0.5f) * (1.0f - cy / w);
    return true;
}

namespace Aimbot {
    constexpr float PI = 3.14159265358979323846f;

    float Normalize(float a) {
        a = fmodf(a, 360.0f);
        if (a < 0.0f) a += 360.0f;
        return a;
    }
}

static bool GetPlayerHeadBone(const PlayerInfo& player, BoneInfo& outHead) {
    for (const auto& bone : player.bones) {
        if (bone.boneName == "Bip01_Head") {
            outHead = bone;
            return true;
        }
    }
    return false;
}

static void BuildPitchShellcode(BYTE* code, size_t& i, uintptr_t hookAddr) {
    uintptr_t retAddr = hookAddr + 8;
    auto hasTargetAddr = reinterpret_cast<uintptr_t>(&Aimbot::g_hasTarget);
    auto deltaAddr = reinterpret_cast<uintptr_t>(&Aimbot::g_targetPitchDelta);

    code[i++] = 0xF3; code[i++] = 0x0F; code[i++] = 0x11;
    code[i++] = 0x89; code[i++] = 0x30; code[i++] = 0x06; code[i++] = 0x00; code[i++] = 0x00;

    code[i++] = 0x50;

    code[i++] = 0x48; code[i++] = 0xB8;
    memcpy(&code[i], &hasTargetAddr, 8); i += 8;

    code[i++] = 0x80; code[i++] = 0x38; code[i++] = 0x00;

    code[i++] = 0x74;
    size_t jePos = i; i += 1;

    code[i++] = 0x48; code[i++] = 0xB8;
    memcpy(&code[i], &deltaAddr, 8); i += 8;

    code[i++] = 0xF3; code[i++] = 0x0F; code[i++] = 0x10; code[i++] = 0x08;
    size_t skipAddr = i;

    code[jePos] = static_cast<BYTE>(skipAddr - (jePos + 1));
    code[i++] = 0x58;
    code[i++] = 0xE9;

    auto jmpRel = static_cast<int32_t>(retAddr - (reinterpret_cast<uintptr_t>(code + i) + 4));
    memcpy(&code[i], &jmpRel, 4); i += 4;
}

bool Aimbot::InstallPitchHook() {
    if (g_pitchHookInstalled) return true;
    if (!g_gameBase) {
        return false;
    }

    g_pitchHookAddr = g_gameBase + 0x4701E5;

    g_pitchShellcode = AllocateNear(g_pitchHookAddr);
    if (!g_pitchShellcode) {
        return false;
    }

    const auto code = static_cast<BYTE*>(g_pitchShellcode);
    size_t i = 0;
    BuildPitchShellcode(code, i, g_pitchHookAddr);

    memcpy(g_pitchOrigBytes, reinterpret_cast<LPVOID>(g_pitchHookAddr), 8);

    DWORD oldProt;
    VirtualProtect(reinterpret_cast<LPVOID>(g_pitchHookAddr), 8, PAGE_EXECUTE_READWRITE, &oldProt);
    BYTE jmp[5] = { 0xE9 };
    const auto rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(g_pitchShellcode) - (g_pitchHookAddr + 5));
    memcpy(&jmp[1], &rel, 4);
    memcpy(reinterpret_cast<LPVOID>(g_pitchHookAddr), jmp, 5);
    memset(reinterpret_cast<LPVOID>(g_pitchHookAddr + 5), 0x90, 3);
    VirtualProtect(reinterpret_cast<LPVOID>(g_pitchHookAddr), 8, oldProt, &oldProt);

    g_pitchHookInstalled = true;
    return true;
}

void Aimbot::UninstallPitchHook() {
    if (!g_pitchHookInstalled) return;
    DWORD oldProt;
    VirtualProtect(reinterpret_cast<LPVOID>(g_pitchHookAddr), 8, PAGE_EXECUTE_READWRITE, &oldProt);
    memcpy(reinterpret_cast<LPVOID>(g_pitchHookAddr), g_pitchOrigBytes, 8);
    VirtualProtect(reinterpret_cast<LPVOID>(g_pitchHookAddr), 8, oldProt, &oldProt);
    if (g_pitchShellcode) {
        VirtualFree(g_pitchShellcode, 0, MEM_RELEASE);
        g_pitchShellcode = nullptr;
    }
    g_pitchHookInstalled = false;
}

void Aimbot::Run() {
    int localTeam = 0;
    if (uintptr_t localTeamAddr = Memory::ResolveAddress(TEAM_FRIEND_EXPR)) {
        localTeam = *reinterpret_cast<int*>(localTeamAddr);
    }

    g_debugHasTarget = false;
    g_hasTarget = false;
    g_targetPitchDelta = 0.0f;

    if (!isEnabled) return;

    static bool initialized = false;
    if (!initialized) {
        if (!InitGameFunctions()) return;
        if (!InstallPitchHook()) return;
        initialized = true;
    }

    std::vector<PlayerInfo> players = GetPlayers();
    if (players.empty()) return;

    std::vector<BoneInfo> allBones;
    CollectAllBones(allBones);
    DistributeBonesToPlayers(players, allBones);

    if (!IsAimingDownSight()) return;

    uintptr_t yawObj = GetYawObject();
    uintptr_t pitchObj = GetPitchObject();
    if (!yawObj || !pitchObj) return;

    ImGuiIO& io = ImGui::GetIO();
    float screenCenterX = io.DisplaySize.x * 0.5f;
    float screenCenterY = io.DisplaySize.y * 0.5f;

    const PlayerInfo* best = nullptr;
    BoneInfo bestHead;
    float bestScreenDist = g_aimFov;

    for (const auto& p : players) {
        if (p.HP <= 34.0f) continue;
        if (p.Team == localTeam) continue;
        BoneInfo head;
        if (!GetPlayerHeadBone(p, head)) continue;
        float sx, sy;
        if (!WorldToScreen(head.x, head.y, head.z, sx, sy)) continue;
        float dx = sx - screenCenterX;
        float dy = sy - screenCenterY;
        float screenDist = sqrtf(dx * dx + dy * dy);
        if (screenDist < bestScreenDist) {
            bestScreenDist = screenDist;
            best = &p;
            bestHead = head;
        }
    }

    if (!best) return;

    g_debugTargetPos[0] = bestHead.x;
    g_debugTargetPos[1] = bestHead.y;
    g_debugTargetPos[2] = bestHead.z;
    WorldToScreen(bestHead.x, bestHead.y, bestHead.z,
                  g_debugTargetScreenPos[0], g_debugTargetScreenPos[1]);
    g_debugHasTarget = true;

    float m[4][4];
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            m[i][j] = ViewMatrix[i * 4 + j];

    float fovY = 2.0f * atanf(1.0f / m[1][1]) * (180.0f / Aimbot::PI);
    if (fovY < 10.0f || fovY > 120.0f) fovY = 60.0f;

    float aspectRatio = io.DisplaySize.x / io.DisplaySize.y;
    float fovX = 2.0f * atanf(tanf(fovY * 0.5f * Aimbot::PI / 180.0f) * aspectRatio) * (180.0f / Aimbot::PI);
    float degreesPerPixelX = fovX / io.DisplaySize.x;
    float degreesPerPixelY = fovY / io.DisplaySize.y;
    float pixelDiffX = g_debugTargetScreenPos[0] - screenCenterX;
    float pixelDiffY = g_debugTargetScreenPos[1] - screenCenterY;
    float neededDeltaYaw = -pixelDiffX * degreesPerPixelX;
    float currentYaw = Memory::ReadFloat(yawObj + OFFSET_CONTROLLER_YAW);
    float newYaw = Aimbot::Normalize(currentYaw + neededDeltaYaw * smooth);
    Memory::WriteFloat(yawObj + OFFSET_CONTROLLER_YAW, newYaw);

    float targetPitch = -pixelDiffY * degreesPerPixelY;
    float currentPitch = Memory::ReadFloat(pitchObj + OFFSET_CONTROLLER_PITCH);

    if (float dp = targetPitch - currentPitch; fabsf(dp) > 0.1f) {
        float residue = Memory::ReadFloat(pitchObj + 0x638);
        g_targetPitchDelta = (dp * smooth) / 0.02f - residue;
        Memory::WriteFloat(pitchObj + 0x638, 0.0f);
    }
}
