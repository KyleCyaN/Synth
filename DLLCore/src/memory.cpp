#include "memory.h"
#include <algorithm>
#include <windows.h>
#include <psapi.h>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "psapi.lib")

namespace Memory
{
	static bool IsValidAddr(const uintptr_t address)
	{
		return address >= 0x10000 && address < 0x7FFFFFFFFFFF;
	}

	template<typename Func>
	bool SafeExecute(Func &&func)
	{
		__try
		{
			return func();
		} __except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	uint8_t ReadByte(uintptr_t address)
	{
		uint8_t v = 0;
		SafeExecute([&]() -> bool
		{
			if (!IsValidAddr(address)) return false;
			v = *reinterpret_cast<uint8_t *>(address);
			return true;
		});
		return v;
	}

	uint16_t ReadShort(uintptr_t address)
	{
		uint16_t v = 0;
		SafeExecute([&]() -> bool
		{
			if (!IsValidAddr(address)) return false;
			v = *reinterpret_cast<uint16_t *>(address);
			return true;
		});
		return v;
	}

	uint32_t ReadInt(uintptr_t address)
	{
		uint32_t v = 0;
		SafeExecute([&]() -> bool
		{
			if (!IsValidAddr(address)) return false;
			v = *reinterpret_cast<uint32_t *>(address);
			return true;
		});
		return v;
	}

	uint64_t ReadLong(uintptr_t address)
	{
		uint64_t v = 0;
		SafeExecute([&]() -> bool
		{
			if (!IsValidAddr(address)) return false;
			v = *reinterpret_cast<uint64_t *>(address);
			return true;
		});
		return v;
	}

	float ReadFloat(const uintptr_t address)
	{
		float v = 0.0f;
		SafeExecute([&]() -> bool
		{
			if (!IsValidAddr(address)) return false;
			v = *reinterpret_cast<float *>(address);
			return true;
		});
		return v;
	}

	double ReadDouble(uintptr_t address)
	{
		double v = 0.0;
		SafeExecute([&]() -> bool
		{
			if (!IsValidAddr(address)) return false;
			v = *reinterpret_cast<double *>(address);
			return true;
		});
		return v;
	}

	void WriteByte(uintptr_t address, uint8_t value)
	{
		SafeExecute([&]() -> bool
		{
			if (!IsValidAddr(address)) return false;

			DWORD oldProtect = 0;
			if (!VirtualProtect(
				reinterpret_cast<LPVOID>(address),
				sizeof(uint8_t),
				PAGE_EXECUTE_READWRITE,
				&oldProtect))
				return false;

			*reinterpret_cast<uint8_t *>(address) = value;

			VirtualProtect(
				reinterpret_cast<LPVOID>(address),
				sizeof(uint8_t),
				oldProtect,
				&oldProtect);

			return true;
		});
	}

	void WriteShort(uintptr_t address, uint16_t value)
	{
		SafeExecute([&]() -> bool
		{
			if (!IsValidAddr(address)) return false;

			DWORD oldProtect = 0;
			if (!VirtualProtect(
				reinterpret_cast<LPVOID>(address),
				sizeof(uint16_t),
				PAGE_EXECUTE_READWRITE,
				&oldProtect))
				return false;

			*reinterpret_cast<uint16_t *>(address) = value;

			VirtualProtect(
				reinterpret_cast<LPVOID>(address),
				sizeof(uint16_t),
				oldProtect,
				&oldProtect);

			return true;
		});
	}

	void WriteInt(uintptr_t address, uint32_t value)
	{
		SafeExecute([&]() -> bool
		{
			if (!IsValidAddr(address)) return false;

			DWORD oldProtect = 0;
			if (!VirtualProtect(
				reinterpret_cast<LPVOID>(address),
				sizeof(uint32_t),
				PAGE_EXECUTE_READWRITE,
				&oldProtect))
				return false;

			*reinterpret_cast<uint32_t *>(address) = value;

			VirtualProtect(
				reinterpret_cast<LPVOID>(address),
				sizeof(uint32_t),
				oldProtect,
				&oldProtect);

			return true;
		});
	}

	void WriteLong(uintptr_t address, uint64_t value)
	{
		SafeExecute([&]() -> bool
		{
			if (!IsValidAddr(address)) return false;

			DWORD oldProtect = 0;
			if (!VirtualProtect(
				reinterpret_cast<LPVOID>(address),
				sizeof(uint64_t),
				PAGE_EXECUTE_READWRITE,
				&oldProtect))
				return false;

			*reinterpret_cast<uint64_t *>(address) = value;

			VirtualProtect(
				reinterpret_cast<LPVOID>(address),
				sizeof(uint64_t),
				oldProtect,
				&oldProtect);

			return true;
		});
	}

	void WriteFloat(uintptr_t address, float value)
	{
		SafeExecute([&]() -> bool
		{
			if (!IsValidAddr(address)) return false;

			DWORD oldProtect = 0;
			if (!VirtualProtect(
				reinterpret_cast<LPVOID>(address),
				sizeof(float),
				PAGE_EXECUTE_READWRITE,
				&oldProtect))
				return false;

			*reinterpret_cast<float *>(address) = value;

			VirtualProtect(
				reinterpret_cast<LPVOID>(address),
				sizeof(float),
				oldProtect,
				&oldProtect);

			return true;
		});
	}

	void WriteDouble(uintptr_t address, double value)
	{
		SafeExecute([&]() -> bool
		{
			if (!IsValidAddr(address)) return false;

			DWORD oldProtect = 0;
			if (!VirtualProtect(
				reinterpret_cast<LPVOID>(address),
				sizeof(double),
				PAGE_EXECUTE_READWRITE,
				&oldProtect))
				return false;

			*reinterpret_cast<double *>(address) = value;

			VirtualProtect(
				reinterpret_cast<LPVOID>(address),
				sizeof(double),
				oldProtect,
				&oldProtect);

			return true;
		});
	}

	bool SafeReadPtr(const uintptr_t address, uintptr_t &out)
	{
		out = 0;
		return SafeExecute([&]() -> bool
		{
			if (!IsValidAddr(address)) return false;
			out = *reinterpret_cast<uintptr_t *>(address);
			return true;
		});
	}

	bool SafeReadByte(const uintptr_t address, uint8_t &out)
	{
		out = ReadByte(address);
		return out != 0;
	}

	bool SafeReadInt(const uintptr_t address, uint32_t &out)
	{
		out = ReadInt(address);
		return out != 0;
	}

	bool SafeReadFloat(const uintptr_t address, float &out)
	{
		return SafeExecute([&]() -> bool
		{
			if (!IsValidAddr(address)) return false;
			out = *reinterpret_cast<float *>(address);
			return true;
		});
	}

	bool SafeReadString(const uintptr_t address, char *buf, const size_t size)
	{
		return SafeExecute([&]() -> bool
		{
			if (!IsValidAddr(address) || !buf || size == 0) return false;

			const char *src = reinterpret_cast<char *>(address);
			for (size_t i = 0; i < size - 1; i++)
			{
				buf[i] = src[i];
				if (buf[i] == '\0') return true;
			}
			buf[size - 1] = '\0';
			return true;
		});
	}

	uintptr_t GetModuleBase(const std::wstring &moduleName)
	{
		HMODULE hMod = GetModuleHandle(moduleName.c_str());
		return reinterpret_cast<uintptr_t>(hMod);
	}

	uintptr_t ResolveAddress(const std::string &expression)
	{
		std::string expr = expression;
		size_t plusPos = expr.find('+');
		if (plusPos == std::string::npos)
		{
			return std::stoull(expr, nullptr, 16);
		}
		std::string modulePart = expr.substr(0, plusPos);
		std::string offsetPart = expr.substr(plusPos + 1);

		std::wstring wmodule(modulePart.begin(), modulePart.end());
		uintptr_t base = GetModuleBase(wmodule);
		if (base == 0) return 0;

		std::string offs = offsetPart;
		size_t arrow;
		while ((arrow = offs.find("->")) != std::string::npos)
		{
			offs.replace(arrow, 2, "+");
		}
		std::vector<uintptr_t> offsets;
		std::stringstream ss(offs);
		std::string token;
		while (std::getline(ss, token, '+'))
		{
			token.erase(0, token.find_first_not_of(" \t"));
			token.erase(token.find_last_not_of(" \t") + 1);
			if (!token.empty())
			{
				uintptr_t off = std::stoull(token, nullptr, 16);
				offsets.push_back(off);
			}
		}
		if (offsets.empty()) return base;

		uintptr_t addr = base + offsets[0];
		for (size_t i = 1; i < offsets.size(); ++i)
		{
			if (addr == 0) break;
			addr = *reinterpret_cast<uintptr_t*>(addr);
			if (addr == 0) break;
			addr += offsets[i];
		}
		return addr;
	}

	uintptr_t ResolvePointerChain(uintptr_t base, const std::vector<uintptr_t> &offsets)
	{
		if (offsets.empty()) return base;

		uintptr_t addr = base + offsets[0];
		for (size_t i = 1; i < offsets.size(); ++i)
		{
			if (addr == 0) return 0;

			uintptr_t tmp = 0;
			if (!SafeReadPtr(addr, tmp)) return 0;
			addr = tmp;

			if (addr == 0) return 0;
			addr += offsets[i];
		}
		return addr;
	}

	uintptr_t FindPattern(const char *moduleName, const BYTE *pattern, const char *mask)
	{
		HMODULE hMod = GetModuleHandleA(moduleName);
		if (!hMod) return 0;

		MODULEINFO modInfo;
		if (!GetModuleInformation(GetCurrentProcess(), hMod, &modInfo, sizeof(modInfo)))
			return 0;

		const auto start = reinterpret_cast<uintptr_t>(hMod);
		const uintptr_t end = start + modInfo.SizeOfImage;
		const size_t patLen = strlen(mask);

		__try
		{
			for (uintptr_t i = start; i < end - patLen; i++)
			{
				bool found = true;
				for (size_t j = 0; j < patLen; j++)
				{
					if (mask[j] == 'x' && *reinterpret_cast<BYTE *>(i + j) != pattern[j])
					{
						found = false;
						break;
					}
				}
				if (found) return i;
			}
		} __except (EXCEPTION_EXECUTE_HANDLER)
		{
			return 0;
		}

		return 0;
	}
}
