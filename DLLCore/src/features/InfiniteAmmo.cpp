#include "InfiniteAmmo.h"
#include "memory.h"
#include <Windows.h>
#include <cstdint>
#include <vector>

struct AmmoHookInfo
{
    uintptr_t hookAddr = 0;
    BYTE origBytes[12] = {0};
    size_t origBytesSize = 0;
    LPVOID shellcode = nullptr;
    size_t shellcodeSize = 0;
    bool installed = false;
};

static std::vector<AmmoHookInfo> g_Hooks(5);
static bool g_GlobalInstalled = false;
static uintptr_t g_ModuleBase = 0;
static int g_AmmoValue = 0;

void InfiniteAmmo::SetAmmoValue(int ammo)
{
    g_AmmoValue = ammo;
}

static LPVOID AllocateNear(uintptr_t target, size_t size)
{
    for (int i = 0; i < 524288; ++i)
    {
        const int dir = (i % 2 == 0) ? 1 : -1;
        uintptr_t addr = target + dir * (i / 2) * 0x1000;
        addr &= ~static_cast<uintptr_t>(0xFFF);
        if (LPVOID p = VirtualAlloc(reinterpret_cast<LPVOID>(addr), size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE))
        {
            if (const auto rel = static_cast<int64_t>(reinterpret_cast<uintptr_t>(p) - (target + 5)); rel >= INT32_MIN && rel <= INT32_MAX)
                return p;
            VirtualFree(p, 0, MEM_RELEASE);
        }
    }
    return nullptr;
}

static bool GenerateShellcodeForHook(AmmoHookInfo& hookInfo, int hookIndex)
{
    const auto code = static_cast<BYTE*>(hookInfo.shellcode);
    size_t i = 0;

    switch (hookIndex)
    {
    case 0:
        {
            code[i++] = 0xC7;
            code[i++] = 0x43;
            code[i++] = 0x28;
            memcpy(&code[i], &g_AmmoValue, 4);
            i += 4;

            // movss xmm1, [rbx+28]
            code[i++] = 0xF3;
            code[i++] = 0x0F;
            code[i++] = 0x10;
            code[i++] = 0x4B;
            code[i++] = 0x28;
            code[i++] = 0xE9; // jmp

            hookInfo.origBytesSize = 5;
            hookInfo.shellcodeSize = i + 4;
            return true;
        }

    case 1:
        {
            // mov edx, imm32
            code[i++] = 0xBA;
            memcpy(&code[i], &g_AmmoValue, 4);
            i += 4;
            // mov [rbx+000002A4], edx
            code[i++] = 0x89;
            code[i++] = 0x93;
            code[i++] = 0xA4;
            code[i++] = 0x02;
            code[i++] = 0x00;
            code[i++] = 0x00;
            code[i++] = 0xE9;

            hookInfo.origBytesSize = 6;
            hookInfo.shellcodeSize = i + 4;
            return true;
        }

    case 2:
        {
            // mov eax, imm32
            code[i++] = 0xB8;
            int value = 1;
            memcpy(&code[i], &value, 4);
            i += 4;

            // mov [rcx], eax
            code[i++] = 0x89;
            code[i++] = 0x01;

            // cmp byte ptr [r13+000003C8],00
            code[i++] = 0x41;
            code[i++] = 0x80;
            code[i++] = 0xBD;
            code[i++] = 0xC8;
            code[i++] = 0x03;
            code[i++] = 0x00;
            code[i++] = 0x00;
            code[i++] = 0x00;

            code[i++] = 0xE9; // jmp

            hookInfo.origBytesSize = 10;
            hookInfo.shellcodeSize = i + 4;

            return true;
        }


    case 3:
        {
            // mov edx, imm32
            code[i++] = 0xBA;
            memcpy(&code[i], &g_AmmoValue, 4);
            i += 4;
            // mov [rcx+000002A4], edx
            code[i++] = 0x89;
            code[i++] = 0x91;
            code[i++] = 0xA4;
            code[i++] = 0x02;
            code[i++] = 0x00;
            code[i++] = 0x00;
            code[i++] = 0xE9;

            hookInfo.origBytesSize = 6;
            hookInfo.shellcodeSize = i + 4;
            return true;
        }

    case 4:
        {
            code[i++] = 0xC7;
            code[i++] = 0x41;
            code[i++] = 0x68;
            const int value = 1;
            memcpy(&code[i], &value, 4);
            i += 4;
            // mov eax, [rcx+68]
            code[i++] = 0x8B;
            code[i++] = 0x41;
            code[i++] = 0x68;
            // imul edx, eax
            code[i++] = 0x0F;
            code[i++] = 0xAF;
            code[i++] = 0xD0;
            code[i++] = 0xE9;

            hookInfo.origBytesSize = 6;
            hookInfo.shellcodeSize = i + 4;
            return true;
        }


    default:
        return false;
    }
}

void InfiniteAmmo::Install()
{
    if (g_GlobalInstalled) return;

    g_ModuleBase = reinterpret_cast<uintptr_t>(GetModuleHandleA("WindowsEntryPoint.Windows_W10.exe"));
    if (!g_ModuleBase)
    {
        return;
    }

    struct AobPattern
    {
        BYTE pattern[24];
        const char* mask;
        size_t patternLen;
    };

    for (int idx = 0; idx < 5; ++idx)
    {
        AobPattern patterns[] = {
            {{0xF3, 0x0F, 0x10, 0x4B, 0x28, 0x0F, 0xAF}, "xxxxxxx", 7},
            {{0x89, 0x93, 0xA4, 0x02, 0x00, 0x00, 0x44, 0x89, 0x8B, 0xA8, 0x02, 0x00, 0x00, 0x44}, "xxxxxxxxxxxxxx", 14},
            {{0x89, 0x01, 0x41, 0x80, 0xBD, 0xC8, 0x03, 0x00, 0x00, 0x00}, "xxxxxxxxxx", 10},
            {{0x89, 0x91, 0xA4, 0x02, 0x00, 0x00}, "xxxxxx", 6},
            {
                {
                    0x8B, 0x41, 0x68, 0x0F, 0xAF, 0xD0, 0x48, 0x8B, 0x41, 0x70, 0x8B, 0x08, 0x8B, 0x05, 0x00, 0x00, 0x00,
                    0x00, 0x0F, 0xAF, 0xC1, 0x3B, 0xD0, 0x89
                },
                "xxxxxxxxxxxxxx????xxxxxx", 24
            }
        };
        AmmoHookInfo& hook = g_Hooks[idx];

        const uintptr_t found = Memory::FindPattern("WindowsEntryPoint.Windows_W10.exe", patterns[idx].pattern,patterns[idx].mask);
        if (!found)
        {
            continue;
        }
        hook.hookAddr = found;

        switch (idx)
        {
        case 0: hook.origBytesSize = 5;
            break;
        case 1: hook.origBytesSize = 6;
            break;
        case 2: hook.origBytesSize = 10;
            break;
        case 3: hook.origBytesSize = 6;
            break;
        case 4: hook.origBytesSize = 6;
            break;
        default:
            break;
        }

        DWORD oldProt;
        VirtualProtect(reinterpret_cast<LPVOID>(hook.hookAddr), hook.origBytesSize, PAGE_EXECUTE_READWRITE, &oldProt);
        memcpy(hook.origBytes, reinterpret_cast<LPVOID>(hook.hookAddr), hook.origBytesSize);
        VirtualProtect(reinterpret_cast<LPVOID>(hook.hookAddr), hook.origBytesSize, oldProt, &oldProt);

        hook.shellcode = AllocateNear(hook.hookAddr, 0x1000);
        if (!hook.shellcode)
        {
            continue;
        }

        if (!GenerateShellcodeForHook(hook, idx))
        {
            VirtualFree(hook.shellcode, 0, MEM_RELEASE);
            hook.shellcode = nullptr;
            continue;
        }

        auto code = static_cast<BYTE*>(hook.shellcode);
        const size_t jmpPos = hook.shellcodeSize - 5;
        auto jmpOffset = static_cast<int32_t>((hook.hookAddr + hook.origBytesSize) - (reinterpret_cast<uintptr_t>(hook.shellcode) + hook.
            shellcodeSize));
        memcpy(&code[jmpPos + 1], &jmpOffset, 4);

        VirtualProtect(reinterpret_cast<LPVOID>(hook.hookAddr), 5, PAGE_EXECUTE_READWRITE, &oldProt);
        BYTE jmp[5] = {0xE9};
        auto rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(hook.shellcode) - (hook.hookAddr + 5));
        memcpy(&jmp[1], &rel, 4);
        memcpy(reinterpret_cast<LPVOID>(hook.hookAddr), jmp, 5);
        if (hook.origBytesSize > 5)
        {
            for (size_t j = 5; j < hook.origBytesSize; ++j)
            {
                *reinterpret_cast<BYTE*>(hook.hookAddr + j) = 0x90; // nop
            }
        }
        VirtualProtect(reinterpret_cast<LPVOID>(hook.hookAddr), 5, oldProt, &oldProt);

        hook.installed = true;
    }

    g_GlobalInstalled = true;
}

void InfiniteAmmo::Uninstall()
{
    if (!g_GlobalInstalled) return;

    for (auto& hook : g_Hooks)
    {
        if (!hook.installed) continue;

        DWORD oldProt;
        VirtualProtect(reinterpret_cast<LPVOID>(hook.hookAddr), hook.origBytesSize, PAGE_EXECUTE_READWRITE, &oldProt);
        memcpy(reinterpret_cast<LPVOID>(hook.hookAddr), hook.origBytes, hook.origBytesSize);
        VirtualProtect(reinterpret_cast<LPVOID>(hook.hookAddr), hook.origBytesSize, oldProt, &oldProt);

        if (hook.shellcode)
        {
            VirtualFree(hook.shellcode, 0, MEM_RELEASE);
            hook.shellcode = nullptr;
        }

        hook.installed = false;
    }

    g_GlobalInstalled = false;
}
