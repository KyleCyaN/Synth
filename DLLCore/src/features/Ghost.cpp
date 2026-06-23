#include "Ghost.h"
#include "memory.h"
#include <Windows.h>
#include <cstdint>

bool Ghost::isEnabled = false;

static uintptr_t g_moduleBase = 0;
static uintptr_t g_hookAddr = 0;
static BYTE g_origByte = 0;
static bool g_installed = false;

void Ghost::Install()
{
    if (g_installed) return;

    g_moduleBase = reinterpret_cast<uintptr_t>(GetModuleHandleA("WindowsEntryPoint.Windows_W10.exe"));
    if (!g_moduleBase)
    {
        return;
    }

    g_hookAddr = g_moduleBase + 0x455700;
    g_origByte = *reinterpret_cast<BYTE*>(g_hookAddr);

    DWORD oldProt;
    VirtualProtect(reinterpret_cast<LPVOID>(g_hookAddr), 1, PAGE_EXECUTE_READWRITE, &oldProt);
    *reinterpret_cast<BYTE*>(g_hookAddr) = 0xC3;
    VirtualProtect(reinterpret_cast<LPVOID>(g_hookAddr), 1, oldProt, &oldProt);

    g_installed = true;
}

void Ghost::Uninstall()
{
    if (!g_installed) return;
    if (!g_hookAddr) return;

    DWORD oldProt;
    VirtualProtect(reinterpret_cast<LPVOID>(g_hookAddr), 1, PAGE_EXECUTE_READWRITE, &oldProt);
    *reinterpret_cast<BYTE*>(g_hookAddr) = g_origByte;
    VirtualProtect(reinterpret_cast<LPVOID>(g_hookAddr), 1, oldProt, &oldProt);

    g_installed = false;
}
