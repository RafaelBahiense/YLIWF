#include <doctest/doctest.h>

#include "combat/CombatRoleRules.h"
#include "combat/ShieldDefenderRules.h"
#include <limits>

TEST_CASE("Defender stamina recovery has separate entry and exit thresholds") {
    using mod::shield_defender_rules::Recovering;
    CHECK_FALSE(Recovering(false, 26, 100));
    CHECK(Recovering(false, 25, 100));
    CHECK(Recovering(true, 49, 100));
    CHECK_FALSE(Recovering(true, 50, 100));
    CHECK(Recovering(false, -1, 100));
    CHECK_FALSE(Recovering(true, 101, 100));
    CHECK(Recovering(false, 10, 0));
    CHECK(Recovering(false, std::numeric_limits<float>::quiet_NaN(), 100));
    CHECK(Recovering(false, 10, std::numeric_limits<float>::infinity()));
}

TEST_CASE("Defenders guard nearby melee attacks aimed at themselves") {
    using namespace mod::shield_defender_rules;
    Threat threat{256, true, true, true};
    CHECK(IncomingMelee(threat, false));
    threat.distance = 300;
    CHECK_FALSE(IncomingMelee(threat, false));
    CHECK(IncomingMelee(threat, true));
    threat.distance = 321;
    CHECK_FALSE(IncomingMelee(threat, true));
    threat.distance = 100;
    threat.targetingDefender = false;
    CHECK_FALSE(IncomingMelee(threat, true));
    threat.targetingDefender = true;
    threat.attacking = false;
    CHECK_FALSE(IncomingMelee(threat, true));
    threat.attacking = true;
    threat.melee = false;
    CHECK_FALSE(IncomingMelee(threat, true));
    threat.melee = true;
    threat.distance = -1;
    CHECK_FALSE(IncomingMelee(threat, true));
    threat.distance = std::numeric_limits<float>::quiet_NaN();
    CHECK_FALSE(IncomingMelee(threat, true));
}

TEST_CASE("Low stamina changes offense without abandoning defense against an attack") {
    using namespace mod::shield_defender_rules;
    CHECK(ChooseStance(false, false) == Stance::Pressure);
    CHECK(ChooseStance(false, true) == Stance::Guard);
    CHECK(ChooseStance(true, false) == Stance::Recover);
    CHECK(ChooseStance(true, true) == Stance::GuardRecover);
    CHECK_FALSE(Guarding(Stance::Recover));
    CHECK(Guarding(Stance::GuardRecover));
    CHECK(DecideStyle(true, true, true, false, false) == StyleAction::Reapply);
    CHECK(DecideStyle(false, true, true, false, false) == StyleAction::Restore);
    CHECK(DecideStyle(true, true, false, false, false) == StyleAction::Release);
}

TEST_CASE("Shield defense yields to stand-down, waiting, scenes and foreign control") {
    using mod::shield_defender_rules::CanDefend;
    CHECK(CanDefend(true, true, false, false, false, false, false));
    CHECK_FALSE(CanDefend(false, true, false, false, false, false, false));
    CHECK_FALSE(CanDefend(true, false, false, false, false, false, false));
    CHECK_FALSE(CanDefend(true, true, true, false, false, false, false));
    CHECK_FALSE(CanDefend(true, true, false, true, false, false, false));
    CHECK_FALSE(CanDefend(true, true, false, false, true, false, false));
    CHECK_FALSE(CanDefend(true, true, false, false, false, true, false));
    CHECK_FALSE(CanDefend(true, true, false, false, false, false, true));
}

TEST_CASE("Defenders counter nearby openings without abandoning guard or stamina recovery") {
    using namespace mod::shield_defender_rules;
    CHECK(CounterOpportunity(192, true, false));
    CHECK(CounterOpportunity(100, false, true));
    CHECK_FALSE(CounterOpportunity(193, true, true));
    CHECK_FALSE(CounterOpportunity(100, false, false));
    CHECK_FALSE(CounterOpportunity(-1, true, true));
    CHECK_FALSE(CounterOpportunity(std::numeric_limits<float>::quiet_NaN(), true, true));
    CHECK(ChooseStance(false, false, true) == Stance::Counter);
    CHECK(ChooseStance(false, true, true) == Stance::Guard);
    CHECK(ChooseStance(true, false, true) == Stance::Recover);
    CHECK(ChooseStance(true, true, true) == Stance::GuardRecover);
}

TEST_CASE("Defender style restores its predecessor without erasing a foreign override") {
    using namespace mod::shield_defender_rules;
    CHECK(DecideStyle(true, false, false, false) == StyleAction::Acquire);
    CHECK(DecideStyle(true, true, true, false) == StyleAction::None);
    CHECK(DecideStyle(true, true, false, true) == StyleAction::Reapply);
    CHECK(DecideStyle(false, true, true, false) == StyleAction::Restore);
    CHECK(DecideStyle(false, true, false, false) == StyleAction::Release);
    CHECK(DecideStyle(true, true, false, false) == StyleAction::Release);
    CHECK(DecideStyle(false, false, false, false) == StyleAction::Release);
}

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
