#include "Accuracy.h"
#include "memory.h"
#include <Windows.h>
#include <cstdint>

bool Accuracy::isEnabled = false;

static uintptr_t g_moduleBase = 0;
static uintptr_t g_hookAddr1 = 0;
static uintptr_t g_hookAddr2 = 0;
static uintptr_t g_hookAddr3 = 0;
static BYTE g_origBytes1[11] = {0};
static BYTE g_origBytes2[11] = {0};
static BYTE g_origBytes3[11] = {0};
static LPVOID g_shellcode1 = nullptr;
static LPVOID g_shellcode2 = nullptr;
static LPVOID g_shellcode3 = nullptr;
static bool g_installed = false;

static bool InstallSingleHook(uintptr_t& hookAddr, LPVOID& shellcode, BYTE* origBytes,
                              BYTE* pattern, const char* mask, size_t patLen, const char* name)
{
    hookAddr = Memory::FindPattern("WindowsEntryPoint.Windows_W10.exe", pattern, mask);
    if (!hookAddr)
    {
        return false;
    }

    shellcode = Memory::AllocateNear(hookAddr);
    if (!shellcode)
    {
        return false;
    }

    BYTE* code = (BYTE*)shellcode;
    size_t i = 0;
    uintptr_t retAddr = hookAddr + 11;

    // subss xmm0, xmm0
    code[i++] = 0xF3;
    code[i++] = 0x0F;
    code[i++] = 0x5C;
    code[i++] = 0xC0;
    // mov rcx, [rbx+0x3B0]
    code[i++] = 0x48;
    code[i++] = 0x8B;
    code[i++] = 0x8B;
    int32_t off3B0 = 0x3B0;
    memcpy(&code[i], &off3B0, 4);
    i += 4;
    // jmp rel32
    code[i++] = 0xE9;
    int32_t jmpRel = (int32_t)(retAddr - ((uintptr_t)(code + i) + 4));
    memcpy(&code[i], &jmpRel, 4);
    i += 4;

    memcpy(origBytes, (LPVOID)hookAddr, 11);

    // JMP (5B) + NOP (6B)
    DWORD oldProt;
    VirtualProtect((LPVOID)hookAddr, 11, PAGE_EXECUTE_READWRITE, &oldProt);
    BYTE jmp[5] = {0xE9};
    int32_t rel = (int32_t)((uintptr_t)shellcode - (hookAddr + 5));
    memcpy(&jmp[1], &rel, 4);
    memcpy((LPVOID)hookAddr, jmp, 5);
    memset((LPVOID)(hookAddr + 5), 0x90, 6);
    VirtualProtect((LPVOID)hookAddr, 11, oldProt, &oldProt);

    return true;
}

static void UninstallSingleHook(uintptr_t hookAddr, LPVOID shellcode, BYTE* origBytes)
{
    if (!hookAddr || !shellcode) return;
    DWORD oldProt;
    VirtualProtect((LPVOID)hookAddr, 11, PAGE_EXECUTE_READWRITE, &oldProt);
    memcpy((LPVOID)hookAddr, origBytes, 11);
    VirtualProtect((LPVOID)hookAddr, 11, oldProt, &oldProt);
    VirtualFree(shellcode, 0, MEM_RELEASE);
}

void Accuracy::Install()
{
    if (g_installed) return;

    g_moduleBase = (uintptr_t)GetModuleHandleA("WindowsEntryPoint.Windows_W10.exe");
    if (!g_moduleBase)
    {
        return;
    }

    // AOB1: F3 0F 5C C2 48 8B 8B B0 03 00 00
    BYTE pat1[] = {0xF3, 0x0F, 0x5C, 0xC2, 0x48, 0x8B, 0x8B, 0xB0, 0x03, 0x00, 0x00};
    bool ok1 = InstallSingleHook(g_hookAddr1, g_shellcode1, g_origBytes1, pat1, "xxxxxxxxxxx", 11, "Hook1");

    // AOB2: F3 0F 58 C2 48 8B 8B B0 03 00 00
    BYTE pat2[] = {0xF3, 0x0F, 0x58, 0xC2, 0x48, 0x8B, 0x8B, 0xB0, 0x03, 0x00, 0x00};
    bool ok2 = InstallSingleHook(g_hookAddr2, g_shellcode2, g_origBytes2, pat2, "xxxxxxxxxxx", 11, "Hook2");

    bool ok3 = InstallSingleHook(g_hookAddr3, g_shellcode3, g_origBytes3, pat2, "xxxxxxxxxxx", 11, "Hook3");

    if (!ok1 && !ok2 && !ok3)
    {
        return;
    }

    g_installed = true;
}

void Accuracy::Uninstall()
{
    if (!g_installed) return;

    UninstallSingleHook(g_hookAddr1, g_shellcode1, g_origBytes1);
    g_hookAddr1 = 0;
    g_shellcode1 = nullptr;

    UninstallSingleHook(g_hookAddr2, g_shellcode2, g_origBytes2);
    g_hookAddr2 = 0;
    g_shellcode2 = nullptr;

    UninstallSingleHook(g_hookAddr3, g_shellcode3, g_origBytes3);
    g_hookAddr3 = 0;
    g_shellcode3 = nullptr;

    g_installed = false;
}
