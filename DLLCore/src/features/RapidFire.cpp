#include "RapidFire.h"
#include "memory.h"
#include <Windows.h>
#include <cstdint>

static uintptr_t g_ModuleBase = 0;
static DWORD g_GlobalPointerOffset = 0x01CF36B8;
static bool g_Installed = false;
static uintptr_t g_HookAddr = 0;
static BYTE g_OrigBytes[7] = {0};
static LPVOID g_Shellcode = nullptr;

static LPVOID AllocateNear(uintptr_t target)
{
    for (int i = 0; i < 524288; ++i)
    {
        const int dir = (i % 2 == 0) ? 1 : -1;
        uintptr_t addr = target + dir * (i / 2) * 0x1000;
        addr &= ~static_cast<uintptr_t>(0xFFF);
        if (LPVOID p = VirtualAlloc(reinterpret_cast<LPVOID>(addr), 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE))
        {
            if (const auto rel = static_cast<int64_t>(reinterpret_cast<uintptr_t>(p) - (target + 5)); rel >= INT32_MIN && rel <= INT32_MAX)
                return p;
            VirtualFree(p, 0, MEM_RELEASE);
        }
    }
    return nullptr;
}

void RapidFire::Install()
{
    if (g_Installed) return;

    g_ModuleBase = reinterpret_cast<uintptr_t>(GetModuleHandleA("WindowsEntryPoint.Windows_W10.exe"));
    if (!g_ModuleBase) return;

    const BYTE pattern[] = {0xF6, 0x4C, 0x63, 0x7A, 0x08, 0x4D, 0x8B, 0xE0};
    const char* mask = "xxxxxxxx";
    const uintptr_t rapidFire = Memory::FindPattern("WindowsEntryPoint.Windows_W10.exe", pattern, mask);
    if (!rapidFire) return;
    g_HookAddr = rapidFire + 1;

    g_Shellcode = AllocateNear(g_HookAddr);
    if (!g_Shellcode)
    {
        return;
    }

    auto code = static_cast<BYTE*>(g_Shellcode);
    size_t i = 0;

    // push rbx
    code[i++] = 0x53;

    // mov rbx, [rip + disp]
    code[i++] = 0x48;
    code[i++] = 0x8B;
    code[i++] = 0x1D;
    const uintptr_t targetAddr = g_ModuleBase + g_GlobalPointerOffset;
    const auto disp1 = static_cast<int32_t>(targetAddr - (reinterpret_cast<uintptr_t>(code + i) + 4));
    memcpy(&code[i], &disp1, 4);
    i += 4;

    for (DWORD chain[] = {0x308, 0x90, 0x160, 0x38, 0x4E0, 0x8}; DWORD off : chain)
    {
        code[i++] = 0x48;
        code[i++] = 0x8B;
        code[i++] = 0x9B;
        memcpy(&code[i], &off, 4);
        i += 4;
    }

    // add rbx, 40
    code[i++] = 0x48;
    code[i++] = 0x83;
    code[i++] = 0xC3;
    code[i++] = 0x40;

    // cmp rbx, rdx
    code[i++] = 0x48;
    code[i++] = 0x39;
    code[i++] = 0xD3;

    // pop rbx
    code[i++] = 0x5B;

    // jne to original
    const size_t jnePos = i;
    code[i++] = 0x0F;
    code[i++] = 0x85;
    const size_t jneOffsetPos = i;
    i += 4;

    // cmp byte ptr [rdx+8], 0B
    code[i++] = 0x80;
    code[i++] = 0x7A;
    code[i++] = 0x08;
    code[i++] = 0x0B;
    // je modify
    const size_t je1Pos = i;
    code[i++] = 0x0F;
    code[i++] = 0x84;
    const size_t je1OffsetPos = i;
    i += 4;

    // cmp byte ptr [rdx+8], 10
    code[i++] = 0x80;
    code[i++] = 0x7A;
    code[i++] = 0x08;
    code[i++] = 0x10;
    // je modify
    const size_t je2Pos = i;
    code[i++] = 0x0F;
    code[i++] = 0x84;
    const size_t je2OffsetPos = i;
    i += 4;

    // jmp original
    const size_t jmpPos = i;
    code[i++] = 0xE9;
    const size_t jmpOffsetPos = i;
    i += 4;

    // modify:
    const size_t modifyPos = i;
    code[i++] = 0xC6;
    code[i++] = 0x42;
    code[i++] = 0x08;
    code[i++] = 0x03;
    // jmp original
    code[i++] = 0xE9;
    const size_t jmp2OffsetPos = i;
    i += 4;

    const size_t originalPos = i;
    // movsxd r15, [rdx+8]
    code[i++] = 0x4C;
    code[i++] = 0x63;
    code[i++] = 0x7A;
    code[i++] = 0x08;
    // mov r12, r8
    code[i++] = 0x4D;
    code[i++] = 0x8B;
    code[i++] = 0xE0;

    // jmp [rip+0] + ret addr
    code[i++] = 0xFF;
    code[i++] = 0x25;
    constexpr DWORD zero = 0;
    memcpy(&code[i], &zero, 4);
    i += 4;
    const uintptr_t retAddr = g_HookAddr + 7;
    memcpy(&code[i], &retAddr, 8);
    i += 8;

    *reinterpret_cast<int32_t*>(code + jneOffsetPos) = static_cast<int32_t>(originalPos - (jnePos + 6));
    *reinterpret_cast<int32_t*>(code + je1OffsetPos) = static_cast<int32_t>(modifyPos - (je1Pos + 6));
    *reinterpret_cast<int32_t*>(code + je2OffsetPos) = static_cast<int32_t>(modifyPos - (je2Pos + 6));
    *reinterpret_cast<int32_t*>(code + jmpOffsetPos) = static_cast<int32_t>(originalPos - (jmpPos + 5));
    *reinterpret_cast<int32_t*>(code + jmp2OffsetPos) = static_cast<int32_t>(originalPos - (modifyPos + 9));


    DWORD oldProt;
    VirtualProtect(reinterpret_cast<LPVOID>(g_HookAddr), 7, PAGE_EXECUTE_READWRITE, &oldProt);
    memcpy(g_OrigBytes, reinterpret_cast<LPVOID>(g_HookAddr), 7);
    const auto rel = static_cast<int64_t>(reinterpret_cast<uintptr_t>(g_Shellcode) - (g_HookAddr + 5));
    const auto r32 = static_cast<int32_t>(rel);
    BYTE jmp[5] = {0xE9};
    memcpy(&jmp[1], &r32, 4);
    memcpy(reinterpret_cast<LPVOID>(g_HookAddr), jmp, 5);
    VirtualProtect(reinterpret_cast<LPVOID>(g_HookAddr), 7, oldProt, &oldProt);
    g_Installed = true;
}

void RapidFire::Uninstall()
{
    if (!g_Installed) return;
    DWORD oldProt;
    VirtualProtect(reinterpret_cast<LPVOID>(g_HookAddr), 7, PAGE_EXECUTE_READWRITE, &oldProt);
    memcpy(reinterpret_cast<LPVOID>(g_HookAddr), g_OrigBytes, 7);
    VirtualProtect(reinterpret_cast<LPVOID>(g_HookAddr), 7, oldProt, &oldProt);
    if (g_Shellcode)
    {
        VirtualFree(g_Shellcode, 0, MEM_RELEASE);
        g_Shellcode = nullptr;
    }
    g_Installed = false;
}
