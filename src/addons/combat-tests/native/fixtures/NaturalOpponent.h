#pragma once
#include "../fixtures/NaturalOpponentRules.h"
#include <cstdint>
#include <string>

namespace RE {
    class Actor;
    class TESObjectREFR;
}

namespace mod::combat_tests::natural {
    inline constexpr std::uint32_t FireMageList = 0x1E7D1;
    // Main-thread only. Shared NPC/list/race records are never modified.
    RE::Actor* Spawn(RE::TESObjectREFR* marker, std::uint32_t token);
    void BeginRun();
    bool Owns(RE::Actor* actor, std::uint32_t base, std::uint32_t token);
    void Capture();
    const std::string& Description();
    const std::string& Details();
    std::uint32_t Level();
    std::uint32_t Participant(std::uint32_t actor, std::uint32_t subject, std::uint32_t enemy);
    void CleanupSummons();
    void Release();
    void Reroll();
    void Reset();
}
