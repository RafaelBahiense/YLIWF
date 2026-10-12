#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace mod::combat_roles {
    // Stored as faction ranks. Keep these values stable across releases.
    enum class Role : std::int32_t {
        Automatic = 0,
        Healer,
        Archer,
        Mage,
        Frontline,
        ShieldDefender,
        Support,
        Count  // Sentinel, never assigned or saved as a role.
    };
    inline constexpr auto RoleCount = static_cast<std::size_t>(Role::Count);

    constexpr bool Valid(std::int32_t value) {
        return value >= 0 && value < static_cast<std::int32_t>(RoleCount);
    }

    constexpr Role Decode(std::int32_t rank) {
        return Valid(rank) ? static_cast<Role>(rank) : Role::Automatic;
    }

    constexpr std::string_view Name(Role role) {
        constexpr std::array names{"Automatic", "Healer", "Archer", "Mage", "Frontline", "Shield defender", "Support"};
        static_assert(names.size() == RoleCount);
        return Valid(static_cast<std::int32_t>(role)) ? names[static_cast<std::size_t>(role)] : names[0];
    }

    struct Capabilities {
        bool bow = false, crossbow = false, arrows = false, bolts = false;
        bool melee = false, oneHanded = false, shield = false;
        bool allyHealing = false, offensiveSpell = false, supportSpell = false;
        bool healingReady = false, offenseReady = false, supportReady = false;
    };

    struct SpellTraits {
        bool castable = false, targetsActor = false, hostile = false;
        bool restoresHealth = false, damagesHealth = false, allyBuff = false, crowdControl = false;
    };

    constexpr void AddSpell(Capabilities& c, const SpellTraits& spell, bool enoughMagicka) {
        if (!spell.castable || !spell.targetsActor)
            return;
        const bool healing = spell.restoresHealth && !spell.hostile;
        const bool offense = spell.damagesHealth && spell.hostile;
        const bool support = spell.crowdControl || (spell.allyBuff && !spell.hostile);
        c.allyHealing |= healing;
        c.offensiveSpell |= offense;
        c.supportSpell |= support;
        c.healingReady |= healing && enoughMagicka;
        c.offenseReady |= offense && enoughMagicka;
        c.supportReady |= support && enoughMagicka;
    }

    struct Eligibility {
        bool eligible = false, ready = false;
        std::string_view reason;
    };

    constexpr Eligibility Evaluate(Role role, const Capabilities& capabilities) {
        const auto& c = capabilities;
        switch (role) {
            case Role::Automatic:
                return {true, true, {}};
            case Role::Healer:
                return {c.allyHealing, c.allyHealing && c.healingReady,
                        c.allyHealing ? (c.healingReady ? "" : "Insufficient magicka for estimated spell cost")
                                      : "Needs a known healing spell that can target allies"};
            case Role::Archer: {
                const bool ready = (c.bow && c.arrows) || (c.crossbow && c.bolts);
                return {ready, ready, ready ? "" : "Needs a bow with arrows or a crossbow with bolts"};
            }
            case Role::Mage:
                return {c.offensiveSpell, c.offensiveSpell && c.offenseReady,
                        c.offensiveSpell ? (c.offenseReady ? "" : "Insufficient magicka for estimated spell cost")
                                         : "Needs a known offensive spell"};
            case Role::Frontline:
                return {c.melee, c.melee, c.melee ? "" : "Needs a melee weapon"};
            case Role::ShieldDefender: {
                const bool ready = c.oneHanded && c.shield;
                return {ready, ready, ready ? "" : "Needs a one-handed weapon and shield"};
            }
            case Role::Support:
                return {c.supportSpell, c.supportSpell && c.supportReady,
                        c.supportSpell ? (c.supportReady ? "" : "Insufficient magicka for estimated spell cost")
                                       : "Needs a known ally buff or crowd-control spell"};
            case Role::Count:
                break;
        }
        return {false, false, "Unknown role"};
    }
}
