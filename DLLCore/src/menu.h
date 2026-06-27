#pragma once

extern bool menuVisible;

namespace UIMenu
{
    void Render();
    void Initialize();
    void Shutdown();
}

inline extern bool g_IsChinese = false;
inline extern float g_WindowAlpha = 1.0f;
inline extern float g_TargetAlpha = 1.0f;
inline extern float g_AlphaTransitionSpeed = 5.0f;
inline extern bool g_ImGuiHasFocus = false;
inline extern float g_FocusedAlpha = 1.0f;
inline extern float g_UnfocusedAlpha = 0.5f;

extern int g_BoxColor[3];
extern int g_BoneColor[3];
extern int g_JointColor[3];
extern int g_EnemyBoxColor[3];
extern int g_FriendBoxColor[3];
extern int g_FovCircleColor[3];
