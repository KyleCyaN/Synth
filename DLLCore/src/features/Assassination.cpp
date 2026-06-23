#include "Assassination.h"
#include "memory.h"
#include <Windows.h>
#include <cstdint>

bool Assassination::isEnabled = false;

static uintptr_t g_moduleBase = 0;
static uintptr_t g_hookAddr = 0;
static BYTE g_origBytes[8] = {0};
static LPVOID g_shellcode = nullptr;
static bool g_installed = false;

static LPVOID AllocateNear(uintptr_t target)
{
    for (int i = 0; i < 524288; ++i)
    {
        int dir = (i % 2 == 0) ? 1 : -1;
        uintptr_t addr = target + dir * (i / 2) * 0x1000;
        addr &= ~((uintptr_t)0xFFF);
        LPVOID p = VirtualAlloc((LPVOID)addr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (p)
        {
            int64_t rel = (int64_t)((uintptr_t)p - (target + 5));
            if (rel >= INT32_MIN && rel <= INT32_MAX)
                return p;
            VirtualFree(p, 0, MEM_RELEASE);
        }
    }
    return nullptr;
}

void Assassination::Install()
{
    if (g_installed) return;

    g_moduleBase = (uintptr_t)GetModuleHandleA("WindowsEntryPoint.Windows_W10.exe");
    if (!g_moduleBase)
    {
        return;
    }

    const BYTE pattern[] = {
        0xFF, 0x48, 0x8B, 0x81, 0xA8, 0x01, 0x00, 0x00,
        0x48, 0x85, 0xC0, 0x74, 0x1B, 0xF3, 0x0F, 0x10, 0x40, 0x28
    };
    const char* mask = "xxxxxxxxxxxxxxxxxx";

    uintptr_t em = Memory::FindPattern("WindowsEntryPoint.Windows_W10.exe", pattern, mask);
    if (!em)
    {
        return;
    }

    g_hookAddr = em + 0x0D;

    char buf[128];

    g_shellcode = AllocateNear(g_hookAddr);
    if (!g_shellcode)
    {
        return;
    }

    BYTE* code = (BYTE*)g_shellcode;
    size_t i = 0;

    code[i++] = 0x81;
    code[i++] = 0xB8;

    *(uint32_t*)&code[i] = 0x00000434;
    i += 4;
    *(uint32_t*)&code[i] = 0x00000000;
    i += 4;

    code[i++] = 0x75;
    size_t jneOffsetPos = i++;

    code[i++] = 0xC7;
    code[i++] = 0x40;
    code[i++] = 0x28;
    float f184 = 184.0f;

    memcpy(&code[i], &f184, 4);
    i += 4;

    code[i++] = 0xEB;
    size_t jmpContinuePos = i++;

    size_t label_zero = i;

    code[i++] = 0xC7;
    code[i++] = 0x40;
    code[i++] = 0x28;
    *(uint32_t*)&code[i] = 0x00000000;
    i += 4;

    size_t label_continue = i;

    code[i++] = 0xF3;
    code[i++] = 0x0F;
    code[i++] = 0x10;
    code[i++] = 0x40;
    code[i++] = 0x28;

    code[i++] = 0xE9;

    uintptr_t retAddr = g_hookAddr + 5;
    int32_t relBack = (int32_t)(retAddr - ((uintptr_t)(code + i) + 4));
    memcpy(&code[i], &relBack, 4);
    i += 4;

    code[jneOffsetPos] = (BYTE)(label_zero - (jneOffsetPos + 1));
    code[jmpContinuePos] = (BYTE)(label_continue - (jmpContinuePos + 1));

    memcpy(g_origBytes, (LPVOID)g_hookAddr, 5);

    DWORD oldProt;
    VirtualProtect((LPVOID)g_hookAddr, 5, PAGE_EXECUTE_READWRITE, &oldProt);

    BYTE jmp[5] = {0xE9};
    int32_t rel = (int32_t)((uintptr_t)g_shellcode - (g_hookAddr + 5));
    memcpy(&jmp[1], &rel, 4);

    memcpy((LPVOID)g_hookAddr, jmp, 5);

    VirtualProtect((LPVOID)g_hookAddr, 5, oldProt, &oldProt);

    g_installed = true;
}


void Assassination::Uninstall()
{
    if (!g_installed) return;
    if (!g_hookAddr) return;

    DWORD oldProt;
    VirtualProtect(reinterpret_cast<LPVOID>(g_hookAddr), 8, PAGE_EXECUTE_READWRITE, &oldProt);
    memcpy(reinterpret_cast<LPVOID>(g_hookAddr), g_origBytes, 8);
    VirtualProtect(reinterpret_cast<LPVOID>(g_hookAddr), 8, oldProt, &oldProt);
    if (g_shellcode)
    {
        VirtualFree(g_shellcode, 0, MEM_RELEASE);
        g_shellcode = nullptr;
    }

    g_installed = false;
}
