#include "ObstacleDetection.h"
#include "memory.h"
#include "features.h"
#include <Windows.h>
#include <cmath>
#include <cstdint>

/*
=====================================================================
RaycastTest func (x64 __fastcall, Standard MSVC ABI)

6.0.1a - sub_140434B30 — Bullet Physics CClosestRayResultCallback::rayTest

RCX = collisionWorld   (void*)
RDX = startPos         (const float*)
R8  = endPos           (const float*)
R9  = hitPosOut        (float*)
[RSP+0x20] = normalOut      (float*)
[RSP+0x28] = hitEntityOut   (void**)
[RSP+0x30] = filterMask1    (int)
[RSP+0x38] = filterMask2    (int)
Return: bool (al) — true: hit, false: not hit
=====================================================================
*/
typedef bool (*RaycastFunc)(
    void*        collisionWorld,
    const float* startPos,
    const float* endPos,
    float*       hitPosOut,
    float*       normalOut,
    void**       hitEntityOut,
    int          filterMask1,
    int          filterMask2
);

static uintptr_t   g_ModuleBase  = 0;
static RaycastFunc g_Raycast     = nullptr;
static bool        g_Installed   = false;

bool  ObstacleDetection::g_obstacleFound  = false;
float ObstacleDetection::g_lastHitPos[3]   = {0.0f, 0.0f, 0.0f};
float ObstacleDetection::g_lastHitNormal[3]= {0.0f, 0.0f, 0.0f};

void ObstacleDetection::Install()
{
    if (g_Installed) return;

    g_ModuleBase = reinterpret_cast<uintptr_t>(
        GetModuleHandleA("WindowsEntryPoint.Windows_W10.exe"));
    if (!g_ModuleBase) return;

    const BYTE pattern[] = {
        0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x10,
        0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57,
        0x48, 0x8D, 0x68, 0x88,
        0x48, 0x81, 0xEC, 0x40, 0x01, 0x00, 0x00
    };
    constexpr char mask[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

    const uintptr_t addr = Memory::FindPattern(
        "WindowsEntryPoint.Windows_W10.exe", pattern, mask);

    if (!addr) return;

    g_Raycast  = reinterpret_cast<RaycastFunc>(addr);
    g_Installed = true;
}

void ObstacleDetection::Uninstall()
{
    g_Raycast   = nullptr;
    g_Installed = false;
}

bool ObstacleDetection::HasLineOfSight(const float startPos[3],const float endPos[3]){
    if (!g_Installed || !g_Raycast)
        return true;

    const uintptr_t collisionBasePtr = Memory::ResolveAddress(MULTI_COLLISION_BASE);
    if (!collisionBasePtr)
        return true;

    void* collisionWorld = *reinterpret_cast<void**>(collisionBasePtr);
    if (!collisionWorld)
        return true;

    float  hitPos[3]    = {0.0f, 0.0f, 0.0f};
    float  hitNormal[3] = {0.0f, 0.0f, 0.0f};
    void*  hitEntity    = nullptr;

    const bool hit = g_Raycast(
        collisionWorld,
        startPos,
        endPos,
        hitPos,
        hitNormal,
        &hitEntity,
        1,
        2
    );

    g_lastHitPos[0]    = hitPos[0];
    g_lastHitPos[1]    = hitPos[1];
    g_lastHitPos[2]    = hitPos[2];
    g_lastHitNormal[0] = hitNormal[0];
    g_lastHitNormal[1] = hitNormal[1];
    g_lastHitNormal[2] = hitNormal[2];

    if (!hit)
    {
        g_obstacleFound = false;
        return true;
    }

    const float dx = endPos[0] - startPos[0];
    const float dy = endPos[1] - startPos[1];
    const float dz = endPos[2] - startPos[2];
    const float targetDist = std::sqrt(dx * dx + dy * dy + dz * dz);

    const float hdx = hitPos[0] - startPos[0];
    const float hdy = hitPos[1] - startPos[1];
    const float hdz = hitPos[2] - startPos[2];
    const float hitDist = std::sqrt(hdx * hdx + hdy * hdy + hdz * hdz);

    constexpr float kThreshold = 2.0f;
    const bool clear = (hitDist >= targetDist - kThreshold);

    g_obstacleFound = !clear;
    return clear;
}

bool ObstacleDetection::IsTargetVisible(
    const float localPos[3],
    const float targetHeadPos[3])
{
    return HasLineOfSight(localPos, targetHeadPos);
}