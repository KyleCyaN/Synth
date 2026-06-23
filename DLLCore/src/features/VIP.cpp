#include "VIP.h"
#include "memory.h"
#include <Windows.h>
#include <cstdint>

bool VIP::isEnabled = false;

static uintptr_t g_moduleBase = 0;
static uintptr_t g_hookAddr = 0;
static BYTE g_origBytes[11] = {0};
static LPVOID g_shellcode = nullptr;
static bool g_installed = false;

static LPVOID AllocateNear(uintptr_t target)
{
    for (int i = 0; i < 524288; ++i)
    {
        int dir = (i % 2 == 0) ? 1 : -1;
        uintptr_t addr = target + dir * (i / 2) * 0x1000;
        addr &= ~static_cast<uintptr_t>(0xFFF);
        LPVOID p = VirtualAlloc(reinterpret_cast<LPVOID>(addr), 0x1000, MEM_COMMIT | MEM_RESERVE,
                                PAGE_EXECUTE_READWRITE);
        if (p)
        {
            int64_t rel = static_cast<int64_t>(reinterpret_cast<uintptr_t>(p) - (target + 5));
            if (rel >= INT32_MIN && rel <= INT32_MAX)
                return p;
            VirtualFree(p, 0, MEM_RELEASE);
        }
    }
    return nullptr;
}

void VIP::Install()
{
    if (g_installed) return;

    g_moduleBase = reinterpret_cast<uintptr_t>(GetModuleHandleA("WindowsEntryPoint.Windows_W10.exe"));
    if (!g_moduleBase)
    {
        return;
    }

    // AOB: 44 01 37 0F B6 84 24 B0 00 00 00
    BYTE pattern[] = {0x44, 0x01, 0x37, 0x0F, 0xB6, 0x84, 0x24, 0xB0, 0x00, 0x00, 0x00};
    const char* mask = "xxxxxxxxxxx";
    g_hookAddr = Memory::FindPattern("WindowsEntryPoint.Windows_W10.exe", pattern, mask);
    if (!g_hookAddr)
    {
        return;
    }

    g_shellcode = AllocateNear(g_hookAddr);
    if (!g_shellcode)
    {
        return;
    }

    auto code = static_cast<BYTE*>(g_shellcode);
    size_t i = 0;
    uintptr_t retAddr = g_hookAddr + 11;

    // inc rdi
    code[i++] = 0x48;
    code[i++] = 0xFF;
    code[i++] = 0xC7;
    // mov [rdi], rdi
    code[i++] = 0x48;
    code[i++] = 0x89;
    code[i++] = 0x3F;
    // add [rdi], r14d
    code[i++] = 0x44;
    code[i++] = 0x01;
    code[i++] = 0x37;
    // movzx eax, byte ptr [rsp+000000B0]
    code[i++] = 0x0F;
    code[i++] = 0xB6;
    code[i++] = 0x84;
    code[i++] = 0x24;
    code[i++] = 0xB0;
    code[i++] = 0x00;
    code[i++] = 0x00;
    code[i++] = 0x00;
    // jmp rel32
    code[i++] = 0xE9;
    const int32_t jmpRel = static_cast<int32_t>(retAddr - (reinterpret_cast<uintptr_t>(code + i) + 4));
    memcpy(&code[i], &jmpRel, 4);
    i += 4;

    memcpy(g_origBytes, reinterpret_cast<LPVOID>(g_hookAddr), 11);

    // JMP (5B) + NOP (6B)
    DWORD oldProt;
    VirtualProtect(reinterpret_cast<LPVOID>(g_hookAddr), 11, PAGE_EXECUTE_READWRITE, &oldProt);
    BYTE jmp[5] = {0xE9};
    const int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(g_shellcode) - (g_hookAddr + 5));
    memcpy(&jmp[1], &rel, 4);
    memcpy(reinterpret_cast<LPVOID>(g_hookAddr), jmp, 5);
    memset(reinterpret_cast<LPVOID>(g_hookAddr + 5), 0x90, 6);
    VirtualProtect(reinterpret_cast<LPVOID>(g_hookAddr), 11, oldProt, &oldProt);

    g_installed = true;
}

void VIP::Uninstall()
{
    if (!g_installed) return;
    if (!g_hookAddr) return;

    DWORD oldProt;
    VirtualProtect(reinterpret_cast<LPVOID>(g_hookAddr), 11, PAGE_EXECUTE_READWRITE, &oldProt);
    memcpy(reinterpret_cast<LPVOID>(g_hookAddr), g_origBytes, 11);
    VirtualProtect(reinterpret_cast<LPVOID>(g_hookAddr), 11, oldProt, &oldProt);
    if (g_shellcode)
    {
        VirtualFree(g_shellcode, 0, MEM_RELEASE);
        g_shellcode = nullptr;
    }

    g_installed = false;
}
