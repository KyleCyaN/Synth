#pragma once
#include <vector>
#include "ESP.h"

namespace TeleportToHead {
	extern bool isEnabled;
	void Run(std::vector<PlayerInfo> &players);
	bool InitFunctions();
	void Install();
	void Uninstall();

	extern float g_furyTargetPos[3];
	extern bool g_furyActive;
	extern int g_furyTargetIndex;
}