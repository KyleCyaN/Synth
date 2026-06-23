#include "Players.h"
#include "features.h"
#include "memory.h"

std::vector<PlayerInfo> GetPlayers() {
	std::vector<PlayerInfo> players;
	uintptr_t baseAddr = Memory::ResolveAddress(ENEMY_LIST_EXPR);
	uintptr_t managerPtr = 0;
	if (!Memory::SafeReadPtr(baseAddr, managerPtr) || !managerPtr) return players;

	int playerOffset = 0x0;
	int step = 0x8;

	for (int i = 0; i < 100; i++) {
		uintptr_t playerCompPtr = 0;
		if (!Memory::SafeReadPtr(managerPtr + i * 0x8, playerCompPtr)) continue;

		uint32_t active = 0;
		PlayerInfo p{};
		p.addr_playerCompPtr = playerCompPtr;

		if (!Memory::SafeReadInt(playerCompPtr + 0x278, active) || active != 1) {
			p.isValid = false;
			players.push_back(p);
			playerOffset += step;
			continue;
		}

		uintptr_t tempPtr1 = 0;
		float worldPos[3], hp = 0;
		if (!Memory::SafeReadPtr(playerCompPtr + 0x1A8, tempPtr1)) { playerOffset += step; continue; }

		if (!Memory::SafeReadFloat(tempPtr1 + 0x28, hp))
		{
			playerOffset += step;
			continue;
		}

		if (hp <= 34.0f)
			continue;

		if (!Memory::SafeReadFloat(playerCompPtr + 0x58, worldPos[0]) ||
			!Memory::SafeReadFloat(playerCompPtr + 0x5C, worldPos[1]) ||
			!Memory::SafeReadFloat(playerCompPtr + 0x60, worldPos[2])) { playerOffset += step; continue; }

		p.X = worldPos[0]; p.Y = worldPos[1]; p.Z = worldPos[2];
		p.HP = hp;
		p.Team = Memory::ReadInt(playerCompPtr + 0xA0);
		p.isValid = true;
		p.addr_playerCompPtr = playerCompPtr;
		players.push_back(p);
		playerOffset += step;
	}

	return players;
}