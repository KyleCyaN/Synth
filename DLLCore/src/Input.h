#pragma once

struct GameInput {
	char  pad0[0x28];
	float mouseX;       // 0x28
	float mouseY;       // 0x2C
};

GameInput* GetGameInput();
