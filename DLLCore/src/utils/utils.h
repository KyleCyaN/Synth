#pragma once
#include "../features/ESP.h"

extern float ViewMatrix[16];
constexpr float PI = 3.14159265358979323846f;
bool GetViewMatrix();

bool WorldToScreen(float x, float y, float z, float& sx, float& sy);

float CalculateDistance(float x1, float y1, float z1, float x2, float y2, float z2);

bool GetPlayerHeadBone(const PlayerInfo& player, BoneInfo& outHead);

bool IsAimingDownSight();

float NormalizeAngle(float angle);