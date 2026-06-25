#include "Ghost.h"
#include "memory.h"
#include <Windows.h>
#include <cstdint>

bool Ghost::isEnabled = false;

static uintptr_t g_moduleBase = 0;
static uintptr_t g_hookAddr = 0;
static bool g_installed = false;

static const BYTE g_pattern[] = {
    0x48, 0x89, 0x5C, 0x24, 0x00,
    0x89, 0x54, 0x24, 0x00,
    0x55,
    0x48, 0x8B, 0xEC
};

static const char* g_mask = "xxxx?xxx?xxxx";

static const BYTE g_originalBytes[] = {
    0x48, 0x89, 0x5C, 0x24, 0x00,
    0x89, 0x54, 0x24, 0x00,
    0x55, 0x48, 0x8B, 0xEC
};

// ret C3
static constexpr BYTE g_patchByte = 0xC3;

void Ghost::Install()
{
    if (g_installed)
        return;

    g_moduleBase = reinterpret_cast<uintptr_t>(GetModuleHandleA("WindowsEntryPoint.Windows_W10.exe"));
    if (!g_moduleBase)
        return;

    g_hookAddr = Memory::FindPattern(
        "WindowsEntryPoint.Windows_W10.exe",
        g_pattern,
        g_mask
    );

    if (!g_hookAddr)
        return;

    DWORD oldProt = 0;
    VirtualProtect(
        reinterpret_cast<LPVOID>(g_hookAddr),
        1,
        PAGE_EXECUTE_READWRITE,
        &oldProt
    );

    memcpy(reinterpret_cast<LPVOID>(g_hookAddr), &g_patchByte, 1);

    VirtualProtect(
        reinterpret_cast<LPVOID>(g_hookAddr),
        1,
        oldProt,
        &oldProt
    );

    g_installed = true;
}

void Ghost::Uninstall()
{
    if (!g_installed)
        return;
    if (!g_hookAddr)
        return;

    DWORD oldProt = 0;
    VirtualProtect(
        reinterpret_cast<LPVOID>(g_hookAddr),
        sizeof(g_originalBytes),
        PAGE_EXECUTE_READWRITE,
        &oldProt
    );

    memcpy(
        reinterpret_cast<LPVOID>(g_hookAddr),
        g_originalBytes,
        sizeof(g_originalBytes)
    );

    VirtualProtect(
        reinterpret_cast<LPVOID>(g_hookAddr),
        sizeof(g_originalBytes),
        oldProt,
        &oldProt
    );

    g_installed = false;
}