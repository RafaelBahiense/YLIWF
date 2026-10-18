#pragma once

#include <cstddef>

namespace RE {
    class Actor;
    class SpellItem;
    class TESPackage;
    class NiPoint3;
}

namespace mod::healer_packages {
    inline constexpr std::size_t Slots = 8;
    void Configure();
    bool Ready(std::size_t slot);
    bool Owned(RE::Actor* actor);
    bool ForeignInterrupt(RE::Actor* actor);
    bool Move(RE::Actor* actor, std::size_t slot, const RE::NiPoint3& destination);
    bool Cast(RE::Actor* actor, std::size_t slot, RE::Actor* target, RE::SpellItem* spell);
    bool Casting(RE::Actor* actor, std::size_t slot);
    void Release(RE::Actor* actor);
}
