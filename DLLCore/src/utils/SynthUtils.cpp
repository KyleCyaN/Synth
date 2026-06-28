// DLLCore/src/features/SynthUtils.cpp
#include "SynthUtils.h"
#include "../features/Bones.h"          // CollectAllBones, DistributeBonesToPlayers
#include "../../include/imgui/imgui.h"
#include "../memory.h"
#include "../features.h"

// =====================================================================
// ViewMatrix
// =====================================================================
float ViewMatrix[16];

bool GetViewMatrix()
{
    static uintptr_t cached = 0;
    static DWORD last = 0;
    DWORD now = GetTickCount();
    if (now - last > 1000) {
        cached = Memory::ResolveAddress(VIEW_MATRIX_EXPR);
        last = now;
    }
    if (!cached) return false;
    for (int i = 0; i < 16; ++i)
        if (!Memory::SafeReadFloat(cached + i * sizeof(float), ViewMatrix[i]))
            return false;
    return true;
}

// =====================================================================
// 坐标转换 (统一版本 — 使用 column-major ViewMatrix)
// =====================================================================
bool WorldToScreen(float x, float y, float z, float& sx, float& sy)
{
    // ViewMatrix 是列主序: [col0, col1, col2, col3]
    // 等价于: m[row][col] = ViewMatrix[col * 4 + row]
    const float w = ViewMatrix[3]  * x + ViewMatrix[7]  * y +
                    ViewMatrix[11] * z + ViewMatrix[15];
    if (w < 0.01f) return false;

    const float cx = ViewMatrix[0]  * x + ViewMatrix[4]  * y +
                     ViewMatrix[8]  * z + ViewMatrix[12];
    const float cy = ViewMatrix[1]  * x + ViewMatrix[5]  * y +
                     ViewMatrix[9]  * z + ViewMatrix[13];

    const ImGuiIO& io = ImGui::GetIO();
    sx = (io.DisplaySize.x * 0.5f) * (1.0f + cx / w);
    sy = (io.DisplaySize.y * 0.5f) * (1.0f - cy / w);
    return true;
}

float CalculateDistance(float x1, float y1, float z1,
                        float x2, float y2, float z2)
{
    const float dx = x2 - x1;
    const float dy = y2 - y1;
    const float dz = z2 - z1;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

// =====================================================================
// 玩家/骨骼
// =====================================================================
bool GetPlayerHeadBone(const PlayerInfo& player, BoneInfo& outHead)
{
    for (const auto& bone : player.bones) {
        if (bone.boneName == "Bip01_Head") {
            outHead = bone;
            return true;
        }
    }
    return false;
}

// =====================================================================
// 游戏状态
// =====================================================================
bool IsAimingDownSight()
{
    static uintptr_t gameBase = reinterpret_cast<uintptr_t>(
        GetModuleHandleA("WindowsEntryPoint.Windows_W10.exe"));
    if (!gameBase) return false;

    const uintptr_t addr = gameBase + 0x01CF5D08;
    uintptr_t ptr1 = 0;
    if (!Memory::SafeReadPtr(addr, ptr1) || !ptr1) return false;
    return Memory::ReadInt(ptr1 + 0x278) != 0;
}

// =====================================================================
// 数学工具
// =====================================================================
float NormalizeAngle(float a)
{
    a = fmodf(a, 360.0f);
    if (a < 0.0f) a += 360.0f;
    return a;
}

// =====================================================================
// 内存工具
// =====================================================================
void* AllocateNear(uintptr_t target)
{
    for (int i = 0; i < 524288; ++i) {
        const int dir = (i % 2 == 0) ? 1 : -1;
        uintptr_t addr = target + dir * (i / 2) * 0x1000;
        addr &= ~static_cast<uintptr_t>(0xFFF);
        if (void* p = VirtualAlloc(reinterpret_cast<void*>(addr), 0x1000,
                                    MEM_COMMIT | MEM_RESERVE,
                                    PAGE_EXECUTE_READWRITE)) {
            const auto rel = static_cast<int64_t>(
                reinterpret_cast<uintptr_t>(p) - (target + 5));
            if (rel >= INT32_MIN && rel <= INT32_MAX)
                return p;
            VirtualFree(p, 0, MEM_RELEASE);
        }
    }
    return nullptr;
}