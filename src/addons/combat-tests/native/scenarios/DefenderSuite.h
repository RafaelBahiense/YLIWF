#pragma once
#include <cstdint>
#include "../testing/Definition.h"
#include "../testing/Selection.h"
#include "../testing/CaseCatalog.h"
#include "DefenderAssessment.h"
#include "../fixtures/Archetypes.h"
#include "DefenderTuning.h"
#include "Scenario.h"
#include "../testing/Preparation.h"
#include "../testing/EncounterOutcome.h"

namespace mod::combat_tests::suite {
    enum class Fixture : std::uint32_t {
        Sustained,
        LowStamina,
        RecoveryWaves,
        Approach,
        Reposition,
        Endurance,
        Heavy,
        Archer,
        Projectile,
        Concentration,
        AllyPressure,
        Multiple,
        Area,
        GroupProtection,
        LeveledMage
    };

    namespace tags {
        inline const testing::Tag Ranged{"ranged"};
        inline const testing::Tag Validation{"validation"};
        inline const testing::Tag ProtectionReview{"protection-review"};
        inline const testing::Tag Acceptance{"acceptance"};
    }

    using Loadout = fixtures::Archetype;

    inline testing::TestCase DefenderCase(Fixture id, const char* name, const char* description) {
        return testing::TestCase{static_cast<std::uint32_t>(id), name, description}
            .Expect(Requirements::Melee())
            .Prepare([](testing::Context& context) { return context.PrepareSubject(); });
    }

    inline auto Opponent(const char* key, Loadout loadout = Loadout::Sword) {
        return fixtures::Spawner(key, loadout).Target("subject");
    }

    inline testing::Phase RecoveryPhase(const char* name, float seconds) {
        return testing::Phase{name, seconds}.OnEnter(testing::Action::StaminaFraction, .15f);
    }

    inline const auto Definition =
        testing::Suite{"Shield defender"}
            .Configure("Defender tuning")
            .Prepare(testing::PrepareArenaWithObserver)
            .Case(DefenderCase(Fixture::Sustained, "Ordinary melee",
                               "Full resources; sword pressure, guard release and offense.")
                      .Tag(tags::Validation)
                      .Tag(tags::Acceptance)
                      .Actor(Opponent("swordsman"))
                      .Phase({"Sustained pressure", 45}))
            .Case(DefenderCase(Fixture::Heavy, "Heavy melee",
                               "Two-handed weapon pressure; report actual power-attack coverage.")
                      .Expect(Requirements::Tactical({Criterion::HeavyWindup, Criterion::DefensiveDecision,
                                                      Criterion::NativeBash, Criterion::DodgeBudget}))
                      .Tag(tags::Validation)
                      .Tag(tags::Acceptance)
                      .Actor(Opponent("heavy", Loadout::Heavy))
                      .Phase({"Heavy attacks", 45}))
            .Case(
                DefenderCase(Fixture::LowStamina, "Low stamina", "Begin combat at 15% stamina; allow natural recovery.")
                    .Expect(Requirements::Melee({Criterion::RecoveryExit}))
                    .Tag(tags::Validation)
                    .Tag(tags::Acceptance)
                    .Actor(Opponent("swordsman"))
                    .Phase(RecoveryPhase("Recover under pressure", 60)))
            .Case(DefenderCase(
                      Fixture::AllyPressure, "Ally protection",
                      "Protect a closer archer ally with finite health; measure survival, damage and intervention.")
                      .Expect(Requirements::Tactical({Criterion::OpponentAttack, Criterion::AllyThreat,
                                                      Criterion::AllyExposure, Criterion::ReachOpponent,
                                                      Criterion::Intervention}))
                      .Tag(tags::Validation)
                      .Tag(tags::ProtectionReview)
                      .Tag(tags::Acceptance)
                      .Actor(Opponent("attacker").At(0, 900).Target("ally"))
                      .Actor(fixtures::Spawner("ally", Loadout::ArcherAlly)
                                 .Role(testing::ActorRole::Ally)
                                 .ProtectHealth(false)
                                 .HealthFromSubject(.75f, 300, 900)
                                 .At(0, 600)
                                 .Target("attacker"))
                      .Variation<yliwf::testing::DefenderTuning>(0, "Challenge on",
                                                                 [](auto& tuning) { tuning.challengeEnabled = 1; })
                      .Variation<yliwf::testing::DefenderTuning>(1, "Challenge off",
                                                                 [](auto& tuning) { tuning.challengeEnabled = 0; })
                      .AlternateVariations()
                      .Phase({"Intercept and protect", 45}))
            .Case(DefenderCase(Fixture::RecoveryWaves, "Recovery waves",
                               "Three consecutive low-stamina recovery windows.")
                      .Actor(Opponent("swordsman"))
                      .Phase(RecoveryPhase("First wave", 20))
                      .Phase(RecoveryPhase("Second wave", 20))
                      .Phase(RecoveryPhase("Third wave", 20)))
            .Case(DefenderCase(Fixture::Approach, "Distance closing",
                               "Begin 1,500 units apart; observe attack and guard handoff.")
                      .Actor(Opponent("swordsman").At(0, 1500))
                      .Phase({"Close and engage", 60}))
            .Case(DefenderCase(Fixture::Reposition, "Repositioning",
                               "Pressure from the front, left and right without resetting the encounter.")
                      .Actor(Opponent("swordsman"))
                      .Phase({"Frontal pressure", 20})
                      .Phase(testing::Phase{"Left flank", 20}.OnEnter(testing::Action::Reposition, -220))
                      .Phase(testing::Phase{"Right flank", 20}.OnEnter(testing::Action::Reposition, 220)))
            .Case(DefenderCase(Fixture::Endurance, "Endurance",
                               "Two minutes of uninterrupted pressure and stamina management.")
                      .Actor(Opponent("swordsman"))
                      .Phase({"Endurance", 120}))
            .Case(DefenderCase(Fixture::Archer, "Archer approach",
                               "Bow opponent begins at range; measure exposure and closing.")
                      .Expect(Requirements::Tactical({Criterion::OpponentAttack, Criterion::BowThreat,
                                                      Criterion::Approach, Criterion::ReachOpponent}))
                      .Tag(tags::Ranged)
                      .Tag(tags::ProtectionReview)
                      .Tag(tags::Acceptance)
                      .Actor(Opponent("archer", Loadout::Bow).At(0, 1500))
                      .Phase({"Close under fire", 45}))
            .Case(DefenderCase(Fixture::Projectile, "Projectile caster",
                               "Firebolt opponent; observe lateral avoidance and approach.")
                      .Expect(Requirements::Tactical(
                          {Criterion::OpponentCast, Criterion::ProjectileThreat, Criterion::Reposition}))
                      .Tag(tags::Ranged)
                      .Tag(tags::ProtectionReview)
                      .Tag(tags::Acceptance)
                      .Actor(Opponent("mage", Loadout::Projectile).At(0, 1500))
                      .Phase({"Projectile pressure", 45}))
            .Case(DefenderCase(Fixture::Concentration, "Sustained caster",
                               "Flames opponent; observe movement out of the casting line.")
                      .Expect(Requirements::Tactical(
                          {Criterion::OpponentCast, Criterion::ConcentrationThreat, Criterion::Reposition}))
                      .Tag(tags::Ranged)
                      .Tag(tags::ProtectionReview)
                      .Tag(tags::Acceptance)
                      .Actor(Opponent("mage", Loadout::Concentration))
                      .Phase({"Sustained casting", 45}))
            .Case(DefenderCase(Fixture::Multiple, "Two attackers",
                               "Two protected sword opponents pressure the defender from different angles.")
                      .Expect(Requirements::Tactical({Criterion::MultipleThreats, Criterion::RepositionDecision}))
                      .Tag(tags::ProtectionReview)
                      .Tag(tags::Acceptance)
                      .Actor(Opponent("attacker"))
                      .Actor(Opponent("flanker").At(-256, -256))
                      .Phase({"Overlapping pressure", 45}))
            .Case(DefenderCase(Fixture::Area, "Area-capable caster",
                               "Fireball opponent; observe spacing against an area-capable projectile.")
                      .Expect(Requirements::Tactical(
                          {Criterion::OpponentCast, Criterion::AreaThreat, Criterion::Reposition}))
                      .Tag(tags::Ranged)
                      .Tag(tags::Acceptance)
                      .Actor(Opponent("mage", Loadout::Area))
                      .Phase({"Area-capable casting", 45}))
            .Case(DefenderCase(Fixture::GroupProtection, "Protect against two attackers",
                               "Protect a finite-health archer from two independently positioned attackers.")
                      .SubjectTarget("attacker")
                      .Expect(Requirements::Tactical({Criterion::OpponentAttack, Criterion::AllyThreat,
                                                      Criterion::AllyExposure, Criterion::ReachOpponent,
                                                      Criterion::Intervention, Criterion::MultipleThreats,
                                                      Criterion::AllOpponentsEngaged}))
                      .Actor(Opponent("attacker").At(0, 900).Target("ally"))
                      .Actor(Opponent("flanker").At(-256, 650).Target("subject"))
                      .Actor(fixtures::Spawner("ally", Loadout::ArcherAlly)
                                 .Role(testing::ActorRole::Ally)
                                 .ProtectHealth(false)
                                 .HealthFromSubject(.75f, 300, 900)
                                 .At(0, 600)
                                 .Target("attacker"))
                      .Phase({"Protect under cross-pressure", 45}))
            .Case(DefenderCase(Fixture::LeveledMage, "Leveled mage encounter",
                               "Face a leveled fire mage with its normal spells, equipment, perks and resources. Both "
                               "hands remain available. Ends in victory, defeat or a 60-second time limit.")
                      .Expect(testing::OutcomeAssessment{})
                      .Actor(
                          Opponent("opponent", Loadout::LeveledMage).PreserveLoadout().ProtectHealth(false).At(0, 1500))
                      .Phase({"Fight", 60}));

    inline const testing::CaseCatalog Catalog{Definition};
    inline const testing::NamedSelection DefenderValidation{"Validation", tags::Validation};
    inline const testing::NamedSelection ProtectionReview{"Protection review", tags::ProtectionReview, true};
    inline const testing::NamedSelection DefenderAcceptance{"Defender acceptance", tags::Acceptance, true};
    inline constexpr scenarios::Entry Navigation{
        0,
        5,
        "Shield defender",
        "Defender validation",
        "Validate melee defense, heavy attacks, recovery and ally protection. Select one case or a batch.",
        "Needs a one-handed weapon and shield.",
        &Definition,
        0,
        5,
        3,
        scenarios::Execution::Defender};
    inline constexpr scenarios::Entry LeveledMageNavigation{
        6,
        5,
        "Shield defender",
        "Leveled mage encounter",
        "Face a leveled fire mage with its normal spells, equipment, perks and resources. Both hands remain available. "
        "Ends in victory, defeat or a 60-second time limit.",
        "Needs a one-handed weapon and shield.",
        &Definition,
        static_cast<std::uint32_t>(Fixture::LeveledMage),
        5,
        3,
        scenarios::Execution::Natural};
}
