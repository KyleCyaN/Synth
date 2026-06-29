#include "BotLobby.h"
#include "memory.h"
#include <Windows.h>
#include <cstdint>

static uintptr_t g_ModuleBase  = 0;
static bool      g_Installed   = false;
static uintptr_t g_HookAddr    = 0;
static BYTE      g_OrigBytes[5] = {0};
static LPVOID    g_Shellcode   = nullptr;
static uintptr_t g_PatternAddr = 0;

void BotRoom::Install()
{
    if (g_Installed) return;

    if (!g_ModuleBase)
    {
        g_ModuleBase = reinterpret_cast<uintptr_t>(
            GetModuleHandleA("WindowsEntryPoint.Windows_W10.exe"));
        if (!g_ModuleBase) return;
    }

    if (!g_PatternAddr)
    {
        const BYTE pattern[] = {0x45, 0x32, 0xFF, 0x84, 0xC0};
        const char* mask = "xxxxx";
        g_PatternAddr = Memory::FindPattern(
            "WindowsEntryPoint.Windows_W10.exe", pattern, mask);
        if (!g_PatternAddr) return;

        MEMORY_BASIC_INFORMATION mbi = {};
        if (!VirtualQuery(reinterpret_cast<LPCVOID>(g_PatternAddr),
                          &mbi, sizeof(mbi)))
            return;

        constexpr DWORD readable =
            PAGE_READONLY | PAGE_READWRITE |
            PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE;
        if (!(mbi.State == MEM_COMMIT && (mbi.Protect & readable)))
            return;

        BYTE actual[5];
        DWORD oldProt;
        if (!VirtualProtect(reinterpret_cast<LPVOID>(g_PatternAddr),
                            5, PAGE_EXECUTE_READWRITE, &oldProt))
            return;
        memcpy(actual, reinterpret_cast<LPCVOID>(g_PatternAddr), 5);
        VirtualProtect(reinterpret_cast<LPVOID>(g_PatternAddr),
                       5, oldProt, &oldProt);

        for (int i = 0; i < 5; ++i)
            if (actual[i] != pattern[i]) return;
    }

    g_HookAddr = g_PatternAddr;

    {
        MEMORY_BASIC_INFORMATION mbi = {};
        if (!VirtualQuery(reinterpret_cast<LPCVOID>(g_HookAddr),
                          &mbi, sizeof(mbi)))
            return;
        constexpr DWORD readable =
            PAGE_READONLY | PAGE_READWRITE |
            PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE;
        if (!(mbi.State == MEM_COMMIT && (mbi.Protect & readable)))
            return;
    }

    g_Shellcode = Memory::AllocateNear(g_HookAddr);
    if (!g_Shellcode) return;

    auto code = static_cast<BYTE*>(g_Shellcode);
    size_t i = 0;
    code[i++] = 0x45;   // xor r15b, r15b
    code[i++] = 0x32;
    code[i++] = 0xFF;
    code[i++] = 0xB0;   // mov al, 5
    code[i++] = 0x05;
    code[i++] = 0x84;   // test al, al
    code[i++] = 0xC0;

    const size_t jmpPos = i;
    code[i++] = 0xE9;   // jmp rel32

    const size_t jmpOffsetPos = i;
    i += 4;

    const uintptr_t returnAddr = g_HookAddr + 5;
    const int32_t jmpOffset = static_cast<int32_t>(
        returnAddr - (reinterpret_cast<uintptr_t>(code + jmpPos) + 5));
    *reinterpret_cast<int32_t*>(code + jmpOffsetPos) = jmpOffset;

    DWORD oldProtect;
    if (!VirtualProtect(reinterpret_cast<LPVOID>(g_HookAddr), 5,
                        PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        VirtualFree(g_Shellcode, 0, MEM_RELEASE);
        g_Shellcode = nullptr;
        return;
    }

    memcpy(g_OrigBytes, reinterpret_cast<LPCVOID>(g_HookAddr), 5);

    const int64_t rel = static_cast<int64_t>(
        reinterpret_cast<uintptr_t>(g_Shellcode) - (g_HookAddr + 5));
    const int32_t r32 = static_cast<int32_t>(rel);
    BYTE jmpInstr[5] = {0xE9};
    memcpy(&jmpInstr[1], &r32, 4);
    memcpy(reinterpret_cast<LPVOID>(g_HookAddr), jmpInstr, 5);

    VirtualProtect(reinterpret_cast<LPVOID>(g_HookAddr), 5, oldProtect, &oldProtect);

    g_Installed = true;
}

void BotRoom::Uninstall()
{
    if (!g_Installed) return;

    DWORD oldProtect;
    VirtualProtect(reinterpret_cast<LPVOID>(g_HookAddr), 5,
                   PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy(reinterpret_cast<LPVOID>(g_HookAddr), g_OrigBytes, 5);
    VirtualProtect(reinterpret_cast<LPVOID>(g_HookAddr), 5, oldProtect, &oldProtect);

    if (g_Shellcode)
    {
        VirtualFree(g_Shellcode, 0, MEM_RELEASE);
        g_Shellcode = nullptr;
    }

    g_Installed = false;
    g_PatternAddr = 0;
}