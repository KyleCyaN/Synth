#include "BotLobby.h"
#include "memory.h"
#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <chrono>

static uintptr_t g_ModuleBase = 0;
static bool g_Installed = false;
static uintptr_t g_HookAddr = 0;
static BYTE g_OrigBytes[7] = {0};
static LPVOID g_Shellcode = nullptr;
static uintptr_t g_PatternAddr = 0;

static bool IsMemoryReadable(uintptr_t address, size_t size = 1)
{
    MEMORY_BASIC_INFORMATION mbi = {0};
    if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) == 0)
    {
        return false;
    }

    return (mbi.State == MEM_COMMIT) &&
    ((mbi.Protect & PAGE_READONLY) ||
        (mbi.Protect & PAGE_READWRITE) ||
        (mbi.Protect & PAGE_EXECUTE_READ) ||
        (mbi.Protect & PAGE_EXECUTE_READWRITE));
}

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

bool BotRoom::IsMemoryReady()
{
    if (g_PatternAddr == 0)
    {
        g_ModuleBase = (uintptr_t)GetModuleHandleA("WindowsEntryPoint.Windows_W10.exe");
        if (!g_ModuleBase)
        {
            return false;
        }

        BYTE pattern[] = {0x45, 0x32, 0xFF, 0x84, 0xC0};
        const char* mask = "xxxxx";
        g_PatternAddr = Memory::FindPattern("WindowsEntryPoint.Windows_W10.exe", pattern, mask);

        if (!g_PatternAddr)
        {
            return false;
        }
    }

    if (!IsMemoryReadable(g_PatternAddr, 5))
    {
        return false;
    }

    BYTE expectedBytes[] = {0x45, 0x32, 0xFF, 0x84, 0xC0};
    BYTE actualBytes[5];

    DWORD oldProtect;
    if (!VirtualProtect((LPVOID)g_PatternAddr, 5, PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        return false;
    }

    memcpy(actualBytes, (LPVOID)g_PatternAddr, 5);
    VirtualProtect((LPVOID)g_PatternAddr, 5, oldProtect, &oldProtect);

    for (int i = 0; i < 5; i++)
    {
        if (actualBytes[i] != expectedBytes[i])
        {
            return false;
        }
    }

    return true;
}

void BotRoom::CheckAndInstall()
{
    if (g_Installed)
    {
        return;
    }

    if (!IsMemoryReady())
    {
        static int checkCount = 0;
        checkCount++;

        if (checkCount % 10 == 0)
        {
        }
        return;
    }

    Install();
}

bool BotRoom::GetStatus()
{
    return g_Installed;
}

void BotRoom::Install()
{
    if (g_Installed)
    {
        return;
    }

    if (g_PatternAddr == 0)
    {
        if (!IsMemoryReady())
        {
            return;
        }
    }

    g_HookAddr = g_PatternAddr;

    if (!IsMemoryReadable(g_HookAddr, 5))
    {
        return;
    }

    g_Shellcode = AllocateNear(g_HookAddr);
    if (!g_Shellcode)
    {
        return;
    }

    BYTE* code = (BYTE*)g_Shellcode;
    size_t i = 0;

    code[i++] = 0x45;
    code[i++] = 0x32;
    code[i++] = 0xFF; // xor r15b, r15b
    code[i++] = 0xB0;
    code[i++] = 0x05; // mov al, 5
    code[i++] = 0x84;
    code[i++] = 0xC0; // test al, al

    size_t jmpPos = i;
    code[i++] = 0xE9;
    size_t jmpOffsetPos = i;
    i += 4;

    uintptr_t returnAddr = g_HookAddr + 5;
    int32_t jmpOffset = (int32_t)(returnAddr - ((uintptr_t)(code + jmpPos) + 5));
    *(int32_t*)(code + jmpOffsetPos) = jmpOffset;

    DWORD oldProtect;

    if (!VirtualProtect((LPVOID)g_HookAddr, 5, PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        VirtualFree(g_Shellcode, 0, MEM_RELEASE);
        g_Shellcode = nullptr;
        return;
    }

    memcpy(g_OrigBytes, (LPVOID)g_HookAddr, 5);

    int64_t rel = (int64_t)((uintptr_t)g_Shellcode - (g_HookAddr + 5));
    int32_t r32 = (int32_t)rel;

    BYTE jmpInstr[5] = {0xE9};
    memcpy(&jmpInstr[1], &r32, 4);
    memcpy((LPVOID)g_HookAddr, jmpInstr, 5);

    VirtualProtect((LPVOID)g_HookAddr, 5, oldProtect, &oldProtect);

    g_Installed = true;
}

void BotRoom::Uninstall()
{
    if (!g_Installed) return;

    DWORD oldProtect;

    VirtualProtect((LPVOID)g_HookAddr, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy((LPVOID)g_HookAddr, g_OrigBytes, 5);
    VirtualProtect((LPVOID)g_HookAddr, 5, oldProtect, &oldProtect);

    if (g_Shellcode)
    {
        VirtualFree(g_Shellcode, 0, MEM_RELEASE);
        g_Shellcode = nullptr;
    }

    g_Installed = false;
    g_PatternAddr = 0;
}

void BotRoom::PeriodicCheck()
{
    static uint32_t lastCheckTime = 0;
    uint32_t currentTime = GetTickCount();

    if (currentTime - lastCheckTime < 100)
    {
        return;
    }

    lastCheckTime = currentTime;
    CheckAndInstall();
}
