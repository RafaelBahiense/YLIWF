#pragma once
#include <cstdint>
#include <algorithm>
#include <span>

namespace mod::combat_tests::fixture_spells {
    enum class Kind { Spell, Power, LesserPower, VoicePower, Scroll, Staff, Passive, Other };
    enum class EffectKind { Other, NightVision, RegisteredScriptedVision };

    struct Effect {
        EffectKind kind = EffectKind::Other;
        bool hostile = true;
    };

    constexpr bool VisionOnly(bool self, std::span<const Effect> effects) {
        return self && !effects.empty() && std::ranges::all_of(effects, [](const auto& effect) {
                   return !effect.hostile && effect.kind != EffectKind::Other;
               });
    }

    constexpr bool Remove(std::uint32_t id, Kind kind, bool constant, std::uint32_t expected) {
        return id && id != expected && !constant && kind <= Kind::Staff;
    }

    constexpr bool Owned(bool actorDynamic, bool baseOwned, bool baseMatches) {
        return actorDynamic && baseOwned && baseMatches;
    }
}
