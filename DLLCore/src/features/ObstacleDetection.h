#pragma once

namespace ObstacleDetection {
    void Install();
    void Uninstall();

    bool HasLineOfSight(const float startPos[3], const float endPos[3]);

    bool IsTargetVisible(const float localPos[3], const float targetHeadPos[3]);

    extern bool  g_obstacleFound;
    extern float g_lastHitPos[3];
    extern float g_lastHitNormal[3];
}