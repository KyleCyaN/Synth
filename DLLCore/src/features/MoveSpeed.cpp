#include "MoveSpeed.h"
#include "memory.h"
#include <Windows.h>
#include <cstdint>

bool MoveSpeed::isEnabled = false;
float MoveSpeed::speed = 6.0f;

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

void MoveSpeed::Install()
{
    if (g_installed) return;

    g_moduleBase = (uintptr_t)GetModuleHandleA("WindowsEntryPoint.Windows_W10.exe");
    if (!g_moduleBase)
    {
        return;
    }

    // AOB: F3 0F 11 87 9C 07 00 00
    BYTE pattern[] = {0xF3, 0x0F, 0x11, 0x87, 0x9C, 0x07, 0x00, 0x00};
    const char* mask = "xxxxxxxx";
    g_hookAddr = Memory::FindPattern("WindowsEntryPoint.Windows_W10.exe", pattern, mask);
    if (!g_hookAddr)
    {
        return;
    }

    g_shellcode = AllocateNear(g_hookAddr);
    if (!g_shellcode)
    {
        return;
    }

    BYTE* code = (BYTE*)g_shellcode;
    size_t i = 0;
    uintptr_t retAddr = g_hookAddr + 8;
    uintptr_t speedAddr = (uintptr_t)&MoveSpeed::speed;

    // mov [rdi+0x79C], (float)speed
    code[i++] = 0xC7;
    code[i++] = 0x87;
    int32_t off79C = 0x79C;
    memcpy(&code[i], &off79C, 4);
    i += 4;
    // mov rax, speedAddr
    code[i++] = 0x48;
    code[i++] = 0xB8;
    memcpy(&code[i], &speedAddr, 8);
    i += 8;
    // movss xmm0, [rax]
    code[i++] = 0xF3;
    code[i++] = 0x0F;
    code[i++] = 0x10;
    code[i++] = 0x00;

    i = 0;

    // mov rax, speedAddr
    code[i++] = 0x48;
    code[i++] = 0xB8;
    memcpy(&code[i], &speedAddr, 8);
    i += 8;
    // movss xmm0, [rax]
    code[i++] = 0xF3;
    code[i++] = 0x0F;
    code[i++] = 0x10;
    code[i++] = 0x00;
    // movss [rdi+0x79C], xmm0
    code[i++] = 0xF3;
    code[i++] = 0x0F;
    code[i++] = 0x11;
    code[i++] = 0x87;
    code[i++] = 0x9C;
    code[i++] = 0x07;
    code[i++] = 0x00;
    code[i++] = 0x00;

    i = 0;
    // mov rax, speedAddr
    code[i++] = 0x48;
    code[i++] = 0xB8;
    memcpy(&code[i], &speedAddr, 8);
    i += 8;
    // movss xmm0, [rax]
    code[i++] = 0xF3;
    code[i++] = 0x0F;
    code[i++] = 0x10;
    code[i++] = 0x00;
    // movss [rdi+0x79C], xmm0
    code[i++] = 0xF3;
    code[i++] = 0x0F;
    code[i++] = 0x11;
    code[i++] = 0x87;
    code[i++] = 0x9C;
    code[i++] = 0x07;
    code[i++] = 0x00;
    code[i++] = 0x00;
    // jmp rel32
    code[i++] = 0xE9;
    int32_t jmpRel = (int32_t)(retAddr - ((uintptr_t)(code + i) + 4));
    memcpy(&code[i], &jmpRel, 4);
    i += 4;

    memcpy(g_origBytes, (LPVOID)g_hookAddr, 8);

    // JMP (5B) + NOP (3B)
    DWORD oldProt;
    VirtualProtect((LPVOID)g_hookAddr, 8, PAGE_EXECUTE_READWRITE, &oldProt);
    BYTE jmp[5] = {0xE9};
    int32_t rel = (int32_t)((uintptr_t)g_shellcode - (g_hookAddr + 5));
    memcpy(&jmp[1], &rel, 4);
    memcpy((LPVOID)g_hookAddr, jmp, 5);
    memset((LPVOID)(g_hookAddr + 5), 0x90, 3);
    VirtualProtect((LPVOID)g_hookAddr, 8, oldProt, &oldProt);

    g_installed = true;
}

void MoveSpeed::Uninstall()
{
    if (!g_installed) return;
    if (!g_hookAddr) return;

    DWORD oldProt;
    VirtualProtect((LPVOID)g_hookAddr, 8, PAGE_EXECUTE_READWRITE, &oldProt);
    memcpy((LPVOID)g_hookAddr, g_origBytes, 8);
    VirtualProtect((LPVOID)g_hookAddr, 8, oldProt, &oldProt);
    if (g_shellcode)
    {
        VirtualFree(g_shellcode, 0, MEM_RELEASE);
        g_shellcode = nullptr;
    }

    g_installed = false;
}
