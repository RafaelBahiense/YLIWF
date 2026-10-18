#include <doctest/doctest.h>

#include "combat/CombatRoleRules.h"

namespace {
    using namespace mod::combat_roles;

    TEST_CASE("Self healing and passive abilities do not qualify an ally healer") {
        Capabilities capabilities;
        AddSpell(capabilities, {.castable = true, .restoresHealth = true}, true);
        CHECK_FALSE(Evaluate(Role::Healer, capabilities).eligible);

        AddSpell(capabilities, {.targetsActor = true, .restoresHealth = true}, true);
        CHECK_FALSE(Evaluate(Role::Healer, capabilities).eligible);

        AddSpell(capabilities, {.castable = true, .targetsActor = true, .hostile = true, .restoresHealth = true}, true);
        CHECK_FALSE(Evaluate(Role::Healer, capabilities).eligible);

        AddSpell(capabilities, {.castable = true, .targetsActor = true, .restoresHealth = true}, true);
        CHECK(Evaluate(Role::Healer, capabilities).ready);
    }

    TEST_CASE("Low magicka preserves eligibility and reports temporary readiness") {
        Capabilities capabilities;
        const SpellTraits healing{.castable = true, .targetsActor = true, .restoresHealth = true};
        AddSpell(capabilities, healing, false);
        auto result = Evaluate(Role::Healer, capabilities);
        CHECK(result.eligible);
        CHECK_FALSE(result.ready);
        CHECK_FALSE(result.reason.empty());

        AddSpell(capabilities, healing, true);
        result = Evaluate(Role::Healer, capabilities);
        CHECK(result.ready);
        CHECK(result.reason.empty());
    }

    TEST_CASE("Archers need ammunition matching at least one carried weapon") {
        Capabilities capabilities{.bow = true, .bolts = true};
        CHECK_FALSE(Evaluate(Role::Archer, capabilities).eligible);
        capabilities.crossbow = true;
        CHECK(Evaluate(Role::Archer, capabilities).eligible);
        capabilities.bolts = false;
        CHECK_FALSE(Evaluate(Role::Archer, capabilities).eligible);
        capabilities.arrows = true;
        CHECK(Evaluate(Role::Archer, capabilities).eligible);
    }

    TEST_CASE("A shield defender requires a one-handed weapon and a shield") {
        Capabilities capabilities{.melee = true, .shield = true};
        CHECK(Evaluate(Role::Frontline, capabilities).eligible);
        CHECK_FALSE(Evaluate(Role::ShieldDefender, capabilities).eligible);
        capabilities.oneHanded = true;
        CHECK(Evaluate(Role::ShieldDefender, capabilities).eligible);
        capabilities.shield = false;
        CHECK_FALSE(Evaluate(Role::ShieldDefender, capabilities).eligible);
    }

    TEST_CASE("Control spells qualify support without pretending to be offensive damage") {
        Capabilities capabilities;
        AddSpell(capabilities, {.castable = true, .targetsActor = true, .hostile = true, .crowdControl = true}, true);
        CHECK(Evaluate(Role::Support, capabilities).eligible);
        CHECK_FALSE(Evaluate(Role::Mage, capabilities).eligible);
        AddSpell(capabilities, {.castable = true, .targetsActor = true, .hostile = true, .damagesHealth = true}, true);
        CHECK(Evaluate(Role::Mage, capabilities).ready);
    }

    TEST_CASE("An assignment remains identifiable after requirements are lost") {
        const auto assigned = Decode(static_cast<std::int32_t>(Role::Healer));
        CHECK(assigned == Role::Healer);
        CHECK_FALSE(Evaluate(assigned, {}).eligible);
        CHECK(Evaluate(Role::Automatic, {}).ready);
        CHECK(Decode(-1) == Role::Automatic);
        CHECK(Decode(127) == Role::Automatic);
        CHECK_FALSE(Valid(-1));
        CHECK_FALSE(Valid(static_cast<std::int32_t>(Role::Count)));
    }
}
