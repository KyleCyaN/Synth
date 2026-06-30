#pragma once
#include <vector>
#include "ESP.h"

namespace TeleportToHead {
	extern bool isEnabled;
	void Run(std::vector<PlayerInfo> &players);
	bool InitFunctions();
	void Install();
	void Uninstall();

	extern float g_teleportToHeadTargetPos[3];
	extern bool g_teleportToHeadActive;
	extern int g_teleportToHeadTargetIndex;
}