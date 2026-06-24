#include "menu.h"
#include "imgui.h"
#include "features.h"
#include "features/Aimbot.h"
#include "features/MoveSpeed.h"

bool menuVisible = true;

int g_BoxColor[3] = {0, 255, 0};
int g_BoneColor[3] = {255, 255, 255};
int g_JointColor[3] = {0, 200, 200};
int g_EnemyBoxColor[3] = {255, 0, 0};
int g_FriendBoxColor[3] = {0, 255, 0};
int g_FovCircleColor[3] = {255, 255, 255};

void UIMenu::Initialize()
{
}

void UIMenu::Shutdown()
{
}

void UIMenu::Render()
{
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(80, 80), ImGuiCond_FirstUseEver);

    ImGui::Begin("Synth by KyleCyaN");

    if (ImGui::IsKeyPressed(ImGuiKey_Insert))
    {
        menuVisible = !menuVisible;
    }

    if (!menuVisible)
    {
        ImGui::End();
        Features::FeaturesTick();
        return;
    }

    if (ImGui::BeginTabBar(""))
    {
        if (ImGui::BeginTabItem("General"))
        {
            ImGui::Checkbox("Bot Lobby", &isBotRoom);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Battle"))
        {
            ImGui::Separator();
            ImGui::Checkbox("Aim Assist", &isAimbot);
            ImGui::SliderFloat("Aim Range", &Aimbot::g_aimFov, 100.0f, 400.0f, "%.0f");
            ImGui::SliderFloat("Aim Factor", &Aimbot::smooth, 0.05f, 0.35f, "%.2f");
            ImGui::Checkbox("Radar", &isRadar);
            ImGui::Checkbox("Infinite Ammo", &isInfiniteAmmo);
            ImGui::SameLine();
            ImGui::Checkbox("Infinite Ammo (Enhanced)", &isInfiniteAmmoExpert);
            ImGui::Checkbox("Full Accuracy", &isAccuracy);
            ImGui::Checkbox("Recoilless", &isRecoilless);
            ImGui::Checkbox("Rapid Fire", &isRapidFire);
            ImGui::Checkbox("Super Movement", &isMoveSpeed);
            ImGui::SameLine();
            ImGui::SliderFloat("Movement Speed", &MoveSpeed::speed, 1.0f, 10.0f, "%.1f");
            ImGui::Checkbox("Deploy Immediately", &isFastRespawn);
            ImGui::Checkbox("Ghost", &isGhost);
            ImGui::SameLine();
            ImGui::Checkbox("Teleport to enemy's head", &isFury);
            ImGui::SameLine();
            ImGui::TextDisabled("(Need to enable Ghost)");
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip(
                    "You must enable ghost mode before teleporting to the enemy's head, otherwise you will disconnect.");
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("ESP"))
        {
            ImGui::Checkbox("Enable ESP", &isESP);
            ImGui::Checkbox("Display Enemies", &g_showEnemy);
            ImGui::SameLine();
            ImGui::Checkbox("Display Friends", &g_showFriend);
            ImGui::Separator();
            ImGui::Text("Box Color");
            ImGui::Separator();

            ImGui::Text("Enemy");
            float enemyBoxPreview[3] = {
                g_EnemyBoxColor[0] / 255.0f, g_EnemyBoxColor[1] / 255.0f, g_EnemyBoxColor[2] / 255.0f
            };
            if (ImGui::ColorEdit3("##EnemyBoxPicker", enemyBoxPreview,
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_DisplayRGB))
            {
                g_EnemyBoxColor[0] = (int)(enemyBoxPreview[0] * 255);
                g_EnemyBoxColor[1] = (int)(enemyBoxPreview[1] * 255);
                g_EnemyBoxColor[2] = (int)(enemyBoxPreview[2] * 255);
            }
            ImGui::SameLine();
            ImGui::PushItemWidth(80);
            ImGui::SliderInt("##EnemyR", &g_EnemyBoxColor[0], 0, 255);
            ImGui::SameLine();
            ImGui::SliderInt("##EnemyG", &g_EnemyBoxColor[1], 0, 255);
            ImGui::SameLine();
            ImGui::SliderInt("##EnemyB", &g_EnemyBoxColor[2], 0, 255);
            ImGui::PopItemWidth();

            ImGui::Text("Friend");
            float teamBoxPreview[3] = {
                g_FriendBoxColor[0] / 255.0f, g_FriendBoxColor[1] / 255.0f, g_FriendBoxColor[2] / 255.0f
            };
            if (ImGui::ColorEdit3("##TeamBoxPicker", teamBoxPreview,
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_DisplayRGB))
            {
                g_FriendBoxColor[0] = (int)(teamBoxPreview[0] * 255);
                g_FriendBoxColor[1] = (int)(teamBoxPreview[1] * 255);
                g_FriendBoxColor[2] = (int)(teamBoxPreview[2] * 255);
            }
            ImGui::SameLine();
            ImGui::PushItemWidth(80);
            ImGui::SliderInt("##TeamR", &g_FriendBoxColor[0], 0, 255);
            ImGui::SameLine();
            ImGui::SliderInt("##TeamG", &g_FriendBoxColor[1], 0, 255);
            ImGui::SameLine();
            ImGui::SliderInt("##TeamB", &g_FriendBoxColor[2], 0, 255);
            ImGui::PopItemWidth();

            ImGui::Text("FOV Circle Color");
            float fovPreview[3] = {
                g_FovCircleColor[0] / 255.0f, g_FovCircleColor[1] / 255.0f, g_FovCircleColor[2] / 255.0f
            };
            if (ImGui::ColorEdit3("##FovPicker", fovPreview,
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_DisplayRGB))
            {
                g_FovCircleColor[0] = (int)(fovPreview[0] * 255);
                g_FovCircleColor[1] = (int)(fovPreview[1] * 255);
                g_FovCircleColor[2] = (int)(fovPreview[2] * 255);
            }
            ImGui::SameLine();
            ImGui::PushItemWidth(80);
            ImGui::SliderInt("##FovR", &g_FovCircleColor[0], 0, 255);
            ImGui::SameLine();
            ImGui::SliderInt("##FovG", &g_FovCircleColor[1], 0, 255);
            ImGui::SameLine();
            ImGui::SliderInt("##FovB", &g_FovCircleColor[2], 0, 255);
            ImGui::PopItemWidth();

            ImGui::Text("Bones Color");
            float bonePreview[3] = {g_BoneColor[0] / 255.0f, g_BoneColor[1] / 255.0f, g_BoneColor[2] / 255.0f};
            if (ImGui::ColorEdit3("##BonePicker", bonePreview,
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_DisplayRGB))
            {
                g_BoneColor[0] = (int)(bonePreview[0] * 255);
                g_BoneColor[1] = (int)(bonePreview[1] * 255);
                g_BoneColor[2] = (int)(bonePreview[2] * 255);
            }
            ImGui::SameLine();
            ImGui::PushItemWidth(80);
            ImGui::SliderInt("##BoneR", &g_BoneColor[0], 0, 255);
            ImGui::SameLine();
            ImGui::SliderInt("##BoneG", &g_BoneColor[1], 0, 255);
            ImGui::SameLine();
            ImGui::SliderInt("##BoneB", &g_BoneColor[2], 0, 255);
            ImGui::PopItemWidth();

            ImGui::Text("Joint Node Color");
            float jointPreview[3] = {g_JointColor[0] / 255.0f, g_JointColor[1] / 255.0f, g_JointColor[2] / 255.0f};
            if (ImGui::ColorEdit3("##JointPicker", jointPreview,
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_DisplayRGB))
            {
                g_JointColor[0] = (int)(jointPreview[0] * 255);
                g_JointColor[1] = (int)(jointPreview[1] * 255);
                g_JointColor[2] = (int)(jointPreview[2] * 255);
            }
            ImGui::SameLine();
            ImGui::PushItemWidth(80);
            ImGui::SliderInt("##JointR", &g_JointColor[0], 0, 255);
            ImGui::SameLine();
            ImGui::SliderInt("##JointG", &g_JointColor[1], 0, 255);
            ImGui::SameLine();
            ImGui::SliderInt("##JointB", &g_JointColor[2], 0, 255);
            ImGui::PopItemWidth();


            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Miscellaneous"))
        {
            // ImGui::Checkbox("Enable 5 Cores for Armor", &isFullCores);
            ImGui::Checkbox("Unlock 5K Credits for Battle Pass", &isBattlePass);
            ImGui::Checkbox("Instantly Complete Assassination Mission", &isAssassination);
            ImGui::Checkbox("Get VIP Points in High-Roll Credits Event ", &isVIP);
            ImGui::SameLine();
            ImGui::TextDisabled("(You need to enable the Instantly Complete Assassination Mission feature first)");
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip(
                    "You need to have a high-rolls credits event in order to get VIP, and you must first enable the instantly complete assassination mission feature.");
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Settings"))
        {
            if (ImGui::CollapsingHeader("Opacity Settings"))
            {
                ImGui::SliderFloat("Opacity when focused", &g_FocusedAlpha, 0.1f, 1.0f, "%.2f");
                ImGui::SliderFloat("Opacity when not focused", &g_UnfocusedAlpha, 0.1f, 1.0f, "%.2f");
                ImGui::SliderFloat("Transition Speed", &g_AlphaTransitionSpeed, 1.0f, 20.0f, "%.1f");

                if (ImGui::Button("Apply Settings"))
                {
                    g_TargetAlpha = g_ImGuiHasFocus ? g_FocusedAlpha : g_UnfocusedAlpha;
                }

                ImGui::Text("Current Status: %s", g_ImGuiHasFocus ? "Focused" : "Not Focused");
                ImGui::Text("Current Opacity: %.3f", g_WindowAlpha);
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
    Features::FeaturesTick();
}
