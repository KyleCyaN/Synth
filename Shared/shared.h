#pragma once
#include <Windows.h>

#pragma pack(push, 1)

struct SharedData
{
	volatile LONG unloadRequested;
};

#pragma pack(pop)