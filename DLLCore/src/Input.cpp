#include "Input.h"
#include <cstdint>

#include "memory.h"

GameInput* GetGameInput() {
	if (uintptr_t addr = Memory::ResolveAddress("WindowsEntryPoint.Windows_W10.exe+01BFBB50->0x8->0x48->0x10"))
		return  reinterpret_cast<GameInput *>(*reinterpret_cast<uintptr_t *>(addr));
	return nullptr;
}
