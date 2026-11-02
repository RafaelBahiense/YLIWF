#pragma once
#include "DefenderTactics.h"
#include <cstddef>

namespace RE {
    class Actor;
    class TESPackage;
    class TESObjectREFR;
}

namespace mod::defender_movement {
    enum class Status : std::uint32_t { Direct, Detour, Blocked, Tether, Yield };

    struct Observation {
        std::uint32_t stalls = 0, detours = 0;
        Status status = Status::Direct;
        float x = 0, y = 0, z = 0, progress = 0, waiting = 0;
        bool active = false;
    };

    inline constexpr std::size_t Slots = 8;
    void Configure();
    bool Ready(std::size_t slot);
    bool Owned(const RE::TESPackage* package);
    bool Move(RE::Actor* actor, RE::Actor* threat, std::size_t slot, defender_tactics::Motion motion,
              std::span<const defender_tactics::Threat> threats, bool guarded);
    void Sample(RE::Actor* actor, float elapsed);
    Observation Inspect(std::uint32_t actor);
    void Clear();
    bool SetTestBounds(RE::Actor* actor, RE::TESObjectREFR* anchor, float radius);
    void ClearTestBounds();
    void Release(RE::Actor* actor);
}
