#include "Recoilless.h"
#include "memory.h"
#include <Windows.h>
#include <cstdint>

bool Recoilless::isEnabled = false;

static uintptr_t g_hookAddr = 0;
static BYTE g_origBytes[6] = {0};
static bool g_installed = false;

void Recoilless::Install()
{
    if (g_installed)
        return;

    if (HMODULE hMod = GetModuleHandleA("WindowsEntryPoint.Windows_W10.exe"); !hMod)
    {
        return;
    }

    // AOB: 89 83 88 05 00 00 F3
    BYTE pattern[] = {0x89, 0x83, 0x88, 0x05, 0x00, 0x00, 0xF3};
    g_hookAddr = Memory::FindPattern(
        "WindowsEntryPoint.Windows_W10.exe", pattern, "xxxxxxx");

    if (!g_hookAddr)
    {
        return;
    }

    memcpy(g_origBytes, (void*)g_hookAddr, 6);

    DWORD oldProtect;
    VirtualProtect((void*)g_hookAddr, 6,PAGE_EXECUTE_READWRITE, &oldProtect);

    BYTE patch[6] = {
        0x90,
        0x90,
        0x90,
        0x90,
        0x90,
        0x90
    };

    memcpy((void*)g_hookAddr, patch, 6);

    VirtualProtect((void*)g_hookAddr, 6,
                   oldProtect, &oldProtect);

    g_installed = true;
}

void Recoilless::Uninstall()
{
    if (!g_installed || !g_hookAddr)
        return;

    DWORD oldProtect;
    VirtualProtect((void*)g_hookAddr, 6,PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy((void*)g_hookAddr, g_origBytes, 6);
    VirtualProtect((void*)g_hookAddr, 6, oldProtect, &oldProtect);

    g_installed = false;
}
