#include "menu.h"
#include "imgui.h"
#include "features.h"
#include "language.h"
#include "shared.h"
#include "features/Aimbot.h"
#include "features/MoveSpeed.h"
#include "fonts/noto_arabic_font.h"

bool menuVisible = true;

int g_BoxColor[3] = {0, 255, 0};
int g_BoneColor[3] = {255, 255, 255};
int g_JointColor[3] = {0, 200, 200};
int g_EnemyBoxColor[3] = {255, 0, 0};
int g_FriendBoxColor[3] = {0, 255, 0};
int g_FovCircleColor[3] = {255, 255, 255};

const char* languages[] = {
    "zh_CN", "简体中文",
    "en_US", "English",
    "ru_RU", "Русский",
    "ja_JP", "日本語",
    "fr_FR", "Français",
    "es_ES", "Español",
    "pt_BR", "Português (BR)",
    "ko_KR", "한국어",
    "ar_SA", "العربية"
};

static ImVector<ImWchar> BuildArabicRanges()
{
    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesChineseFull());
    builder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesKorean());
    builder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesCyrillic());
    builder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesDefault());

    for (ImWchar c = 0x0600; c <= 0x06FF; c++)
        builder.AddChar(c);
    for (ImWchar c = 0x0750; c <= 0x077F; c++)
        builder.AddChar(c);

    ImVector<ImWchar> ranges;
    builder.BuildRanges(&ranges);
    return ranges;
}

void UIMenu::Initialize()
{
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesChineseFull());
    builder.AddRanges(io.Fonts->GetGlyphRangesKorean());
    builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
    builder.AddRanges(io.Fonts->GetGlyphRangesDefault());

    ImVector<ImWchar> arabicRanges = BuildArabicRanges();

    for (ImWchar c = 0x0600; c <= 0x06FF; c++)
        builder.AddChar(c);
    for (ImWchar c = 0x0750; c <= 0x077F; c++)
        builder.AddChar(c);

    ImVector<ImWchar> finalRanges;
    builder.BuildRanges(&finalRanges);

    ImFontConfig baseConfig;
    baseConfig.SizePixels = 20.0f;
    io.Fonts->AddFontFromFileTTF(
        R"(C:\Windows\Fonts\msyh.ttc)",
        20.0f,
        &baseConfig,
        finalRanges.Data
    );

    ImFontConfig koreanConfig;
    koreanConfig.MergeMode = true;
    koreanConfig.SizePixels = 20.0f;
    io.Fonts->AddFontFromFileTTF(
        R"(C:\Windows\Fonts\malgun.ttf)",
        20.0f,
        &koreanConfig,
        io.Fonts->GetGlyphRangesKorean()
    );

    ImFontConfig arabicConfig;
    arabicConfig.MergeMode = true;
    arabicConfig.SizePixels = 26.0f;

    io.Fonts->AddFontFromMemoryTTF(
        NotoSansArabic_Regular_ttf,
        NotoSansArabic_Regular_ttf_len,
        26.0f,
        &arabicConfig,
        arabicRanges.Data
    );

    Language::Instance().Load("zh_CN");
}

void UIMenu::Shutdown() {
}

void UIMenu::Render() {
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(80, 80), ImGuiCond_FirstUseEver);

    ImGui::Begin(LOC("menu.title"));

    if (ImGui::IsKeyPressed(ImGuiKey_Insert)) {
        menuVisible = !menuVisible;
    }

    if (!menuVisible) {
        ImGui::End();
        Features::FeaturesTick();
        return;
    }

    if (ImGui::BeginTabBar("")) {
        if (ImGui::BeginTabItem(LOC("tab.general"))) {
            ImGui::Checkbox(LOC("general.bot_lobby"), &isBotRoom);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(LOC("tab.battle"))) {
            ImGui::Separator();
            ImGui::Checkbox(LOC("battle.aimbot"), &isAimbot);
            ImGui::SliderFloat(LOC("battle.aim_range"), &Aimbot::g_aimFov, 100.0f, 400.0f, "%.0f");
            ImGui::SliderFloat(LOC("battle.aim_factor"), &Aimbot::smooth, 0.05f, 0.35f, "%.2f");
            ImGui::Checkbox(LOC("battle.radar"), &isRadar);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(LOC("battle.radar.tips"));
            }
            ImGui::Checkbox(LOC("battle.infinite_ammo"), &isInfiniteAmmo);
            ImGui::SameLine();
            ImGui::Checkbox(LOC("battle.infinite_ammo_expert"), &isInfiniteAmmoExpert);
            ImGui::Checkbox(LOC("battle.accuracy"), &isAccuracy);
            ImGui::Checkbox(LOC("battle.recoilless"), &isRecoilless);
            ImGui::Checkbox(LOC("battle.rapid_fire"), &isRapidFire);
            ImGui::Checkbox(LOC("battle.move_speed"), &isMoveSpeed);
            ImGui::SameLine();
            ImGui::SliderFloat(LOC("battle.move_speed.desc"), &MoveSpeed::speed, 1.0f, 10.0f, "%.1f");
            ImGui::Checkbox(LOC("battle.fast_respawn"), &isFastRespawn);
            ImGui::Checkbox(LOC("battle.ghost"), &isGhost);
            ImGui::SameLine();
            ImGui::Checkbox(LOC("battle.teleport_to_enemies_head"), &isFury);
            ImGui::SameLine();
            ImGui::TextDisabled(LOC("battle.teleport_to_enemies_head.desc"));
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(LOC("battle.teleport_to_enemies_head.tips"));
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(LOC("tab.esp"))) {
            ImGui::Checkbox(LOC("esp.enable"), &isESP);
            ImGui::Checkbox(LOC("esp.enemy"), &g_showEnemy);
            ImGui::SameLine();
            ImGui::Checkbox(LOC("esp.friend"), &g_showFriend);
            ImGui::Separator();
            ImGui::Text(LOC("esp.box_color"));
            ImGui::Separator();

            ImGui::Text(LOC("esp.box_color.enemy"));
            float enemyBoxPreview[3] = {
                g_EnemyBoxColor[0] / 255.0f, g_EnemyBoxColor[1] / 255.0f, g_EnemyBoxColor[2] / 255.0f
            };
            if (ImGui::ColorEdit3("##EnemyBoxPicker", enemyBoxPreview,
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_DisplayRGB)) {
                g_EnemyBoxColor[0] = static_cast<int>(enemyBoxPreview[0] * 255);
                g_EnemyBoxColor[1] = static_cast<int>(enemyBoxPreview[1] * 255);
                g_EnemyBoxColor[2] = static_cast<int>(enemyBoxPreview[2] * 255);
            }
            ImGui::SameLine();
            ImGui::PushItemWidth(80);
            ImGui::SliderInt("##EnemyR", &g_EnemyBoxColor[0], 0, 255);
            ImGui::SameLine();
            ImGui::SliderInt("##EnemyG", &g_EnemyBoxColor[1], 0, 255);
            ImGui::SameLine();
            ImGui::SliderInt("##EnemyB", &g_EnemyBoxColor[2], 0, 255);
            ImGui::PopItemWidth();

            ImGui::Text(LOC("esp.box_color.friend"));
            float teamBoxPreview[3] = {
                g_FriendBoxColor[0] / 255.0f, g_FriendBoxColor[1] / 255.0f, g_FriendBoxColor[2] / 255.0f
            };
            if (ImGui::ColorEdit3("##FriendBoxPicker", teamBoxPreview,
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_DisplayRGB)) {
                g_FriendBoxColor[0] = static_cast<int>(teamBoxPreview[0] * 255);
                g_FriendBoxColor[1] = static_cast<int>(teamBoxPreview[1] * 255);
                g_FriendBoxColor[2] = static_cast<int>(teamBoxPreview[2] * 255);
            }
            ImGui::SameLine();
            ImGui::PushItemWidth(80);
            ImGui::SliderInt("##FriendR", &g_FriendBoxColor[0], 0, 255);
            ImGui::SameLine();
            ImGui::SliderInt("##FriendG", &g_FriendBoxColor[1], 0, 255);
            ImGui::SameLine();
            ImGui::SliderInt("##FriendB", &g_FriendBoxColor[2], 0, 255);
            ImGui::PopItemWidth();

            ImGui::Text(LOC("esp.fov_circle"));
            float fovPreview[3] = {
                g_FovCircleColor[0] / 255.0f, g_FovCircleColor[1] / 255.0f, g_FovCircleColor[2] / 255.0f
            };
            if (ImGui::ColorEdit3("##FovPicker", fovPreview,
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_DisplayRGB)) {
                g_FovCircleColor[0] = static_cast<int>(fovPreview[0] * 255);
                g_FovCircleColor[1] = static_cast<int>(fovPreview[1] * 255);
                g_FovCircleColor[2] = static_cast<int>(fovPreview[2] * 255);
            }
            ImGui::SameLine();
            ImGui::PushItemWidth(80);
            ImGui::SliderInt("##FovR", &g_FovCircleColor[0], 0, 255);
            ImGui::SameLine();
            ImGui::SliderInt("##FovG", &g_FovCircleColor[1], 0, 255);
            ImGui::SameLine();
            ImGui::SliderInt("##FovB", &g_FovCircleColor[2], 0, 255);
            ImGui::PopItemWidth();

            ImGui::Text(LOC("esp.bone_color"));
            float bonePreview[3] = {g_BoneColor[0] / 255.0f, g_BoneColor[1] / 255.0f, g_BoneColor[2] / 255.0f};
            if (ImGui::ColorEdit3("##BonePicker", bonePreview,
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_DisplayRGB)) {
                g_BoneColor[0] = static_cast<int>(bonePreview[0] * 255);
                g_BoneColor[1] = static_cast<int>(bonePreview[1] * 255);
                g_BoneColor[2] = static_cast<int>(bonePreview[2] * 255);
            }
            ImGui::SameLine();
            ImGui::PushItemWidth(80);
            ImGui::SliderInt("##BoneR", &g_BoneColor[0], 0, 255);
            ImGui::SameLine();
            ImGui::SliderInt("##BoneG", &g_BoneColor[1], 0, 255);
            ImGui::SameLine();
            ImGui::SliderInt("##BoneB", &g_BoneColor[2], 0, 255);
            ImGui::PopItemWidth();

            ImGui::Text(LOC("esp.joint_color"));
            float jointPreview[3] = {g_JointColor[0] / 255.0f, g_JointColor[1] / 255.0f, g_JointColor[2] / 255.0f};
            if (ImGui::ColorEdit3("##JointPicker", jointPreview,
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_DisplayRGB)) {
                g_JointColor[0] = static_cast<int>(jointPreview[0] * 255);
                g_JointColor[1] = static_cast<int>(jointPreview[1] * 255);
                g_JointColor[2] = static_cast<int>(jointPreview[2] * 255);
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

        if (ImGui::BeginTabItem(LOC("tab.misc"))) {
            // ImGui::Checkbox(LOC("misc.full_core"), &isFullCores);
            ImGui::Checkbox(LOC("misc.battle_pass"), &isBattlePass);
            ImGui::Checkbox(LOC("misc.assassination_mission"), &isAssassination);
            ImGui::Checkbox(LOC("misc.vip"), &isVIP);
            ImGui::SameLine();
            ImGui::TextDisabled(LOC("misc.vip.desc"));
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(LOC("misc.vip.tips"));
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(LOC("tab.settings"))) {
            if (ImGui::CollapsingHeader(LOC("settings.language"))) {
                static int currentLang = 0;
                for (int i = 0; i < IM_ARRAYSIZE(languages); i += 2) {
                    if (ImGui::RadioButton(languages[i + 1], currentLang == i / 2)) {
                        currentLang = i / 2;
                        Language::Instance().Load(languages[i]);
                    }
                }
            }
            if (ImGui::CollapsingHeader(LOC("settings.opacity"))) {
                ImGui::SliderFloat(LOC("settings.focused"), &g_FocusedAlpha, 0.1f, 1.0f, "%.2f");
                ImGui::SliderFloat(LOC("settings.unfocused"), &g_UnfocusedAlpha, 0.1f, 1.0f, "%.2f");
                ImGui::SliderFloat(LOC("settings.transition"), &g_AlphaTransitionSpeed, 1.0f, 20.0f, "%.1f");

                if (ImGui::Button(LOC("settings.apply"))) {
                    g_TargetAlpha = g_ImGuiHasFocus ? g_FocusedAlpha : g_UnfocusedAlpha;
                }

                ImGui::Text(g_ImGuiHasFocus
                                ? LOC("settings.current_status.1")
                                : LOC("settings.current_status.0"));
                ImGui::Text(LOC("settings.current_opacity"), g_WindowAlpha);
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
    Features::FeaturesTick();
}
