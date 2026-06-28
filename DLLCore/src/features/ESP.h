#pragma once
#include <string>
#include <utility>
#include <vector>
#include <cstdint>

struct PointerChainStep {
	int step;
	std::string description;
	uintptr_t address;
	std::string valueStr;

	PointerChainStep(int s, std::string desc, uintptr_t addr, std::string val = "")
		: step(s), description(std::move(desc)), address(addr), valueStr(std::move(val)) {}
};

struct BoneInfo {
	float x{}, y{}, z{};
	float curX{}, curY{}, curZ{};
	float screenX = 0.0f, screenY = 0.0f;
	bool isOnScreen = false;
	uintptr_t nodeAddress = 0;
	uintptr_t collisionObject = 0;
	std::string boneName;
	int boneId = -1;
	std::string pointerChainStr;
	uintptr_t ownerPlayerAddr = 0;
	std::vector<PointerChainStep> pointerChain;
};

struct PlayerInfo {
	float X{}, Y{}, Z{};
	float HP{};
	float distance = 0.0f;
	bool isValid = false;
	uint32_t Team = 0;
	float headX{}, headY{}, headZ{};
	bool hasHeadBone = false;
	uintptr_t addr_playerCompPtr = 0;
	uintptr_t addr_tempPtr1 = 0;
	uintptr_t addr_PosX = 0;
	uintptr_t addr_PosY = 0;
	uintptr_t addr_PosZ = 0;
	uintptr_t addr_HP = 0;
	std::vector<BoneInfo> bones;
};

extern float LocalPlayerPosition[3];

namespace ESP {
	void Render();
}