#include <windows.h>
#include <cstdio>
#include "ESP.h"

#include <algorithm>
#include <vector>
#include <cmath>
#include "Players.h"
#include "Bones.h"
#include "../utils/utils.h"
#include "features.h"
#include "imgui.h"
#include "memory.h"
#include "menu.h"

std::vector<BoneInfo> g_headBones;
float LocalPlayerPosition[3];

// =====================================================================
// ⚠️ 以下函数已移至 SynthUtils，此处删除
//   - ViewMatrix[16]           → SynthUtils.cpp
//   - GetViewMatrix()          → SynthUtils.cpp
//   - WorldToScreenPoint()     → SynthUtils::WorldToScreen()
//   - CalculateDistance()      → SynthUtils.cpp
//   - GetHeadForPlayer()       → SynthUtils::GetPlayerHeadBone()
// =====================================================================

inline ImU32 GetHealthGradientColor(const float p) {
    int r = static_cast<int>(255.0f * (1.0f - p));
    int g = static_cast<int>(255.0f * p);
    return IM_COL32(r, g, 0, 255);
}

void DrawPlayerSkeleton(const std::vector<BoneInfo>& bones, const float hp, const float distance) {
    if (hp <= 34.0f || bones.empty()) return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    int alpha = (distance > 50.f) ? 100 : (distance > 30.f) ? 160 : 210;

    ImU32 boneColor = IM_COL32(g_BoneColor[0], g_BoneColor[1], g_BoneColor[2], alpha);
    ImU32 jointColor = IM_COL32(g_JointColor[0], g_JointColor[1], g_JointColor[2], alpha);

    auto findBone = [&](const char* n) -> const BoneInfo* {
        for (auto& b : bones) if (b.boneName == n) return &b;
        return nullptr;
    };

    const BoneInfo* head   = findBone("Bip01_Head");
    const BoneInfo* spine  = findBone("Bip01_Spine");
    const BoneInfo* pelvis = findBone("Bip01_Pelvis");
    const BoneInfo* lUA    = findBone("Bip01_L_UpperArm");
    const BoneInfo* rUA    = findBone("Bip01_R_UpperArm");
    const BoneInfo* lFA    = findBone("Bip01_L_Forearm");
    const BoneInfo* rFA    = findBone("Bip01_R_Forearm");
    const BoneInfo* lT     = findBone("Bip01_L_Thigh");
    const BoneInfo* rT     = findBone("Bip01_R_Thigh");
    const BoneInfo* lC     = findBone("Bip01_L_Calf");
    const BoneInfo* rC     = findBone("Bip01_R_Calf");

    auto line = [&](const BoneInfo* a, const BoneInfo* b, ImU32 c, const ImGuiIO& io) {
        if (!a || !b) return;
        bool aOn = a->isOnScreen;
        bool bOn = b->isOnScreen;
        if (!aOn && !bOn) return;
        float ax = aOn ? a->screenX : io.DisplaySize.x * 0.5f;
        float ay = aOn ? a->screenY : 0.0f;
        float bx = bOn ? b->screenX : io.DisplaySize.x * 0.5f;
        float by = bOn ? b->screenY : 0.0f;
        dl->AddLine(ImVec2(ax, ay), ImVec2(bx, by), c, 1.5f);
    };
    ImGuiIO& io = ImGui::GetIO();

    line(head, spine, boneColor, io);
    line(spine, pelvis, boneColor, io);
    line(spine, lUA, boneColor, io);
    line(lUA, lFA, boneColor, io);
    line(spine, rUA, boneColor, io);
    line(rUA, rFA, boneColor, io);
    line(pelvis, lT, boneColor, io);
    line(lT, lC, boneColor, io);
    line(pelvis, rT, boneColor, io);
    line(rT, rC, boneColor, io);

    for (auto& b : bones) {
        if (!b.isOnScreen) continue;
        dl->AddCircleFilled(ImVec2(b.screenX, b.screenY), 2.5f, jointColor);
    }
}

void DrawPlayerBox(const PlayerInfo& player, const float hp, const uint32_t team,
                   const int localTeam, float distance) {
    if (hp <= 34.0f) return;

    BoneInfo head;
    if (!GetPlayerHeadBone(player, head)) return;  // ✅ SynthUtils

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImGuiIO& io = ImGui::GetIO();

    float hy, fx, fy;
    if (!WorldToScreen(head.x, head.y, head.z, fx, hy)) return;  // ✅ SynthUtils
    if (!WorldToScreen(player.X, player.Y, player.Z, fx, fy)) return;

    const float height = fabs(hy - fy);
    const float width  = height * 0.5f;
    const float x      = fx - width * 0.5f;
    float y            = fy - height;

    constexpr float barW = 4.0f;
    const float barH = height;
    const float barX = x - barW - 2.0f;

    dl->AddRectFilled(ImVec2(barX, y), ImVec2(barX + barW, y + barH), IM_COL32(0, 0, 0, 180));

    const float hpRatio = std::clamp(hp / 100.0f, 0.0f, 1.0f);
    dl->AddRectFilled(
        ImVec2(barX, y + barH * (1.0f - hpRatio)),
        ImVec2(barX + barW, y + barH),
        GetHealthGradientColor(hpRatio));

    const ImU32 boxColor = (team != localTeam)
        ? IM_COL32(g_EnemyBoxColor[0], g_EnemyBoxColor[1], g_EnemyBoxColor[2], 255)
        : IM_COL32(g_FriendBoxColor[0], g_FriendBoxColor[1], g_FriendBoxColor[2], 255);

    dl->AddRect(ImVec2(x, y), ImVec2(x + width, y + height), boxColor, 0, 0, 2.0f);
    dl->AddLine(ImVec2(io.DisplaySize.x * 0.5f, 0), ImVec2(fx, fy - height), boxColor, 1.2f);

    char txt[16];
    sprintf_s(txt, "HP: %.0f", hp);
    dl->AddText(ImVec2(fx - 20, fy - height - 14), boxColor, txt);
}

void ESP::Render() {
    if (!isESP) return;
    if (!GetViewMatrix()) return;

    int localTeam = 0;
    if (const uintptr_t a = Memory::ResolveAddress(TEAM_FRIEND_EXPR))
        localTeam = *reinterpret_cast<int*>(a);

    auto players = GetPlayers();
    if (players.empty()) return;

    if (const uintptr_t lp = Memory::ResolveAddress(LOCAL_PLAYER_POSITION_EXPR))
        for (int i = 0; i < 3; i++)
            Memory::SafeReadFloat(lp + i * sizeof(float), LocalPlayerPosition[i]);

    std::vector<BoneInfo> allBones;
    CollectAllBones(allBones);

    for (auto& b : allBones)
        b.isOnScreen = WorldToScreen(b.x, b.y, b.z, b.screenX, b.screenY);

    DistributeBonesToPlayers(players, allBones);

    for (auto& p : players) {
        if (!p.isValid) continue;
        if (p.HP <= 34.0f) continue;

        const bool enemy = p.Team != localTeam;
        if (enemy && ! isShowEnemy) continue;
        if (!enemy && !isShowFriend) continue;

        p.distance = CalculateDistance(
            LocalPlayerPosition[0], LocalPlayerPosition[1], LocalPlayerPosition[2],
            p.X, p.Y, p.Z);

        DrawPlayerSkeleton(p.bones, p.HP, p.distance);
        DrawPlayerBox(p, p.HP, p.Team, localTeam, p.distance);
    }
}