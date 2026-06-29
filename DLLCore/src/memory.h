#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <windows.h>

namespace Memory
{
    uint8_t ReadByte(uintptr_t address);

    uint16_t ReadShort(uintptr_t address);

    uint32_t ReadInt(uintptr_t address);

    uint64_t ReadLong(uintptr_t address);

    float ReadFloat(uintptr_t address);

    double ReadDouble(uintptr_t address);

    void WriteByte(uintptr_t address, uint8_t value);

    void WriteShort(uintptr_t address, uint16_t value);

    void WriteInt(uintptr_t address, uint32_t value);

    void WriteLong(uintptr_t address, uint64_t value);

    void WriteFloat(uintptr_t address, float value);

    void WriteDouble(uintptr_t address, double value);

    bool SafeReadPtr(uintptr_t address, uintptr_t& out);

    bool SafeReadByte(uintptr_t address, uint8_t& out);

    bool SafeReadInt(uintptr_t address, uint32_t& out);

    bool SafeReadFloat(uintptr_t address, float& out);

    bool SafeReadString(uintptr_t address, char* buf, size_t size);

    uintptr_t GetModuleBase(const std::wstring& moduleName);

    uintptr_t ResolveAddress(const std::string& expr);

    uintptr_t ResolvePointerChain(uintptr_t base, const std::vector<uintptr_t>& offsets);

    uintptr_t FindPattern(const char* module, const BYTE* pattern, const char* mask);

    void* AllocateNear(uintptr_t target);
}
