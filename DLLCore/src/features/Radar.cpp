#include "Radar.h"
#include "memory.h"
#include <Windows.h>
#include <cstdint>

bool Radar::isEnabled = false;

static uintptr_t g_moduleBase = 0;
static uintptr_t g_hookAddr = 0;
static BYTE     g_origBytes[6] = {0};
static LPVOID   g_shellcode = nullptr;
static bool     g_installed = false;

static LPVOID AllocateNear(uintptr_t target) {
    for (int i = 0; i < 524288; ++i) {
        int dir = (i % 2 == 0) ? 1 : -1;
        uintptr_t addr = target + dir * (i / 2) * 0x1000;
        addr &= ~static_cast<uintptr_t>(0xFFF);
        LPVOID p = VirtualAlloc(reinterpret_cast<LPVOID>(addr), 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (p) {
            int64_t rel = static_cast<int64_t>(reinterpret_cast<uintptr_t>(p) - (target + 5));
            if (rel >= INT32_MIN && rel <= INT32_MAX)
                return p;
            VirtualFree(p, 0, MEM_RELEASE);
        }
    }
    return nullptr;
}

void Radar::Install() {
    if (g_installed) return;

    g_moduleBase = reinterpret_cast<uintptr_t>(GetModuleHandleA("WindowsEntryPoint.Windows_W10.exe"));
    if (!g_moduleBase) {
        return;
    }

    // AOB: 8B 83 10 05 00 00
    BYTE pattern[] = { 0x8B, 0x83, 0x10, 0x05, 0x00, 0x00 };
    const char* mask = "xxxxxx";
    g_hookAddr = Memory::FindPattern("WindowsEntryPoint.Windows_W10.exe", pattern, mask);
    if (!g_hookAddr) {
        return;
    }

    g_shellcode = AllocateNear(g_hookAddr);
    if (!g_shellcode) {
        return;
    }

    auto code = static_cast<BYTE *>(g_shellcode);
    size_t i = 0;
    uintptr_t retAddr = g_hookAddr + 6;

    // mov [rbx+0x510], 1000
    code[i++] = 0xC7; code[i++] = 0x83;
    int32_t off510 = 0x510;
    memcpy(&code[i], &off510, 4); i += 4;
    int32_t val1000 = 1000;
    memcpy(&code[i], &val1000, 4); i += 4;

    // mov eax, [rbx+0x510]
    code[i++] = 0x8B; code[i++] = 0x83;
    memcpy(&code[i], &off510, 4); i += 4;

    // jmp rel32
    code[i++] = 0xE9;
    int32_t jmpRel = static_cast<int32_t>(retAddr - (reinterpret_cast<uintptr_t>(code + i) + 4));
    memcpy(&code[i], &jmpRel, 4); i += 4;

    memcpy(g_origBytes, reinterpret_cast<LPVOID>(g_hookAddr), 6);

    // JMP (5B) + NOP (1B)
    DWORD oldProt;
    VirtualProtect(reinterpret_cast<LPVOID>(g_hookAddr), 6, PAGE_EXECUTE_READWRITE, &oldProt);
    BYTE jmp[5] = { 0xE9 };
    int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(g_shellcode) - (g_hookAddr + 5));
    memcpy(&jmp[1], &rel, 4);
    memcpy(reinterpret_cast<LPVOID>(g_hookAddr), jmp, 5);
    memset(reinterpret_cast<LPVOID>(g_hookAddr + 5), 0x90, 1);
    VirtualProtect(reinterpret_cast<LPVOID>(g_hookAddr), 6, oldProt, &oldProt);

    g_installed = true;
}

void Radar::Uninstall() {
    if (!g_installed) return;
    if (!g_hookAddr) return;

    DWORD oldProt;
    VirtualProtect(reinterpret_cast<LPVOID>(g_hookAddr), 6, PAGE_EXECUTE_READWRITE, &oldProt);
    memcpy(reinterpret_cast<LPVOID>(g_hookAddr), g_origBytes, 6);
    VirtualProtect(reinterpret_cast<LPVOID>(g_hookAddr), 6, oldProt, &oldProt);
    if (g_shellcode) {
        VirtualFree(g_shellcode, 0, MEM_RELEASE);
        g_shellcode = nullptr;
    }

    g_installed = false;
}