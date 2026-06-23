#include "BattlePass.h"
#include "memory.h"
#include <Windows.h>
#include <cstdint>

bool BattlePass::isEnabled = false;

static uintptr_t g_moduleBase = 0;
static uintptr_t g_hookAddr = 0;
static BYTE g_origBytes[9] = {0};
static bool g_installed = false;

void BattlePass::Install()
{
    if (g_installed) return;

    g_moduleBase = reinterpret_cast<uintptr_t>(GetModuleHandleA("WindowsEntryPoint.Windows_W10.exe"));
    if (!g_moduleBase)
    {
        return;
    }

    // AOB: 41 8B 40 34 3D 90 01 31 21
    BYTE pattern[] = {0x41, 0x8B, 0x40, 0x34, 0x3D, 0x90, 0x01, 0x31, 0x21};
    const char* mask = "xxxxxxxxx";
    g_hookAddr = Memory::FindPattern("WindowsEntryPoint.Windows_W10.exe", pattern, mask);
    if (!g_hookAddr)
    {
        return;
    }

    memcpy(g_origBytes, reinterpret_cast<LPVOID>(g_hookAddr), 9);

    // AOB: B8 01 00 00 00 41 89 40 34
    DWORD oldProt;
    VirtualProtect(reinterpret_cast<LPVOID>(g_hookAddr), 9, PAGE_EXECUTE_READWRITE, &oldProt);
    BYTE patch[9] = {0xB8, 0x01, 0x00, 0x00, 0x00, 0x41, 0x89, 0x40, 0x34};
    memcpy(reinterpret_cast<LPVOID>(g_hookAddr), patch, 9);
    VirtualProtect(reinterpret_cast<LPVOID>(g_hookAddr), 9, oldProt, &oldProt);

    g_installed = true;
}

void BattlePass::Uninstall()
{
    if (!g_installed) return;
    if (!g_hookAddr) return;

    DWORD oldProt;
    VirtualProtect(reinterpret_cast<LPVOID>(g_hookAddr), 9, PAGE_EXECUTE_READWRITE, &oldProt);
    memcpy(reinterpret_cast<LPVOID>(g_hookAddr), g_origBytes, 9);
    VirtualProtect(reinterpret_cast<LPVOID>(g_hookAddr), 9, oldProt, &oldProt);

    g_installed = false;
}
