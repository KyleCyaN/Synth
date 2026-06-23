// BotRoom.h
#pragma once
#include <cstdint>

namespace BotRoom
{
    void Install();
    void Uninstall();

    bool IsMemoryReady();
    void CheckAndInstall();
    void PeriodicCheck();
    bool GetStatus();
}
