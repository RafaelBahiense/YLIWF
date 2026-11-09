#pragma once
#include <cstdint>

namespace RE {
    class TESNPC;
}

namespace mod::combat_tests::fixture_bases {
    inline constexpr std::uint32_t First = 0x850;
    inline constexpr std::uint32_t Ally = 6;
    inline constexpr std::uint32_t Count = Ally + 1;
    inline constexpr const char* Name = "YLIWF Combat Test Opponent";
    inline constexpr const char* AllyName = "YLIWF Combat Test Archer Ally";
    void Configure();
    void Reset();
    bool Owns(const RE::TESNPC* base);
    bool IsAlly(const RE::TESNPC* base);
    RE::TESNPC* Prepare(std::uint32_t loadout);
}
