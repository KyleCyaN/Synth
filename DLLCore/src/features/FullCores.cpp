#include "FullCores.h"
#include <Windows.h>
#include <cstdint>

#include "memory.h"

static bool g_Installed = false;
static uintptr_t g_HookAddr = 0;
static BYTE g_OrigBytes[6] = {0};
static LPVOID g_Shellcode = nullptr;
static uintptr_t g_ModuleBase = 0;

void FullCores::Install()
{
    if (g_Installed) return;

    g_ModuleBase = (uintptr_t)GetModuleHandleA("WindowsEntryPoint.Windows_W10.exe");
    if (!g_ModuleBase)
    {
        return;
    }

    g_HookAddr = g_ModuleBase + 0x96705E;

    BYTE expectedBytes[] = {0x8B, 0x93, 0xCC, 0x00, 0x00, 0x00};
    if (memcmp((LPVOID)g_HookAddr, expectedBytes, 6) != 0)
    {
        return;
    }

    g_Shellcode = Memory::AllocateNear(g_HookAddr);
    if (!g_Shellcode)
    {
        return;
    }

    BYTE* code = (BYTE*)g_Shellcode;
    size_t i = 0;

    const size_t DATA_OFFSET = 0x80;

    // cmp byte ptr [rip+DATA_OFFSET+8], 0
    code[i++] = 0x80;
    code[i++] = 0x3D;
    int32_t modeOffset = (int32_t)((uintptr_t)(code + DATA_OFFSET + 8) - (uintptr_t)(code + i + 4));
    memcpy(&code[i], &modeOffset, 4);
    i += 4;
    code[i++] = 0x00;

    // je active_mode
    code[i++] = 0x74; // je rel8
    uint8_t jePos = (uint8_t)i;
    i += 1;

    // mov eax, [rip+DATA_OFFSET+4]
    code[i++] = 0x8B;
    code[i++] = 0x05;
    int32_t origValReadOff = (int32_t)((uintptr_t)(code + DATA_OFFSET + 4) - (uintptr_t)(code + i + 4));
    memcpy(&code[i], &origValReadOff, 4);
    i += 4;

    // mov [rbx+0xCC], eax
    code[i++] = 0x89;
    code[i++] = 0x83;
    DWORD offCC = 0xCC;
    memcpy(&code[i], &offCC, 4);
    i += 4;

    // jmp to code
    code[i++] = 0xEB; // jmp rel8
    uint8_t jmpToCode1 = (uint8_t)i;
    i += 1;

    code[jePos] = (uint8_t)(i - (jePos + 1));

    // cmp byte ptr [rip+DATA_OFFSET], 1
    code[i++] = 0x80;
    code[i++] = 0x3D;
    int32_t flagOff = (int32_t)((uintptr_t)(code + DATA_OFFSET) - (uintptr_t)(code + i + 4));
    memcpy(&code[i], &flagOff, 4);
    i += 4;
    code[i++] = 0x01;

    // jne skip_save
    code[i++] = 0x75; // jne rel8
    uint8_t jneSkipSave = (uint8_t)i;
    i += 1;

    // mov eax, [rbx+0xCC]
    code[i++] = 0x8B;
    code[i++] = 0x83;
    memcpy(&code[i], &offCC, 4);
    i += 4;

    // mov [rip+DATA_OFFSET+4], eax
    code[i++] = 0x89;
    code[i++] = 0x05;
    int32_t saveOff = (int32_t)((uintptr_t)(code + DATA_OFFSET + 4) - (uintptr_t)(code + i + 4));
    memcpy(&code[i], &saveOff, 4);
    i += 4;

    // mov byte ptr [rip+DATA_OFFSET], 0
    code[i++] = 0xC6;
    code[i++] = 0x05;
    int32_t clearFlagOff = (int32_t)((uintptr_t)(code + DATA_OFFSET) - (uintptr_t)(code + i + 4));
    memcpy(&code[i], &clearFlagOff, 4);
    i += 4;
    code[i++] = 0x00;

    // jne skip_save
    code[jneSkipSave] = (uint8_t)(i - (jneSkipSave + 1));

    // mov dword ptr [rbx+0xCC], 5
    code[i++] = 0xC7;
    code[i++] = 0x83;
    memcpy(&code[i], &offCC, 4);
    i += 4;
    DWORD val5 = 5;
    memcpy(&code[i], &val5, 4);
    i += 4;

    code[jmpToCode1] = (uint8_t)(i - (jmpToCode1 + 1));

    // mov edx, [rbx+0xCC]
    code[i++] = 0x8B;
    code[i++] = 0x93;
    memcpy(&code[i], &offCC, 4);
    i += 4;

    // jmp back
    code[i++] = 0xFF;
    code[i++] = 0x25;
    DWORD zero = 0;
    memcpy(&code[i], &zero, 4);
    i += 4;
    uintptr_t retAddr = g_HookAddr + 6;
    memcpy(&code[i], &retAddr, 8);
    i += 8;

    if (i > DATA_OFFSET)
    {
        VirtualFree(g_Shellcode, 0, MEM_RELEASE);
        g_Shellcode = nullptr;
        return;
    }

    code[DATA_OFFSET] = 1;
    *(DWORD*)(code + DATA_OFFSET + 4) = 0;
    code[DATA_OFFSET + 8] = 0;

    DWORD oldProt;
    VirtualProtect((LPVOID)g_HookAddr, 6, PAGE_EXECUTE_READWRITE, &oldProt);
    memcpy(g_OrigBytes, (LPVOID)g_HookAddr, 6);

    BYTE jmp[6] = {0xE9, 0x00, 0x00, 0x00, 0x00, 0x90};
    int32_t r32 = (int32_t)((uintptr_t)g_Shellcode - (g_HookAddr + 5));
    memcpy(&jmp[1], &r32, 4);
    memcpy((LPVOID)g_HookAddr, jmp, 6);

    VirtualProtect((LPVOID)g_HookAddr, 6, oldProt, &oldProt);

    g_Installed = true;
}

void FullCores::Uninstall()
{
    if (!g_Installed) return;

    if (g_Shellcode)
    {
        BYTE* shell = (BYTE*)g_Shellcode;
        shell[0x80 + 8] = 1;

        Sleep(50);

        DWORD oldProt;
        VirtualProtect((LPVOID)g_HookAddr, 6, PAGE_EXECUTE_READWRITE, &oldProt);
        memcpy((LPVOID)g_HookAddr, g_OrigBytes, 6);
        VirtualProtect((LPVOID)g_HookAddr, 6, oldProt, &oldProt);

        Sleep(10);

        VirtualFree(g_Shellcode, 0, MEM_RELEASE);
        g_Shellcode = nullptr;
    }

    g_Installed = false;
}
