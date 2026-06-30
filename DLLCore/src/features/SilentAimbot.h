#pragma once

namespace SilentAimbot {
    void Install();
    void Uninstall();
    void Run();

    void SetTargetPos(const float targetPos[3]);
    void ClearTarget();
    void SetLocalPos(const float localPos[3]);

    extern bool isEnabled;
    extern float g_targetHeadPos[3];
    extern float g_localPos[3];
    extern bool g_hasTarget;
    extern float g_silentFov;
}