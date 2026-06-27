#pragma once

namespace Aimbot
{
    extern bool isEnabled;
    extern float smooth;
    extern float g_aimFov;
    void Run();
    bool InitGameFunctions();
    bool InstallPitchHook();
    void UninstallPitchHook();

    extern float g_targetPitchDelta;
    extern bool g_hasTarget;
    extern bool g_overridePitch;
    extern float g_targetPitch;
    extern float g_debugTargetPos[3];
    extern bool g_debugHasTarget;
    extern float g_debugTargetScreenPos[2];
}
