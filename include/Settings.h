#pragma once
#include <windows.h>
#include <array>
#include <cstdint>
#include <string>
#include <mutex>
#include "ModInfo.h"

namespace mod::settings {
    inline std::recursive_mutex Mutex;

    constexpr std::size_t kMaxPerkSpecs = 8;
    constexpr const char* kIniPath = mod::info::IniPath;

    struct PerkSpec {
        std::string file;
        std::uint32_t localID = 0;
    };

    inline std::int32_t MaxExtraFollowers = 0;
    inline std::int32_t FollowerPerkOption = 0;
    inline std::int32_t SpeechLevelsPerSlot = 0;
    inline std::array<PerkSpec, kMaxPerkSpecs> PerkSpecs{};
    inline std::size_t PerkSpecCount = 0;
    inline bool FollowerEssential = false;
    inline bool FriendlyFire = false;
    inline bool FollowerSandbox = false;
    inline bool FollowerCrossfire = false;
    inline bool FollowerHomes = false;
    inline char PerkListBuffer[2048]{};
    inline bool Loaded = false;
    inline std::string SaveStatus;
    inline void (*ApplyGateCallback)() = nullptr;
    inline void (*FriendlyFireCallback)() = nullptr;
    inline void (*SandboxCallback)() = nullptr;
    inline void (*CrossfireCallback)() = nullptr;
    inline void (*HomesCallback)() = nullptr;
    inline void (*EssentialCallback)() = nullptr;
    void Load(bool force = false);
    bool Save();
    void ParsePerkListIntoSpecs(const std::string& raw);
    void BuildPerkListBuffer();
}
