#include "Aimbot.h"
#include "../utils/utils.h"
#include "features.h"
#include "memory.h"
#include <cmath>
#include "Bones.h"
#include "imgui.h"
#include "menu.h"
#include "ObstacleDetection.h"
#include "Players.h"

bool Aimbot::isEnabled = false;
float Aimbot::smooth = 0.35f;
float Aimbot::g_targetPitchDelta = 0.0f;
bool Aimbot::g_hasTarget = false;
float Aimbot::g_debugTargetPos[3] = {0, 0, 0};
bool Aimbot::g_debugHasTarget = false;
float Aimbot::g_debugTargetScreenPos[2] = {0, 0};
float Aimbot::g_aimFov = 25.0f;

#define OFFSET_CONTROLLER_YAW   0x6C
#define OFFSET_CONTROLLER_PITCH 0x64

static uintptr_t g_gameBase = 0;

bool Aimbot::InitGameFunctions() {
    g_gameBase = reinterpret_cast<uintptr_t>(
        GetModuleHandleA("WindowsEntryPoint.Windows_W10.exe"));
    if (!g_gameBase) {
        return false;
    }
    ObstacleDetection::Install();
    return true;
}

static uintptr_t GetYawObject() {
    uintptr_t addr = Memory::ResolveAddress(FPS_YAW_EXPR);
    return addr ? (addr - OFFSET_CONTROLLER_YAW) : 0;
}

static void ResetPitchOffset(uintptr_t yawObj) {
    if (yawObj) {
        Memory::WriteFloat(yawObj + OFFSET_CONTROLLER_PITCH, 0.0f);
    }
}

bool Aimbot::InstallPitchHook() { return true; }

void Aimbot::UninstallPitchHook() {
}

static float GetRealFOV() {
    static auto gameBase = reinterpret_cast<uintptr_t>(
        GetModuleHandleA("WindowsEntryPoint.Windows_W10.exe"));
    if (!gameBase) return 60.0f;

    constexpr uintptr_t CAMERA_MAIN_PTR_OFFSET = 0x1D8E3A0;
    uintptr_t mainCamPtr = 0;

    if (!Memory::SafeReadPtr(gameBase + CAMERA_MAIN_PTR_OFFSET, mainCamPtr))
        return 60.0f;

    if (!mainCamPtr) return 60.0f;

    float fov = Memory::ReadFloat(mainCamPtr + 0xA0);

    if (fov < 1.0f || fov > 120.0f) fov = 60.0f;
    return fov;
}

void Aimbot::Run() {
    static int frameCount = 0;
    ++frameCount;

    int localTeam = 0;
    if (uintptr_t localTeamAddr = Memory::ResolveAddress(TEAM_FRIEND_EXPR)) {
        localTeam = *reinterpret_cast<int *>(localTeamAddr);
    }

    g_debugHasTarget = false;
    g_hasTarget = false;
    g_targetPitchDelta = 0.0f;

    if (!isEnabled) return;
    if (!GetViewMatrix()) return;

    static bool initialized = false;
    if (!initialized) {
        if (!InitGameFunctions()) {
            return;
        }
        initialized = true;
    }

    std::vector<PlayerInfo> players = GetPlayers();
    if (players.empty()) return;

    std::vector<BoneInfo> allBones;
    CollectAllBones(allBones);
    DistributeBonesToPlayers(players, allBones);

    uintptr_t yawObj = GetYawObject();
    if (!yawObj) {
        return;
    }

    ImGuiIO &io = ImGui::GetIO();
    float screenCenterX = io.DisplaySize.x * 0.5f;
    float screenCenterY = io.DisplaySize.y * 0.5f;

    float fovY = GetRealFOV();

    float aspectRatio = io.DisplaySize.x / io.DisplaySize.y;
    float fovX = 2.0f * atanf(tanf(fovY * 0.5f * PI / 180.0f) * aspectRatio)
                 * (180.0f / PI);
    float degreesPerPixelX = fovX / io.DisplaySize.x;
    float degreesPerPixelY = fovY / io.DisplaySize.y;

    const PlayerInfo *best = nullptr;
    BoneInfo bestHead;
    float bestAngularDist = g_aimFov;

    if (!IsAimingDownSight()) {
        ResetPitchOffset(yawObj);
        return;
    }

    for (const auto &p: players) {
        if (p.HP <= 34.0f) continue;
        if (p.Team == localTeam) continue;
        BoneInfo head;
        if (!GetPlayerHeadBone(p, head)) continue;
        float sx, sy;
        if (!WorldToScreen(head.x, head.y, head.z, sx, sy)) continue;

        float dx = sx - screenCenterX;
        float dy = sy - screenCenterY;
        float angularDistX = fabsf(dx) * degreesPerPixelX;
        float angularDistY = fabsf(dy) * degreesPerPixelY;
        float angularDist = sqrtf(angularDistX * angularDistX + angularDistY * angularDistY);

        if (angularDist < bestAngularDist) {
            bestAngularDist = angularDist;
            best = &p;
            bestHead = head;
        }
    }

    if (!best) {
        ResetPitchOffset(yawObj);
        return;
    }

    float localPos[3] = {0.0f, 0.0f, 0.0f};
    if (uintptr_t localPosAddr = Memory::ResolveAddress(LOCAL_PLAYER_POSITION_EXPR)) {
        localPos[0] = Memory::ReadFloat(localPosAddr);
        localPos[1] = Memory::ReadFloat(localPosAddr + 4);
        localPos[2] = Memory::ReadFloat(localPosAddr + 8);
    }

    float targetHeadPos[3] = {bestHead.x, bestHead.y, bestHead.z};
    if (!ObstacleDetection::IsTargetVisible(localPos, targetHeadPos)) {
        ResetPitchOffset(yawObj);
        return;
    }

    g_debugTargetPos[0] = bestHead.x;
    g_debugTargetPos[1] = bestHead.y;
    g_debugTargetPos[2] = bestHead.z;
    WorldToScreen(bestHead.x, bestHead.y, bestHead.z,
                  g_debugTargetScreenPos[0], g_debugTargetScreenPos[1]);
    g_debugHasTarget = true;

    float pixelDiffX = g_debugTargetScreenPos[0] - screenCenterX;
    float pixelDiffY = g_debugTargetScreenPos[1] - screenCenterY;

    float angularDistDeg = sqrtf(
        (pixelDiffX * degreesPerPixelX) * (pixelDiffX * degreesPerPixelX) +
        (pixelDiffY * degreesPerPixelY) * (pixelDiffY * degreesPerPixelY)
    );


    if (constexpr float deadZoneDeg = 0.15f; angularDistDeg < deadZoneDeg) return;

    float aimFactor = 0.18f - smooth * 0.3f;
    if (aimFactor < 0.03f) aimFactor = 0.03f;

    float scopeCompensation = 60.0f / fovY;
    if (scopeCompensation < 1.0f) scopeCompensation = 1.0f;
    if (scopeCompensation > 4.0f) scopeCompensation = 4.0f;

    aimFactor *= scopeCompensation;
    if (aimFactor > 0.40f) aimFactor = 0.40f;

    float neededDeltaYaw = -pixelDiffX * degreesPerPixelX;
    float currentYaw = Memory::ReadFloat(yawObj + OFFSET_CONTROLLER_YAW);
    float newYaw = currentYaw + neededDeltaYaw * aimFactor;

    while (newYaw > 360.0f) newYaw -= 360.0f;
    while (newYaw < 0.0f) newYaw += 360.0f;

    Memory::WriteFloat(yawObj + OFFSET_CONTROLLER_YAW, newYaw);

    float neededDeltaPitch = -pixelDiffY * degreesPerPixelY;
    float currentPitch = Memory::ReadFloat(yawObj + OFFSET_CONTROLLER_PITCH);
    float newPitch = currentPitch + neededDeltaPitch * aimFactor;

    if (newPitch > 90.0f) newPitch = 90.0f;
    if (newPitch < -90.0f) newPitch = -90.0f;

    Memory::WriteFloat(yawObj + OFFSET_CONTROLLER_PITCH, newPitch);
}

void Aimbot::DrawFovCircle() {
    if (!isAimbot) return;

    ImDrawList *dl = ImGui::GetBackgroundDrawList();
    const ImGuiIO &io = ImGui::GetIO();

    float fovY = GetRealFOV();
    float degPerPixel = fovY / io.DisplaySize.y;
    float radius = g_aimFov / degPerPixel;

    dl->AddCircle(
        ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
        radius,IM_COL32(g_FovCircleColor[0], g_FovCircleColor[1], g_FovCircleColor[2], 100), 64, 2.0f);
}
