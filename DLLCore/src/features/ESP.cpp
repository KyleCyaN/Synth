#include <windows.h>
#include <cstdio>
#include "ESP.h"

#include <algorithm>
#include <vector>
#include <cmath>
#include "Players.h"
#include "Bones.h"
#include "Aimbot.h"
#include "features.h"
#include "imgui.h"
#include "memory.h"
#include "menu.h"

std::vector<BoneInfo> g_headBones;

float ViewMatrix[16];
float LocalPlayerPosition[3];

bool GetViewMatrix()
{
    static uintptr_t cached = 0;
    static DWORD last = 0;
    DWORD now = GetTickCount();
    if (now - last > 1000)
    {
        cached = Memory::ResolveAddress(VIEW_MATRIX_EXPR);
        last = now;
    }
    if (!cached) return false;
    for (int i = 0; i < 16; ++i)
        if (!Memory::SafeReadFloat(cached + i * sizeof(float), ViewMatrix[i]))
            return false;
    return true;
}

bool WorldToScreenPoint(float x, float y, float z, float& sx, float& sy)
{
    float w =
        ViewMatrix[3] * x + ViewMatrix[7] * y + ViewMatrix[11] * z + ViewMatrix[15];
    if (w < 0.01f) return false;

    float cx =
        ViewMatrix[0] * x + ViewMatrix[4] * y + ViewMatrix[8] * z + ViewMatrix[12];
    float cy =
        ViewMatrix[1] * x + ViewMatrix[5] * y + ViewMatrix[9] * z + ViewMatrix[13];

    ImGuiIO& io = ImGui::GetIO();
    sx = (io.DisplaySize.x * 0.5f) * (1.0f + cx / w);
    sy = (io.DisplaySize.y * 0.5f) * (1.0f - cy / w);
    return true;
}

float CalculateDistance(float x1, float y1, float z1, float x2, float y2, float z2)
{
    float dx = x2 - x1, dy = y2 - y1, dz = z2 - z1;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

bool GetHeadForPlayer(const PlayerInfo& player, BoneInfo& outHead)
{
    float minDist = 2.0f;
    bool found = false;
    for (const auto& bone : player.bones)
    {
        if (bone.boneName != "Bip01_Head") continue;
        float d = CalculateDistance(player.X, player.Y, player.Z, bone.x, bone.y, bone.z);
        if (d < minDist)
        {
            minDist = d;
            outHead = bone;
            found = true;
        }
    }
    return found;
}

inline ImU32 GetHealthGradientColor(float p)
{
    int r = (int)(255.0f * (1.0f - p));
    int g = (int)(255.0f * p);
    return IM_COL32(r, g, 0, 255);
}

void DrawPlayerSkeleton(const std::vector<BoneInfo>& bones, float hp, float distance)
{
    if (hp <= 34.0f || bones.empty()) return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    int alpha = (distance > 50.f) ? 100 : (distance > 30.f) ? 160 : 210;

    ImU32 boneColor = IM_COL32(g_BoneColor[0], g_BoneColor[1], g_BoneColor[2], alpha);
    ImU32 jointColor = IM_COL32(g_JointColor[0], g_JointColor[1], g_JointColor[2], alpha);

    auto findBone = [&](const char* n) -> const BoneInfo*
    {
        for (auto& b : bones) if (b.boneName == n) return &b;
        return nullptr;
    };

    const BoneInfo* head = findBone("Bip01_Head");
    const BoneInfo* spine = findBone("Bip01_Spine");
    const BoneInfo* pelvis = findBone("Bip01_Pelvis");
    const BoneInfo* lUA = findBone("Bip01_L_UpperArm");
    const BoneInfo* rUA = findBone("Bip01_R_UpperArm");
    const BoneInfo* lFA = findBone("Bip01_L_Forearm");
    const BoneInfo* rFA = findBone("Bip01_R_Forearm");
    const BoneInfo* lT = findBone("Bip01_L_Thigh");
    const BoneInfo* rT = findBone("Bip01_R_Thigh");
    const BoneInfo* lC = findBone("Bip01_L_Calf");
    const BoneInfo* rC = findBone("Bip01_R_Calf");

    auto line = [&](const BoneInfo* a, const BoneInfo* b, ImU32 c, const ImGuiIO& io)
    {
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

    for (auto& b : bones)
    {
        if (!b.isOnScreen) continue;
        dl->AddCircleFilled(ImVec2(b.screenX, b.screenY), 2.5f, jointColor);
    }
}

void DrawPlayerBox(PlayerInfo& player, float hp, int team, int localTeam, float distance)
{
    if (hp <= 34.0f) return;

    BoneInfo head;
    if (!GetHeadForPlayer(player, head)) return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImGuiIO& io = ImGui::GetIO();

    float hx, hy, fx, fy;
    if (!WorldToScreenPoint(head.x, head.y, head.z, hx, hy)) return;
    if (!WorldToScreenPoint(player.X, player.Y, player.Z, fx, fy)) return;

    float height = fabs(hy - fy);
    float width = height * 0.5f;
    float x = fx - width * 0.5f;
    float y = fy - height;

    float barW = 4.0f;
    float barH = height;
    float barX = x - barW - 2.0f;

    dl->AddRectFilled(ImVec2(barX, y), ImVec2(barX + barW, y + barH), IM_COL32(0, 0, 0, 180));

    float hpRatio = std::clamp(hp / 100.0f, 0.0f, 1.0f);
    dl->AddRectFilled(
        ImVec2(barX, y + barH * (1.0f - hpRatio)),
        ImVec2(barX + barW, y + barH),
        GetHealthGradientColor(hpRatio)
    );

    ImU32 boxColor = (team != localTeam)
                         ? IM_COL32(g_EnemyBoxColor[0], g_EnemyBoxColor[1], g_EnemyBoxColor[2], 255)
                         : IM_COL32(g_FriendBoxColor[0], g_FriendBoxColor[1], g_FriendBoxColor[2], 255);

    dl->AddRect(ImVec2(x, y), ImVec2(x + width, y + height), boxColor, 0, 0, 2.0f);

    dl->AddLine(ImVec2(io.DisplaySize.x * 0.5f, 0), ImVec2(fx, fy - height), boxColor, 1.2f);

    char txt[16];
    sprintf_s(txt, "HP: %.0f", hp);
    dl->AddText(ImVec2(fx - 20, fy - height - 14), boxColor, txt);
}

void ESP::Render()
{
    if (!isESP) return;
    if (!GetViewMatrix()) return;

    int localTeam = 0;
    if (uintptr_t a = Memory::ResolveAddress(TEAM_FRIEND_EXPR))
        localTeam = *reinterpret_cast<int*>(a);

    auto players = GetPlayers();
    if (players.empty()) return;

    if (uintptr_t lp = Memory::ResolveAddress(LOCAL_PLAYER_POSITION_EXPR))
        for (int i = 0; i < 3; i++)
            Memory::SafeReadFloat(lp + i * sizeof(float), LocalPlayerPosition[i]);

    std::vector<BoneInfo> allBones;
    CollectAllBones(allBones);

    for (auto& b : allBones)
        b.isOnScreen = WorldToScreenPoint(b.x, b.y, b.z, b.screenX, b.screenY);

    DistributeBonesToPlayers(players, allBones);

    for (auto& p : players)
    {
        if (!p.isValid) continue;
        if (p.HP <= 34.0f) continue;

        const bool enemy = p.Team != localTeam;
        if (enemy && !g_showEnemy) continue;
        if (!enemy && !g_showFriend) continue;

        p.distance = CalculateDistance(
            LocalPlayerPosition[0], LocalPlayerPosition[1], LocalPlayerPosition[2],
            p.X, p.Y, p.Z
        );

        DrawPlayerSkeleton(p.bones, p.HP, p.distance);
        DrawPlayerBox(p, p.HP, p.Team, localTeam, p.distance);
    }

    if (isAimbot)
    {
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        ImGuiIO& io = ImGui::GetIO();
        dl->AddCircle(
            ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
            Aimbot::g_aimFov,
            IM_COL32(g_FovCircleColor[0], g_FovCircleColor[1], g_FovCircleColor[2], 100),
            64, 2.0f
        );
    }
}
