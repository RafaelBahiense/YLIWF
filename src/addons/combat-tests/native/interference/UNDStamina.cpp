#include "UNDStamina.h"
#include "RE/Skyrim.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

bool mod::combat_tests::interference::und::StaminaCost(RE::Actor* actor, std::uintptr_t base, float& cost) {
    if (!actor || !actor->Is3DLoaded() || !base)
        return false;
    constexpr std::array<unsigned char, 8> entry{0x48, 0x8B, 0xC4, 0x53, 0x48, 0x83, 0xEC, 0x70};
    if (std::memcmp(reinterpret_cast<void*>(base + 0x50FD0), entry.data(), entry.size()))
        return false;
    if (!*reinterpret_cast<const bool*>(base + 0x748B1)) {
        cost = 0;
        return true;
    }
    using Calculate = float (*)(void*, RE::Actor*, const void*, bool);
    const auto calculate = reinterpret_cast<Calculate>(base + 0x50FD0);
    auto* owner = reinterpret_cast<void*>(base + 0x78FC0);
    const auto* factors = reinterpret_cast<const void*>(base + 0x79004);
    const float step = calculate(owner, actor, factors, false);
    const float roll = calculate(owner, actor, factors, true);
    if (!std::isfinite(step) || !std::isfinite(roll) || step < 0 || roll < 0)
        return false;
    cost = std::max(step, roll);
    return true;
}
