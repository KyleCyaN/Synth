#include "FastRespawn.h"
#include "memory.h"
#include <Windows.h>
#include <cstdint>

bool FastRespawn::isEnabled = false;

static uintptr_t g_moduleBase = 0;
static uintptr_t g_hookAddr = 0;
static bool g_installed = false;

void FastRespawn::Install()
{
    if (g_installed) return;

    g_moduleBase = (uintptr_t)GetModuleHandleA("WindowsEntryPoint.Windows_W10.exe");
    if (!g_moduleBase)
    {
        return;
    }

    // AOB: 03 D0 66 0F 6E C2 F3 0F E6 C0 F2 0F 11 85 ?? ?? ?? ?? 48 8D 15 ?? ?? ?? ?? 48 8D 8D ?? ?? ?? ?? E8 ?? ?? ?? ?? 90 48 8D 8D
    BYTE pattern[] = {
        0x03, 0xD0, 0x66, 0x0F, 0x6E, 0xC2, 0xF3, 0x0F, 0xE6, 0xC0,
        0xF2, 0x0F, 0x11, 0x85, 0x00, 0x00, 0x00, 0x00,
        0x48, 0x8D, 0x15, 0x00, 0x00, 0x00, 0x00,
        0x48, 0x8D, 0x8D, 0x00, 0x00, 0x00, 0x00,
        0xE8, 0x00, 0x00, 0x00, 0x00,
        0x90,
        0x48, 0x8D, 0x8D
    };
    const char* mask = "xxxxxxxxxxxx????xxx????xxx????x????xxxx";
    g_hookAddr = Memory::FindPattern("WindowsEntryPoint.Windows_W10.exe", pattern, mask);
    if (!g_hookAddr)
    {
        BYTE pat2[] = {0x03, 0xD0, 0x66, 0x0F, 0x6E, 0xC2};
        g_hookAddr = Memory::FindPattern("WindowsEntryPoint.Windows_W10.exe", pat2, "xxxxxx");
        if (!g_hookAddr)
        {
            return;
        }
    }

    DWORD oldProt;
    VirtualProtect((LPVOID)g_hookAddr, 2, PAGE_EXECUTE_READWRITE, &oldProt);
    BYTE patch[2] = {0x31, 0xD2};
    memcpy((LPVOID)g_hookAddr, patch, 2);
    VirtualProtect((LPVOID)g_hookAddr, 2, oldProt, &oldProt);

    g_installed = true;
}

void FastRespawn::Uninstall()
{
    if (!g_installed) return;
    if (!g_hookAddr) return;

    DWORD oldProt;
    VirtualProtect((LPVOID)g_hookAddr, 2, PAGE_EXECUTE_READWRITE, &oldProt);
    BYTE orig[2] = {0x03, 0xD0};
    memcpy((LPVOID)g_hookAddr, orig, 2);
    VirtualProtect((LPVOID)g_hookAddr, 2, oldProt, &oldProt);

    g_installed = false;
}
