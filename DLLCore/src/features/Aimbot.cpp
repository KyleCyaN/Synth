#include "Aimbot.h"
#include <cstdio>
#include "../utils/utils.h"
#include "features.h"
#include "memory.h"
#include <cmath>
#include "Bones.h"
#include "imgui.h"
#include "menu.h"
#include "ObstacleDetection.h"
#include "Players.h"

static void DBG(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    const int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (n > 0 && static_cast<size_t>(n) < sizeof(buf) - 2) {
        buf[n]     = '\r';
        buf[n + 1] = '\n';
        buf[n + 2] = '\0';
    }
    OutputDebugStringA(buf);
}

bool Aimbot::isEnabled = false;
float Aimbot::smooth = 0.25f;
float Aimbot::g_targetPitchDelta = 0.0f;
bool Aimbot::g_hasTarget = false;
float Aimbot::g_debugTargetPos[3] = {0, 0, 0};
bool Aimbot::g_debugHasTarget = false;
float Aimbot::g_debugTargetScreenPos[2] = {0, 0};
float Aimbot::g_aimFov = 10.0f;

#define OFFSET_CONTROLLER_YAW   0x6C
#define OFFSET_CONTROLLER_PITCH 0x64

static uintptr_t g_gameBase = 0;

bool Aimbot::InitGameFunctions() {
    g_gameBase = reinterpret_cast<uintptr_t>(
        GetModuleHandleA("WindowsEntryPoint.Windows_W10.exe"));
    if (!g_gameBase) {
        DBG("[Aimbot] InitGameFunctions FAILED: module base is NULL");
        return false;
    }
    DBG("[Aimbot] InitGameFunctions OK: module base = 0x%p", (void*)g_gameBase);
    ObstacleDetection::Install();
    return true;
}

static uintptr_t GetYawObject() {
    uintptr_t addr = Memory::ResolveAddress(FPS_YAW_EXPR);
    return addr ? (addr - OFFSET_CONTROLLER_YAW) : 0;
}

bool Aimbot::InstallPitchHook()  { return true; }
void Aimbot::UninstallPitchHook() {}

void Aimbot::Run() {
    static int frameCount = 0;
    ++frameCount;

    int localTeam = 0;
    if (uintptr_t localTeamAddr = Memory::ResolveAddress(TEAM_FRIEND_EXPR)) {
        localTeam = *reinterpret_cast<int*>(localTeamAddr);
    }

    g_debugHasTarget   = false;
    g_hasTarget        = false;
    g_targetPitchDelta = 0.0f;

    if (!isEnabled) return;

    static bool initialized = false;
    if (!initialized) {
        DBG("[Aimbot] First Run: initializing...");
        if (!InitGameFunctions()) {
            DBG("[Aimbot] Run ABORT: InitGameFunctions failed");
            return;
        }
        initialized = true;
    }

    std::vector<PlayerInfo> players = GetPlayers();
    if (players.empty()) return;

    std::vector<BoneInfo> allBones;
    CollectAllBones(allBones);
    DistributeBonesToPlayers(players, allBones);

    if (!IsAimingDownSight()) return;

    uintptr_t yawObj = GetYawObject();
    if (!yawObj) {
        if (frameCount % 300 == 0)
            DBG("[Aimbot] Run: yawObj is NULL");
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    float screenCenterX = io.DisplaySize.x * 0.5f;
    float screenCenterY = io.DisplaySize.y * 0.5f;

    float m[4][4];
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            m[r][c] = ViewMatrix[r * 4 + c];

    float fovY = 2.0f * atanf(1.0f / m[1][1]) * (180.0f / PI);
    if (fovY < 10.0f || fovY > 120.0f) fovY = 60.0f;

    float aspectRatio = io.DisplaySize.x / io.DisplaySize.y;
    float fovX = 2.0f * atanf(tanf(fovY * 0.5f * PI / 180.0f) * aspectRatio)
                 * (180.0f / PI);
    float degreesPerPixelX = fovX / io.DisplaySize.x;
    float degreesPerPixelY = fovY / io.DisplaySize.y;

    const PlayerInfo* best = nullptr;
    BoneInfo bestHead;
    float bestAngularDist = g_aimFov;

    for (const auto& p : players) {
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

    if (!best) return;

    float localPos[3] = {0.0f, 0.0f, 0.0f};
    if (uintptr_t localPosAddr = Memory::ResolveAddress(LOCAL_PLAYER_POSITION_EXPR)) {
        localPos[0] = Memory::ReadFloat(localPosAddr);
        localPos[1] = Memory::ReadFloat(localPosAddr + 4);
        localPos[2] = Memory::ReadFloat(localPosAddr + 8);
    }

    float targetHeadPos[3] = {bestHead.x, bestHead.y, bestHead.z};
    if (!ObstacleDetection::IsTargetVisible(localPos, targetHeadPos)) return;

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
    const float deadzoneDeg = 0.15f;
    if (angularDistDeg < deadzoneDeg) return;

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
    while (newYaw < 0.0f)   newYaw += 360.0f;

    Memory::WriteFloat(yawObj + OFFSET_CONTROLLER_YAW, newYaw);

    float neededDeltaPitch = -pixelDiffY * degreesPerPixelY;
    float currentPitch = Memory::ReadFloat(yawObj + OFFSET_CONTROLLER_PITCH);
    float newPitch = currentPitch + neededDeltaPitch * aimFactor;

    if (newPitch > 89.0f)  newPitch = 89.0f;
    if (newPitch < -89.0f) newPitch = -89.0f;

    Memory::WriteFloat(yawObj + OFFSET_CONTROLLER_PITCH, newPitch);

    if (frameCount % 60 == 0) {
        DBG("[Aimbot] Frm=%d | FOV=%.1f° aimFov=%.0f° AngD=%.2f° Dead=%.2f° | "
            "ScopeComp=%.2f AimF=%.3f | "
            "pixDiff=(%.0f,%.0f) dYaw=%.3f dPitch=%.3f | "
            "Yaw %.2f→%.2f | Pitch %.2f→%.2f",
            frameCount, fovY, g_aimFov,
            bestAngularDist, deadzoneDeg,
            scopeCompensation, aimFactor,
            pixelDiffX, pixelDiffY,
            neededDeltaYaw, neededDeltaPitch,
            currentYaw, newYaw,
            currentPitch, newPitch);
    }
}

void Aimbot::DrawFovCircle() {
    if (!isAimbot) return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImGuiIO& io = ImGui::GetIO();

    float fovY = 2.0f * atanf(1.0f / ViewMatrix[5]) * (180.0f / 3.14159265f);
    if (fovY < 10.0f || fovY > 120.0f) fovY = 60.0f;
    float degPerPixel = fovY / io.DisplaySize.y;
    float radius = g_aimFov / degPerPixel;

    dl->AddCircle(
        ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
        radius,
        IM_COL32(g_FovCircleColor[0], g_FovCircleColor[1], g_FovCircleColor[2], 100),
        64, 2.0f);
}