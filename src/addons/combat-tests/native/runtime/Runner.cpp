#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include "Runner.h"
#include "ParticipantActors.h"
#include "../scenarios/PrimitiveSuite.h"
#include "../telemetry/TacticalMetrics.h"
#include "../configuration/TuningProfile.h"
#include "../arena/ObserverPosition.h"
#include "../fixtures/FixtureStartupRules.h"
#include "StartupCombatRules.h"
#include "../testing/SuiteSession.h"
#include "../testing/Actors.h"
#include "../fixtures/FixtureSpells.h"
#include "../fixtures/FixtureBases.h"
#include "../testing/Assessment.h"
#include "../testing/EncounterOutcome.h"
#include "../fixtures/NaturalOpponent.h"
#include "../arena/SceneControl.h"
#include "../fixtures/FixtureProtection.h"
#include "../arena/ObserverControl.h"
#include "../interference/ExternalAttacks.h"
#include "../interference/DodgeBudgetRules.h"
#include "../ui/Menu.h"
#include "Rules.h"
#include "AddonInfo.h"
#include "ModInfo.h"
#include <Windows.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>
#include <bit>

namespace {
    using namespace yliwf::testing;
    using mod::test_rules::Plan;
    void Stop(Phase result, const char* reason);
    namespace travel = mod::combat_tests::travel;
    namespace natural = mod::combat_tests::natural;
    namespace scene_control = mod::combat_tests::scene_control;
    namespace observer_control = mod::combat_tests::observer_control;
    namespace spell_diagnostic = mod::combat_tests::spell_diagnostic;
    natural::Outcome encounterOutcome = natural::Outcome::NotMeasured;

    bool NaturalEncounter();
    namespace interference = mod::combat_tests::interference;
    using Clock = std::chrono::steady_clock;
    static_assert(mod::addon::RequiredAdapterAPIVersion == yliwf::testing::Version);
    constexpr char ArenaPlugin[] = "YouLeadIWillFollow.CombatArena.esp";
    constexpr std::uint32_t PowerBashIdle = 0x820, PowerAttackRoot = 0x13384;
    constexpr std::array<RE::FormID, 6> FixtureItems{0x12EB7, 0x1359D, 0x13985, 0x12FD0, 0x12FCD, 0x1C789};
    constexpr std::uint32_t SerializationID = 0x54434C59;  // YLCT
    constexpr std::uint32_t PlanRecord = 0x4E414C50;       // PLAN
    std::recursive_mutex mutex;
    Host host;
    Snapshot snapshot;

    Plan plan;
    std::atomic_bool ready{false}, active{false}, tickQueued{false};
    std::atomic_bool recheckRequested{false};
    std::atomic_uint32_t session{1};
    std::uint32_t ticket = 0, requestedActor = 0;
    bool initialized = false, abortRequested = false, beginPending = false, beginIssued = false;
    bool configured = false, normalized = false;
    mod::combat_tests::fixture_startup::Settling fixtureSettling;
    bool restoreCombatStopped = false;
    bool defenseBegun = false;
    bool restoredForReview = false;
    bool recoverableDefeat = false;
    bool allyDefeated = false;
    float defeatRecoveryElapsed = 0;
    bool previousBlock = false, previousBash = false, previousAttack = false, unsupported = false;
    Phase outcome = Phase::Complete;
    float phaseTime = 0, previousHealth = 0, enemyDamage = 0, auxiliaryDamage = 0, startupWait = 0;
    float seriesAllyHealth = 0;
    std::uint32_t fixtureDriftSamples = 0;
    float previousPlayerHealth = 0;
    std::map<std::string, float> behaviorTimes;

    struct HitCounts {
        std::uint32_t incoming = 0, blocked = 0, power = 0, bash = 0, outgoing = 0, outgoingBash = 0;
    } hitCounts;

    std::ofstream csv;
    std::string reportFile, stopReason;
    std::string seriesReport;
    std::vector<RunResult> results;
    std::uint32_t requestedRuns = 1, currentRun = 1;
    bool nextRun = false, travelMenuPending = false;
    bool suiteActive = false;
    bool defenderBatch = false;
    std::uint32_t selectedEncounter = 0;

    std::uint32_t suiteRepeats = mod::combat_tests::testing::DefaultRepeats;
    std::uint32_t suiteSelection = mod::combat_tests::suite::Catalog.Tagged(mod::combat_tests::suite::tags::Validation);
    bool suiteExpandVariations = false;
    std::uint32_t fixtureSteps = 0;
    DefenderObservation lastDefense;
    mod::combat_tests::tactics::Metrics tacticalMetrics;
    mod::combat_tests::engagement::Metrics engagementMetrics;
    mod::combat_tests::startup_combat::Snapshot startupCombat;
    mod::combat_tests::testing::SuiteSession testSession;
    bool casePreparationStarted = false;
    mod::combat_tests::ParticipantActors participantActors;
    mod::combat_tests::comparison::Setup comparisonSetup;

    Experiments experiments;
    DefenderTuning tuningProfile;
    std::uint32_t tuningRevision = 0;
    std::string tuningStatus;
    std::uint32_t fixtureSpellsRemoved = 0, fixtureSpellPasses = 0, unexpectedFixtureSpells = 0;
    std::vector<mod::combat_tests::fixture_spells::Spell> fixtureSpellChanges;
    mod::combat_tests::metrics::Tracker metricTracker;

    mod::combat_tests::testing::RunPlan SuitePlan() {
        return defenderBatch
                   ? mod::combat_tests::testing::RunPlan{mod::combat_tests::suite::Catalog,
                                                         {suiteRepeats, suiteSelection, suiteExpandVariations}}
                   : mod::combat_tests::testing::RunPlan{
                         mod::test_rules::EncounterCatalog(selectedEncounter),
                         {requestedRuns,
                          mod::combat_tests::testing::CaseMask(mod::test_rules::Scenarios[selectedEncounter].caseID),
                          false}};
    }

    const mod::combat_tests::testing::TestCase* SuiteCase() {
        return suiteActive ? SuitePlan().ForRun(currentRun) : nullptr;
    }

    bool NaturalEncounter() {
        const auto* test = SuiteCase();
        return test ? test->Expectations<mod::combat_tests::testing::OutcomeAssessment>() != nullptr
                    : mod::test_rules::NaturalEncounter(snapshot.encounter);
    }

    TestPhase PhaseSummary(std::size_t index) {
        TestPhase result;
        const auto& phases = testSession.current.Results();
        if (!suiteActive || index >= phases.size())
            return result;
        const auto* definition = SuiteCase();
        const auto& phase = phases[index];
        result.index = static_cast<std::uint32_t>(index + 1);
        result.count = static_cast<std::uint32_t>(phases.size());
        result.entered = phase.entered;
        result.completed = phase.completed;
        result.elapsed = phase.elapsed;
        result.duration = definition->phases[index].seconds;
        std::snprintf(result.name, sizeof(result.name), "%s", phase.name);
        return result;
    }

    void UpdatePhaseSummary() {
        if (!suiteActive || testSession.current.Results().empty())
            return;
        auto index = testSession.current.Index();
        if (testSession.current.Done() && index)
            --index;
        snapshot.testPhase = PhaseSummary(std::min<std::size_t>(index, testSession.current.Results().size() - 1));
    }

    bool bypassInterruptions = false;
    bool bypassDodging = false;
    Clock::time_point lastTick;

    std::uint32_t OpponentLoadout();

    RE::FormID FixtureItem(std::uint32_t loadout) {
        return loadout < FixtureItems.size() ? FixtureItems[loadout] : 0;
    }

    std::uint32_t StartRun(std::uint32_t actor, std::uint32_t encounter);

    struct Record {
        std::uint32_t type, version;
        std::vector<std::uint8_t> bytes;
    };

    std::vector<Record> originalRecords;

    RE::Actor* Actor(std::uint32_t id) {
        return RE::TESForm::LookupByID<RE::Actor>(id);
    }

    RE::TESObjectREFR* Reference(std::uint32_t id) {
        return RE::TESForm::LookupByID<RE::TESObjectREFR>(id);
    }

    RE::TESObjectREFR* Marker(std::uint32_t id) {
        auto* data = RE::TESDataHandler::GetSingleton();
        return data ? data->LookupForm<RE::TESObjectREFR>(id, ArenaPlugin) : nullptr;
    }

    float Value(RE::Actor* actor, RE::ActorValue value) {
        return actor->AsActorValueOwner()->GetActorValue(value);
    }

    float Maximum(RE::Actor* actor, RE::ActorValue value) {
        return actor->AsActorValueOwner()->GetPermanentActorValue(value) +
               actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, value);
    }

    void RestoreValue(RE::Actor* actor, RE::ActorValue value, float target) {
        actor->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, value,
                                                      target - Value(actor, value));
    }

    void Status(const char* text) {
        std::snprintf(snapshot.status, sizeof(snapshot.status), "%s", text);
    }

    mod::combat_tests::resources::Stats ResourceStats(RE::Actor* actor) {
        if (!actor)
            return {};
        return {Maximum(actor, RE::ActorValue::kHealth),       Maximum(actor, RE::ActorValue::kStamina),
                Maximum(actor, RE::ActorValue::kMagicka),      Value(actor, RE::ActorValue::kHealRate),
                Value(actor, RE::ActorValue::kStaminaRate),    Value(actor, RE::ActorValue::kMagickaRate),
                Value(actor, RE::ActorValue::kHealRateMult),   Value(actor, RE::ActorValue::kStaminaRateMult),
                Value(actor, RE::ActorValue::kMagickaRateMult)};
    }

    std::uint64_t EquipmentFingerprint(RE::Actor* actor) {
        if (!actor)
            return 0;
        std::uint64_t hash = 14695981039346656037ull;
        const auto mix = [&](const RE::TESForm* item) {
            hash = (hash ^ (item ? item->GetFormID() : 0)) * 1099511628211ull;
        };
        mix(actor->GetEquippedObject(false));
        mix(actor->GetEquippedObject(true));
        for (unsigned slot = 0; slot < 32; ++slot)
            mix(actor->GetWornArmor(static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>(1u << slot), true));
        return hash;
    }

    std::uint64_t ParticipantFingerprint() {
        std::uint64_t hash = 14695981039346656037ull;
        for (const auto& entry : participantActors.participants.Entries()) {
            auto* actor = Actor(entry.id);
            hash = (hash ^ EquipmentFingerprint(actor)) * 1099511628211ull;
            for (const auto stat : ResourceStats(actor))
                hash = (hash ^ std::bit_cast<std::uint32_t>(stat)) * 1099511628211ull;
        }
        return hash;
    }

    void WriteSeriesReport(bool continuing, const char* note = "") {
        if (reportFile.empty())
            return;
        const auto directory = SKSE::log::log_directory();
        if (!directory)
            return;
        auto path = *directory / reportFile;
        path.replace_extension(".txt");
        std::ofstream output(path);
        output << "YLIWF Combat tests\nRuns finished: " << results.size() << " / " << requestedRuns
               << (continuing ? "\nSeries in progress\n" : "\nSeries ended\n") << note << '\n';
        if (suiteActive)
            output << "Suite phase,Repetition,Verdict,";
        else if (mod::test_rules::DirectEncounter(snapshot.encounter))
            output << "Verdict,";
        output << "Run,Result,Seconds,Health lost,Opponent attacks,Block starts,Bash starts,Incoming hits,Blocked "
                  "hits,Ally heals,Damage per second,Dodge episodes,Dodge net loss,Recovery entries,Recovery exits,"
                  "Challenge,Ally damage,Ally exposure,First intervention,First reach,First contact,Closing damage,"
                  "Aim-line exposure,Aim-line exits,Target switches,Directed evade requests,Directed evade completed,"
                  "Directed evade timeouts,Flight readable seconds,Quiet projectile path seconds,Projectile pressure "
                  "seconds\n";
        for (std::size_t i = 0; i < results.size(); ++i) {
            const auto& row = results[i];
            if (suiteActive) {
                const auto run = static_cast<std::uint32_t>(i + 1);
                output << SuitePlan().ForRun(run)->name << ',' << SuitePlan().At(run).repetition << ','
                       << mod::combat_tests::suite::Name(row.verdict) << ',';
            } else if (mod::test_rules::DirectEncounter(snapshot.encounter))
                output << mod::combat_tests::suite::Name(row.verdict) << ',';
            output << i + 1 << ','
                   << (row.encounterOutcome != natural::Outcome::NotMeasured
                           ? natural::Name(row.encounterOutcome).data()
                       : row.phase == Phase::Complete ? "Complete"
                       : row.phase == Phase::Aborted  ? "Aborted"
                                                      : "Failed")
                   << ',' << row.elapsed << ',' << row.damage << ',' << row.attacks << ',' << row.blocks << ','
                   << row.bashes << ',' << row.incoming << ',' << row.blocked << ',' << row.healing << ','
                   << (row.metrics.seconds > 0 ? row.metrics.damage / row.metrics.seconds : 0) << ','
                   << row.metrics.dodgeEpisodes << ',' << row.metrics.dodgeNetLoss << ',' << row.metrics.recoveryEntries
                   << ',' << row.metrics.recoveryExits << ',' << row.challengeEnabled << ',' << row.allyDamage << ','
                   << row.allyExposure << ',' << row.firstIntervention << ',' << row.engagement.firstReach << ','
                   << row.engagement.firstContact << ',' << row.engagement.closingDamage << ','
                   << row.engagement.aimLineExposure << ',' << row.engagement.aimLineExits << ','
                   << row.engagement.targetSwitches << ',' << row.evadeRequests << ',' << row.evadeCompleted << ','
                   << row.evadeTimeouts << ',' << row.engagement.flightReadableSeconds << ','
                   << row.engagement.projectileGapSeconds << ',' << row.engagement.projectilePressureSeconds << '\n';
        }
        if (defenderBatch) {
            using namespace mod::combat_tests::suite;
            output << "\nDEFENDER VALIDATION - " << suiteRepeats << " inherited repetitions per case\n";
            for (const auto phase : Catalog.Order()) {
                if (!(suiteSelection & (1u << phase)))
                    continue;
                std::uint32_t passed = 0, failed = 0, inconclusive = 0, completed = 0;
                mod::combat_tests::testing::OutcomeCounts outcomes;
                float damage = 0;
                std::uint32_t incoming = 0, blocked = 0;
                for (std::size_t i = 0; i < results.size(); ++i) {
                    if (SuitePlan().At(static_cast<std::uint32_t>(i + 1)).id != phase)
                        continue;
                    ++completed;
                    outcomes.Add(results[i].encounterOutcome);
                    passed += results[i].verdict == Verdict::Passed;
                    failed += results[i].verdict == Verdict::Failed;
                    inconclusive += results[i].verdict == Verdict::Inconclusive;
                    damage += results[i].damage;
                    incoming += results[i].incoming;
                    blocked += results[i].blocked;
                }
                output << Catalog.Find(phase)->name << ": " << completed << "/" << SuitePlan().CaseRuns(phase);
                if (Catalog.Find(phase)->Expectations<mod::combat_tests::testing::OutcomeAssessment>())
                    output << "; victories " << outcomes.victories << "; defeats " << outcomes.defeats
                           << "; time limits " << outcomes.timeLimits << "; cancelled " << outcomes.cancelled
                           << "; invalid " << outcomes.invalid;
                else
                    output << "; passed " << passed << "; failed " << failed << "; inconclusive " << inconclusive;
                if (completed)
                    output << "; mean sampled health loss " << damage / static_cast<float>(completed)
                           << "; incoming/blocked hit events " << incoming << '/' << blocked;
                output << '\n';
            }
            std::uint32_t power = 0, dodge = 0, handoff = 0, reaction = 0;
            for (const auto& row : results) {
                power += row.powerHits;
                dodge += row.dodges;
                handoff += row.attackHandoffs;
                reaction += row.reactionWaits;
            }
            output << "\nEVENT COVERAGE (zero means not exercised, not passed)\nPower-attack hit events: " << power
                   << "\nDodge graph events: " << dodge << "\nAccepted external attack requests: " << handoff
                   << "\nBlock-reaction release deferrals: " << reaction
                   << "\nScope: melee, ranged, caster and private ally/two-attacker fixtures; no forced scenes or "
                      "save/load phases."
                   << "\nMelee baseline phases require guard/block coverage. Tactical phases assess their own threat, "
                      "approach "
                      "and repositioning coverage. All require clean restoration."
                   << "\nFunctional coverage and combat effectiveness are assessed separately.\n";
        }
        if (defenderBatch &&
            (suiteExpandVariations && (suiteSelection & mod::combat_tests::testing::CaseMask(static_cast<std::uint32_t>(
                                                            mod::combat_tests::suite::Fixture::AllyPressure))))) {
            output << "\nCHALLENGE COMPARISON\nOrder alternates on/off then off/on. "
                      "Positive off-minus-on differences favor Challenge.\n";
            std::uint32_t matched = 0, pending = 0, excluded = 0;
            float damageDifference = 0, exposureDifference = 0;
            for (std::uint32_t repeat = 1;
                 repeat <=
                 SuitePlan().CaseRepeats(static_cast<std::uint32_t>(mod::combat_tests::suite::Fixture::AllyPressure));
                 ++repeat) {
                const RunResult* on = nullptr;
                const RunResult* off = nullptr;
                for (std::size_t i = 0; i < results.size(); ++i) {
                    const auto scheduled = SuitePlan().At(static_cast<std::uint32_t>(i + 1));
                    if (scheduled.parameterized &&
                        scheduled.id == static_cast<std::uint32_t>(mod::combat_tests::suite::Fixture::AllyPressure) &&
                        scheduled.repetition == repeat)
                        (results[i].challengeEnabled ? on : off) = &results[i];
                }
                output << "Pair " << repeat << ": ";
                if (!on || !off) {
                    ++pending;
                    output << "incomplete\n";
                } else if (!on->comparisonEligible || !off->comparisonEligible ||
                           !mod::combat_tests::comparison::Matched(on->setup, off->setup)) {
                    ++excluded;
                    output << "excluded: invalid encounter or mismatched prepared setup\n";
                } else {
                    ++matched;
                    damageDifference += off->allyDamage - on->allyDamage;
                    exposureDifference += off->allyExposure - on->allyExposure;
                    output << "matched; ally loss on/off " << on->allyDamage << '/' << off->allyDamage
                           << "; ally exposure on/off " << on->allyExposure << '/' << off->allyExposure
                           << " s; intervention on/off " << on->firstIntervention << '/' << off->firstIntervention
                           << " s (-1 none); ally defeated on/off " << on->allyDefeated << '/' << off->allyDefeated
                           << "; duration on/off " << on->elapsed << '/' << off->elapsed << " s\n";
                }
            }
            output << "Matched pairs " << matched << "; excluded " << excluded << "; incomplete " << pending;
            if (matched)
                output << "; mean off-minus-on ally loss " << damageDifference / static_cast<float>(matched)
                       << "; mean off-minus-on ally exposure " << exposureDifference / static_cast<float>(matched)
                       << " s";
            output << "\nAI choices and external effects remain variable. Pair exclusions and early defeats must be "
                      "reviewed alongside the differences.\n";
        }
        output << "\nCoverage limits\n"
                  "Directed evasion requires the verified UND/TK backend, a clear route and a stamina reserve. "
                  "Accepted requests are not completion; absent requests leave that behavior untested.\n"
                  "Projectile gaps are bounded flight observations, not predictions of future shots. "
                  "Persistent ground hazards and concurrent native control of multiple defenders are not covered.\n";
        output << seriesReport;
        if (!output)
            SKSE::log::error("Combat tests: report could not be written to {}", path.string());
        std::snprintf(snapshot.reportPath, sizeof(snapshot.reportPath), "%s", path.string().c_str());
    }

    void WriteSample(RE::Actor* subject, RE::Actor* enemy, const char* phase, const char* behavior = "") {
        if (!csv)
            return;
        std::int32_t castState = -1, castable = -1, cannotCast = -1;
        RE::FormID currentSpell = 0, leftSpell = 0;
        std::int32_t leftCastState = -1;
        if (NaturalEncounter() && enemy && enemy->Is3DLoaded()) {
            if (auto* caster = enemy->GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand)) {
                castState = static_cast<std::int32_t>(caster->state.get());
                currentSpell = caster->currentSpell ? caster->currentSpell->GetFormID() : 0;
            }
            if (auto* caster = enemy->GetMagicCaster(RE::MagicSystem::CastingSource::kLeftHand)) {
                leftCastState = static_cast<std::int32_t>(caster->state.get());
                leftSpell = caster->currentSpell ? caster->currentSpell->GetFormID() : 0;
            }
        }
        if (!NaturalEncounter() && enemy && enemy->Is3DLoaded() && OpponentLoadout() >= 3) {
            auto* caster = enemy->GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand);
            auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(FixtureItem(OpponentLoadout()));
            if (caster && spell) {
                castState = static_cast<std::int32_t>(caster->state.get());
                currentSpell = caster->currentSpell ? caster->currentSpell->GetFormID() : 0;
                auto reason = RE::MagicSystem::CannotCastReason::kOK;
                float strength = 1;
                castable = caster->CheckCast(spell, false, &strength, &reason, false);
                cannotCast = static_cast<std::int32_t>(reason);
            }
        }
        csv << currentRun << ',';
        const auto* weapon = enemy->GetEquippedObject(false);
        char weaponID[9];
        std::snprintf(weaponID, sizeof(weaponID), "%08X", weapon ? weapon->GetFormID() : 0);
        DefenderObservation observation;
        const bool observed = host.observe && host.observe(subject->GetFormID(), &observation);
        mod::combat_tests::telemetry::Sample(subject, snapshot.elapsed, phase, observation);
        if (std::string_view(phase) == "running" || std::string_view(phase) == "stopping") {
            const auto dodge = mod::combat_tests::telemetry::Read();
            metricTracker.Observe({snapshot.elapsed, Value(subject, RE::ActorValue::kStamina),
                                   Maximum(subject, RE::ActorValue::kStamina), Value(subject, RE::ActorValue::kHealth),
                                   (dodge.readable & 1) && (dodge.flags & 1), subject->IsAttacking(),
                                   observation.recovering != 0, hitCounts.incoming, hitCounts.outgoing, fixtureSteps,
                                   (dodge.readable & 1) != 0,
                                   observed && observation.available && observation.requested && observation.age >= 0 &&
                                       observation.age <= .3f,
                                   snapshot.tuning.recoveryEnter, snapshot.tuning.recoveryExit});
            snapshot.metrics = metricTracker.Read();
        }
        const auto* left = subject->GetEquippedObject(true);
        const auto* shield = left ? left->As<RE::TESObjectARMO>() : nullptr;
        csv << snapshot.elapsed << ',' << Value(subject, RE::ActorValue::kHealth) << ','
            << Value(subject, RE::ActorValue::kStamina) << ',' << subject->IsBlocking() << ','
            << (subject->AsActorState()->GetAttackState() == RE::ATTACK_STATE_ENUM::kBash) << ','
            << enemy->IsAttacking() << ',' << Value(enemy, RE::ActorValue::kHealth) << ',' << phase << ',' << phaseTime
            << ',' << weaponID << ',' << subject->IsAttacking() << ','
            << static_cast<std::uint32_t>(subject->AsActorState()->GetAttackState()) << ','
            << static_cast<std::uint32_t>(enemy->AsActorState()->GetAttackState()) << ','
            << subject->AsActorState()->actorState2.staggered << ','
            << static_cast<std::uint32_t>(subject->AsActorState()->actorState2.recoil) << ','
            << (shield && shield->IsShield()) << ',' << observation.available << ',' << observation.requested << ','
            << observation.owned << ',' << observation.yielded << ',' << observation.stance << ',' << std::hex
            << observation.style << ',' << observation.liveStyle << ',' << observation.threat << std::dec << ','
            << observation.distance << ',' << observation.heading << ',' << observation.targeting << ','
            << observation.incoming << ',' << observation.committed << ',' << observation.guardRemaining << ','
            << observation.recovering << ',' << observation.counter << ',' << observation.maximumStamina << ','
            << observation.age << ',' << hitCounts.incoming << ',' << hitCounts.blocked << ',' << hitCounts.power << ','
            << hitCounts.bash << ',' << hitCounts.outgoing << ",\"";
        for (const char ch : std::string_view(observation.reason)) {
            if (ch == '"')
                csv << '"';
            csv << ch;
        }
        csv << "\"," << observation.actionAvailable << ',' << observation.actionMode << ',' << observation.guardPhase
            << ',' << observation.actionFault << ',' << observation.actionRequests << ',' << observation.attackRequests
            << ',' << observation.attacksRejected << ',' << observation.engineRejected << ','
            << observation.guardRequests << ',' << observation.guardAccepted << ',' << observation.guardObserved << ','
            << observation.releaseRequests << ',' << observation.releaseAccepted << ',' << observation.releaseObserved
            << ',' << observation.actionTimeouts << ',' << std::hex << observation.lastAction << std::dec << ",\"";
        for (const char ch : std::string_view(observation.actionStatus)) {
            if (ch == '"')
                csv << '"';
            csv << ch;
        }
        csv << "\"," << observation.guardRejected << ',' << observation.dispatch << ',' << observation.processResult
            << ',' << observation.processReturned << ',' << observation.processFlags << ','
            << observation.validationMask << ',' << observation.blockRequests << ',' << observation.bashRequests << ','
            << observation.unknownLeftRequests << ",\"";
        for (const char ch : std::string_view(observation.guardEvent)) {
            if (ch == '"')
                csv << '"';
            csv << ch;
        }
        csv << "\"," << observation.guardSuspended << ',' << observation.blockReadable << ',' << observation.wantsBlock
            << ',' << observation.startRecovering << ',' << observation.startStaminaFraction << ','
            << observation.releaseRejected << ',' << snapshot.encounter << ',' << snapshot.injuryWaves << ','
            << snapshot.healingEffects << ',' << snapshot.selfHealingEffects << ',' << snapshot.playerHealthGain << ','
            << snapshot.injuredSeconds << ',' << Value(RE::PlayerCharacter::GetSingleton(), RE::ActorValue::kHealth)
            << ',' << Value(subject, RE::ActorValue::kMagicka) << ','
            << subject->GetPosition().GetDistance(RE::PlayerCharacter::GetSingleton()->GetPosition()) << ",\"";
        for (const char ch : std::string_view(behavior)) {
            if (ch == '"')
                csv << '"';
            csv << ch;
        }
        const auto interrupts = interference::Read();
        const auto dodges = interference::und::Read();
        const auto dodgeState = mod::combat_tests::telemetry::Read();
        const auto externalAttack = interference::external_attacks::Read();
        csv << "\"," << snapshot.bypassInterruptions << ',' << interrupts.armed << ',' << interrupts.blockStops << ','
            << interrupts.bashStops << ',' << interrupts.suppressed << ',' << snapshot.bypassDodging << ','
            << dodges.armed << ',' << dodges.checks << ',' << dodges.exclusions << ',' << dodges.attempts << ','
            << dodges.denied << ',' << dodgeState.readable << ',' << dodgeState.flags << ',' << dodgeState.attached
            << ',' << dodgeState.events << ',' << dodgeState.dodgeEvents << ',' << dodgeState.guardEvents << ','
            << dodgeState.overwritten << ',' << dodgeState.dodgingSeconds << ",\"";
        for (const char ch : std::string_view(dodgeState.lastEvent)) {
            if (ch == '"')
                csv << '"';
            csv << ch;
        }
        csv << "\"," << observation.dodgeDeferrals << ',' << observation.dodgeInterruptions << ','
            << observation.dodgeResumes << ',' << dodgeState.requestObserver << ',' << dodgeState.notifications << ','
            << dodgeState.notificationCompleted << ',' << dodgeState.notificationFalse << ','
            << dodgeState.notificationSuppressed << ',' << observation.attackDeferrals << ','
            << observation.attackInterruptions << ',' << observation.attackResumes << ',' << externalAttack.proposals
            << ',' << externalAttack.accepted << ',' << externalAttack.rejected << ',' << externalAttack.cleared << ','
            << externalAttack.superseded << ',' << externalAttack.endedActive << ',' << externalAttack.active << ','
            << observation.releaseDeferrals << ',' << observation.releaseWait << ',' << observation.blockHitReadable
            << ',' << observation.blockHit << ',' << observation.graphWantReadable << ',' << observation.graphWantBlock
            << ',' << observation.releaseWaitPeak << ',' << suiteActive << ','
            << (suiteActive ? SuitePlan().At(currentRun).id + 1 : 0) << ','
            << (suiteActive ? SuitePlan().At(currentRun).repetition : currentRun) << ',' << fixtureSteps << ','
            << observation.recoveredInterruptTimeouts << ',' << externalAttack.permaProposals << ','
            << externalAttack.permaAccepted << ',' << externalAttack.permaRejected << ',' << snapshot.experiments.flags
            << ',' << observation.nativeBashRequests << ',' << observation.nativeBashAccepted << ','
            << observation.nativeBashObserved << ',' << observation.nativeBashCompleted << ','
            << observation.nativeBashRejected << ',' << observation.nativeBashTimeouts << ',' << observation.bashPhase
            << ',' << hitCounts.outgoingBash << ',' << snapshot.metrics.lowStaminaSeconds << ','
            << snapshot.metrics.zeroStaminaSeconds << ',' << snapshot.metrics.dodgeEpisodes << ','
            << snapshot.metrics.dodgeNetLoss << ',' << snapshot.metrics.recoveryEntries << ','
            << snapshot.metrics.recoveryExits << ',' << snapshot.metrics.recoverySeconds << ','
            << subject->AsActorState()->IsBleedingOut() << ',' << subject->IsDead() << ',' << subject->IsDisabled()
            << ',' << (subject->GetCurrentScene() != nullptr) << ',' << dodges.reserveDenied << ','
            << dodges.cooldownDenied << ',' << dodges.snapshotDenied << ',' << observation.powerBashRequests << ','
            << observation.powerBashAccepted << ',' << observation.powerBashObserved << ','
            << observation.powerBashCompleted << ',' << observation.powerBashRejected << ','
            << observation.powerBashTimeouts << ',' << externalAttack.animationClears << ','
            << externalAttack.recoveryClears << ',' << externalAttack.rejectionClears << ',' << externalAttack.handoffs
            << ',' << externalAttack.finishing << ',' << observation.handoffAttackRejections << ','
            << observation.nativeBashInterrupted << ',' << observation.powerBashInterrupted << ','
            << observation.bashGraphStarts << ',' << observation.bashGraphEnds << ',' << observation.bashNativeStarts
            << ',' << observation.rightAttackRequests << ',' << observation.rightAttackAccepted << ','
            << observation.rightAttackObserved << ',' << observation.rightAttackCompleted << ','
            << observation.rightAttackInterrupted << ',' << observation.rightAttackRejected << ','
            << observation.rightAttackTimeouts << ',' << observation.rightAttackPhase << ','
            << observation.attackGraphStarts << ',' << observation.attackGraphEnds << ','
            << observation.attackNativeStarts << ',' << observation.attackRecoveryCompletions << ','
            << observation.intent << ',' << observation.motion << ',' << observation.threatKind << ','
            << observation.threatCount << ',' << observation.moving << ',' << observation.movementRequests << ','
            << observation.movementFailures << ',' << observation.precisionAvailable << ','
            << observation.precisionCallbacks << ',' << observation.weaponReach << ',' << observation.precisionIncoming
            << ',' << observation.precisionOutgoing << ',' << tacticalMetrics.moved << ',' << tacticalMetrics.allyHits
            << ',' << subject->GetPositionX() << ',' << subject->GetPositionY() << ',' << subject->GetPositionZ() << ','
            << enemy->GetPositionX() << ',' << enemy->GetPositionY() << ',' << enemy->GetPositionZ() << ','
            << observation.destinationX << ',' << observation.destinationY << ',' << observation.destinationZ << ','
            << observation.movementProgress << ',' << observation.movementWait << ',' << observation.movementStalls
            << ',' << observation.movementDetours << ',' << observation.movementStatus << ','
            << observation.guardPreparing << ',' << tacticalMetrics.inactivity << ',' << tacticalMetrics.friendlyHits
            << ',' << tacticalMetrics.crossfire << ',' << tacticalMetrics.casts << ',' << castState << ','
            << currentSpell << ',' << castable << ',' << cannotCast << ',' << Value(enemy, RE::ActorValue::kMagicka)
            << ',' << observation.guardLocomotionSpeed << ',' << observation.guardLocomotionReady << ','
            << observation.threatVisible << ',' << observation.threatAttacking << ',' << observation.threatPreparing
            << ',' << snapshot.tuning.revision << ',' << snapshot.tuning.name;
        const auto precision = csv.precision(std::numeric_limits<float>::max_digits10);
        for (const auto& field : TuningFields)
            csv << ',' << snapshot.tuning.*field.member;
        csv << ',' << snapshot.tuning.challengeEnabled;
        csv.precision(precision);
        const auto* package = subject->GetCurrentPackage();
        const auto* combat = subject->GetActorRuntimeData().combatController;
        csv << ',' << fixtureSpellsRemoved << ',' << fixtureSpellPasses << ',' << unexpectedFixtureSpells << ','
            << leftCastState << ',' << leftSpell << ',' << (NaturalEncounter() ? natural::Level() : 0) << ','
            << plan.enemyBase << ',' << (package ? package->GetFormID() : 0) << ','
            << scene_control::Requested(subject->GetFormID()) << ','
            << (subject->GetActorRuntimeData().currentCombatTarget.get()
                    ? subject->GetActorRuntimeData().currentCombatTarget.get()->GetFormID()
                    : 0)
            << ','
            << (enemy->GetActorRuntimeData().currentCombatTarget.get()
                    ? enemy->GetActorRuntimeData().currentCombatTarget.get()->GetFormID()
                    : 0)
            << ',' << observer_control::Rejections() << ',' << subject->IsAIEnabled() << ',' << enemy->IsAIEnabled()
            << ',' << subject->IsInCombat() << ',' << enemy->IsInCombat() << ','
            << (combat ? static_cast<int>(combat->ignoringCombat) : -1) << ','
            << (combat ? static_cast<int>(combat->inactive) : -1) << ','
            << (combat ? static_cast<int>(combat->stoppedCombat) : -1) << ',' << dodges.recoveryDenied << ','
            << tacticalMetrics.allyInitialHealth << ',' << tacticalMetrics.allyHealth << ','
            << tacticalMetrics.allyDamage << ',' << tacticalMetrics.allyExposure << ','
            << tacticalMetrics.firstIntervention << ',' << allyDefeated << ',' << observer_control::FriendlyRejections()
            << ',' << subject->IsHostileToActor(enemy) << ',' << enemy->IsHostileToActor(subject) << ','
            << enemy->IsGhost() << ',' << observation.tauntAvailable << ',' << observation.tauntPhase << ','
            << observation.tauntEnemy << ',' << observation.tauntRequests << ',' << observation.tauntObserved << ','
            << observation.tauntUnobserved << ',' << observation.tauntReleased << ',' << observation.tauntCancelled
            << ',' << observation.tauntTargetRejections << ',' << observation.tauntEffectPresent << ','
            << observation.tauntRemaining << ',' << observation.tauntCooldown << ','
            << observation.precisionPreCallbacks << ',' << observation.precisionPreOutgoing << ','
            << observation.precisionLastCandidate << ',' << observation.precisionActorActive << ','
            << observation.precisionThreatActive << ',' << observation.precisionThreatControllerHittable << ','
            << tacticalMetrics.tauntHitEvents << ',' << mod::combat_tests::fixture_protection::Protected(plan.enemy)
            << ',' << mod::combat_tests::fixture_protection::Read().blocked << ','
            << mod::combat_tests::fixture_protection::Read().damage << ','
            << mod::combat_tests::fixture_protection::Read().publicWrites << ','
            << mod::combat_tests::fixture_protection::Read().combatWrites << ','
            << mod::combat_tests::fixture_protection::Read().otherWrites << ',' << engagementMetrics.firstReach << ','
            << engagementMetrics.firstContact << ',' << engagementMetrics.outsideReach << ','
            << engagementMetrics.closingDamage << ',' << engagementMetrics.castingPressure << ','
            << engagementMetrics.aimLineExposure << ',' << engagementMetrics.aimLineExits << ','
            << engagementMetrics.targetSwitches << ',' << mod::combat_tests::observer_position::CameraFree() << ',';
        const auto* auxiliary = Actor(plan.auxiliary);
        const auto auxiliaryTarget = auxiliary ? auxiliary->GetActorRuntimeData().currentCombatTarget.get() : nullptr;
        csv << (auxiliaryTarget ? auxiliaryTarget->GetFormID() : 0) << ',' << observation.casterBashRequests << ','
            << observation.casterBashCompleted << ',' << fixtureDriftSamples;
        for (auto* fixture : {enemy, Actor(plan.auxiliary)})
            for (const auto value : ResourceStats(fixture))
                csv << ',' << value;
        csv << ',' << mod::combat_tests::observer_position::Positioned() << ','
            << enemy->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth) << ','
            << (auxiliary ? auxiliary->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth)
                          : 0)
            << ',' << observation.flightReadable << ',' << observation.projectileGap << ','
            << observation.incomingProjectiles << ',' << observation.projectilesExamined << ','
            << observation.evadeAvailable << ',' << observation.evadePhase << ',' << observation.evadeRequests << ','
            << observation.evadeAccepted << ',' << observation.evadeObserved << ',' << observation.evadeCompleted << ','
            << observation.evadeRejected << ',' << observation.evadeTimeouts << ',' << observation.evadeDisplacement
            << ',' << startupCombat.allyReady << ',' << startupCombat.opponentHostileToAlly << ','
            << startupCombat.opponentTarget << ',' << startupCombat.auxiliary << ',' << snapshot.testPhase.index << ','
            << snapshot.testPhase.elapsed << ',' << snapshot.testPhase.completed << ",\"";
        for (const auto& entry : participantActors.participants.Entries()) {
            auto* actor = Actor(entry.id);
            const auto target = actor ? actor->GetActorRuntimeData().currentCombatTarget.get() : nullptr;
            csv << entry.spec.key << ':' << entry.id << ':' << (target ? target->GetFormID() : 0) << ':'
                << entry.incoming << ':' << entry.outgoing << ':' << entry.healthLost << ';';
        }
        csv << "\"\n";
    }

    bool Dynamic(const RE::TESForm* form) {
        return form && (form->GetFormID() & 0xFF000000) == 0xFF000000;
    }

    bool OwnedEnemy(RE::Actor* actor) {
        if (suiteActive)
            return participantActors.Owns(actor);
        if (mod::test_rules::NaturalEncounter(plan.encounter))
            return natural::Owns(actor, plan.enemyBase, plan.generation);
        const auto* base = actor ? actor->GetActorBase() : nullptr;
        const bool ally = actor && actor->GetFormID() == plan.auxiliary &&
                          static_cast<mod::combat_tests::suite::Fixture>(plan.fixture) ==
                              mod::combat_tests::suite::Fixture::AllyPressure;
        return base && Dynamic(actor) && mod::combat_tests::fixture_bases::Owns(base) &&
               (ally ? mod::combat_tests::fixture_bases::IsAlly(base) : base->GetFormID() == plan.enemyBase);
    }

    std::string SpellIsolationFailure(const mod::combat_tests::fixture_spells::Result& result) {
        if (!result.owned)
            return "Fixture spell isolation ownership check failed.";
        if (!result.bounded)
            return "Fixture spell list exceeded its safe inspection bound.";
        if (result.remaining.empty())
            return {};
        const auto& spell = result.remaining.front();
        char message[256];
        std::snprintf(message, sizeof(message), "Unexpected fixture spell %08X %.80s remains; run invalidated.",
                      spell.id, spell.name.c_str());
        return message;
    }

    std::string InspectFixtureSpellbooks() {
        if (suiteActive)
            return participantActors.Validate(snapshot.phase == Phase::Running);
        if (NaturalEncounter())
            return {};
        for (const auto id : {plan.enemy, plan.auxiliary}) {
            if (!id)
                continue;
            auto* actor = Actor(id);
            auto* expected = id == plan.enemy && OpponentLoadout() >= 3
                                 ? RE::TESForm::LookupByID<RE::SpellItem>(FixtureItem(OpponentLoadout()))
                                 : nullptr;
            const auto inspection =
                mod::combat_tests::fixture_spells::Inspect(actor, actor ? actor->GetActorBase() : nullptr, expected);
            if (const auto failure = SpellIsolationFailure(inspection); !failure.empty()) {
                unexpectedFixtureSpells = static_cast<std::uint32_t>(inspection.remaining.size());
                return failure;
            }
        }
        return {};
    }

    mod::combat_tests::suite::Fixture Fixture() {
        return static_cast<mod::combat_tests::suite::Fixture>(plan.fixture);
    }

    bool AllyEncounter() {
        return suiteActive ? participantActors.participants.First(mod::combat_tests::testing::ActorRole::Ally) != 0
                           : Fixture() == mod::combat_tests::suite::Fixture::AllyPressure;
    }

    std::string FixtureResourceFailure() {
        if (suiteActive) {
            for (const auto& entry : participantActors.participants.Entries()) {
                if (entry.spec.preserveLoadout)
                    continue;
                auto* actor = Actor(entry.id);
                if (!OwnedEnemy(actor) ||
                    !mod::combat_tests::resources::AtTarget(Maximum(actor, RE::ActorValue::kHealth),
                                                            entry.spec.health) ||
                    !mod::combat_tests::resources::AtTarget(Maximum(actor, RE::ActorValue::kMagicka), 2000))
                    return entry.spec.key + ": resource maxima differ from the declared setup.";
            }
            return {};
        }
        for (auto* fixture : {Actor(plan.enemy), Actor(plan.auxiliary)}) {
            if (!fixture)
                continue;
            const bool ally = fixture->GetFormID() == plan.auxiliary && AllyEncounter();
            const float expectedHealth = ally ? seriesAllyHealth : 2000;
            const auto health = Maximum(fixture, RE::ActorValue::kHealth);
            const auto magicka = Maximum(fixture, RE::ActorValue::kMagicka);
            if (!mod::combat_tests::resources::AtTarget(health, expectedHealth) ||
                !mod::combat_tests::resources::AtTarget(magicka, 2000)) {
                std::ostringstream failure;
                failure << "Fixture " << std::hex << fixture->GetFormID() << std::dec << " maxima health/magicka "
                        << health << '/' << magicka << "; expected " << expectedHealth
                        << "/2000. Equipment or setup effects changed after normalization.";
                return failure.str();
            }
        }
        return {};
    }

    void CapturePreparedSetup(RE::Actor* subject, RE::Actor* enemy) {
        const auto* right = subject->GetEquippedObject(false);
        const auto* left = subject->GetEquippedObject(true);
        auto* extra = Actor(plan.auxiliary);
        comparisonSetup = {right ? right->GetFormID() : 0,
                           left ? left->GetFormID() : 0,
                           enemy->GetActorBase()->GetFormID(),
                           Value(subject, RE::ActorValue::kHealth),
                           Value(subject, RE::ActorValue::kStamina),
                           Value(subject, RE::ActorValue::kMagicka),
                           extra ? Value(extra, RE::ActorValue::kHealth) : 0,
                           Value(enemy, RE::ActorValue::kHealth)};
        comparisonSetup.subjectStats = ResourceStats(subject);
        comparisonSetup.enemyStats = ResourceStats(enemy);
        comparisonSetup.allyStats = ResourceStats(extra);
        comparisonSetup.subjectEquipment = EquipmentFingerprint(subject);
        comparisonSetup.enemyEquipment = EquipmentFingerprint(enemy);
        comparisonSetup.allyEquipment = EquipmentFingerprint(extra);
    }

    std::uint32_t OpponentLoadout() {
        if (suiteActive) {
            if (const auto* entry = participantActors.participants.Find(plan.enemy))
                return entry->spec.archetype == 6 ? 2 : entry->spec.archetype;
            return UINT32_MAX;
        }
        return static_cast<std::uint32_t>(mod::combat_tests::fixtures::Archetype::Sword);
    }

    bool OpponentEquipped(RE::Actor* enemy) {
        if (NaturalEncounter())
            return enemy && enemy->Is3DLoaded();
        const auto loadout = OpponentLoadout();

        const auto* item = enemy ? enemy->GetEquippedObject(false) : nullptr;
        return loadout < FixtureItems.size() && item && item->GetFormID() == FixtureItems[loadout];
    }

    bool AllyReady() {
        if (suiteActive) {
            for (const auto& entry : participantActors.participants.Entries())
                if (entry.spec.role == mod::combat_tests::testing::ActorRole::Ally) {
                    auto* ally = Actor(entry.id);
                    if (!OwnedEnemy(ally) || !ally->Is3DLoaded() || !ally->IsAIEnabled() || !ally->IsPlayerTeammate() ||
                        ally->IsDead() || Value(ally, RE::ActorValue::kHealth) <= 0)
                        return false;
                }
            return true;
        }
        if (!AllyEncounter())
            return true;
        auto* ally = Actor(plan.auxiliary);
        auto* weapon = ally ? ally->GetEquippedObject(false) : nullptr;
        return OwnedEnemy(ally) && ally->Is3DLoaded() && ally->IsAIEnabled() && ally->IsPlayerTeammate() &&
               !ally->GetActorBase()->IsInvulnerable() && !ally->IsDead() && Value(ally, RE::ActorValue::kHealth) > 0 &&
               weapon && weapon->GetFormID() == FixtureItems[2];
    }

    mod::combat_tests::startup_combat::State ParticipantEngagement() {
        using State = mod::combat_tests::startup_combat::State;
        const auto& registry = participantActors.participants;
        bool waiting = false;
        std::vector<std::uint32_t> ids{plan.subject};
        for (const auto& entry : registry.Entries())
            ids.push_back(entry.id);
        for (const auto id : ids) {
            auto* actor = Actor(id);
            if (!actor || !actor->IsInCombat()) {
                waiting = true;
                continue;
            }
            const auto target = actor->GetActorRuntimeData().currentCombatTarget.get();
            if (!target) {
                waiting = true;
                continue;
            }
            const auto targetID = NaturalEncounter()
                                      ? natural::Participant(target->GetFormID(), plan.subject, plan.enemy)
                                      : target->GetFormID();
            if (!registry.Opposed(id, targetID, plan.subject))
                return State::UnsafeTarget;
            if (!actor->IsHostileToActor(Actor(targetID)))
                waiting = true;
        }
        return waiting ? State::Waiting : State::Ready;
    }

    bool ReadSubject(Subject& state) {
        return host.inspect && host.inspect(plan.subject ? plan.subject : requestedActor, &state) && state.managed;
    }

    mod::combat_tests::testing::Result ExecuteTestCommand(const mod::combat_tests::testing::Command& command) {
        namespace harness = mod::combat_tests::testing;
        if (!active || !command.generation || command.generation != ticket)
            return harness::Result::Failed("Test command belongs to an expired case generation.");
        auto* subject = Actor(plan.subject ? plan.subject : requestedActor);
        switch (command.action) {
            case harness::Action::PrepareArena:
                return harness::CheckArena(subject, Marker(0x811));
            case harness::Action::PreparePlayer:
                if (!mod::combat_tests::observer_position::Begin())
                    return harness::Result::Failed("Observer positioning unavailable; fixture setup was not started.");
                if (!testSession.context.resources.Own(harness::Scope::Suite, "observer", 0x14, [] {
                        mod::combat_tests::observer_position::End();
                        return mod::combat_tests::observer_position::Pending()
                                   ? harness::Result::Pending("Waiting for observer restoration.")
                                   : harness::Result{};
                    }))
                    return harness::Result::Failed("Observer cleanup registration failed.");
                return {};
            case harness::Action::Guard:
            case harness::Action::Release:
            case harness::Action::Bash:
            case harness::Action::PowerBash:
            case harness::Action::RightAttack: {
                const auto action = command.action == harness::Action::Guard         ? PrimitiveAction::Guard
                                    : command.action == harness::Action::Release     ? PrimitiveAction::Release
                                    : command.action == harness::Action::PowerBash   ? PrimitiveAction::PowerBash
                                    : command.action == harness::Action::RightAttack ? PrimitiveAction::RightAttack
                                                                                     : PrimitiveAction::Bash;
                if (!host.requestDirect || !host.requestDirect(plan.subject, plan.generation, action))
                    testSession.context.Finish(harness::Verdict::Inconclusive,
                                               "Host rejected the explicit request before queueing; no retry.");
                return {};
            }
            case harness::Action::InjurePlayer: {
                auto* player = RE::PlayerCharacter::GetSingleton();
                const float maximum = player ? Maximum(player, RE::ActorValue::kHealth) : 0;
                const float health = player ? Value(player, RE::ActorValue::kHealth) : 0;
                const float target = mod::combat_tests::healer::InjuryTarget(maximum, command.value);
                if (!player || player->IsDead() || player->GetCurrentScene() || target <= 0 || !std::isfinite(health))
                    return harness::Result::Failed("Player state or health changed; Party pressure invalidated.");
                if (snapshot.injuryWaves)
                    snapshot.playerHealthGain += std::max(health - previousPlayerHealth, 0.0f);
                if (health > target) {
                    plan.playerChanged = 1;
                    RestoreValue(player, RE::ActorValue::kHealth, target);
                }
                previousPlayerHealth = Value(player, RE::ActorValue::kHealth);
                ++snapshot.injuryWaves;
                ++fixtureSteps;
                return {};
            }
            case harness::Action::PrepareSubject: {
                Subject state;
                if (!subject || !subject->Is3DLoaded() || !ReadSubject(state) || !state.allowed ||
                    subject->GetCurrentScene() || Value(subject, RE::ActorValue::kWaitingForPlayer) != 0)
                    return harness::Result::Failed("Prepared subject no longer has follower/quest permission.");
                return {};
            }
            case harness::Action::StaminaFraction: {
                if (!subject || !std::isfinite(command.value) || command.value < 0 || command.value > 1)
                    return harness::Result::Failed("Invalid subject stamina request.");
                const float maximum = Maximum(subject, RE::ActorValue::kStamina);
                const float current = Value(subject, RE::ActorValue::kStamina);
                if (!std::isfinite(maximum) || maximum <= 0 || !std::isfinite(current))
                    return harness::Result::Failed("Subject stamina is unreadable.");
                if (current > maximum * command.value)
                    RestoreValue(subject, RE::ActorValue::kStamina, maximum * command.value);
                ++fixtureSteps;
                return {};
            }
            case harness::Action::Reposition: {
                auto* opponent = Actor(plan.enemy);
                if (!subject || !opponent || !std::isfinite(command.value) || std::abs(command.value) > 512)
                    return harness::Result::Failed("Invalid opponent reposition request.");
                auto position = subject->GetPosition();
                position.x = std::clamp(position.x + std::cos(subject->GetAngleZ()) * command.value, -1800.0f, 256.0f);
                position.y = std::clamp(position.y - std::sin(subject->GetAngleZ()) * command.value, -1800.0f, 1800.0f);
                opponent->SetPosition(position, true);
                ++fixtureSteps;
                return {};
            }
            case harness::Action::Spawn: {
                const auto result = participantActors.Spawn(testSession.context, command.actor, Marker(0x812));
                const auto& actors = participantActors.participants;
                plan.enemy = actors.First(harness::ActorRole::Opponent);
                if (const auto* primary = actors.Find(plan.enemy))
                    plan.enemyBase = primary->base;
                plan.auxiliary = actors.First(harness::ActorRole::Ally);
                if (!plan.auxiliary)
                    for (const auto& entry : actors.Entries())
                        if (entry.id != plan.enemy) {
                            plan.auxiliary = entry.id;
                            break;
                        }
                if (const auto* ally = actors.Find(actors.First(harness::ActorRole::Ally)))
                    seriesAllyHealth = ally->spec.health;
                return result;
            }
        }
        return harness::Result::Failed("Unknown test command.");
    }

    bool PrepareCaseActors() {
        const auto result = testSession.current.Prepare();
        if (result.state == mod::combat_tests::testing::State::Failed) {
            Stop(Phase::Failed, result.reason.c_str());
            return false;
        }
        if (result.state == mod::combat_tests::testing::State::Pending) {
            Status(result.reason.empty() ? "Waiting for case preparation." : result.reason.c_str());
            return false;
        }
        const auto* definition = SuiteCase();
        if (!definition || participantActors.participants.Entries().size() != definition->actors.size() ||
            !plan.enemy) {
            Stop(Phase::Failed,
                 "A case factory did not register its declared participant with the execution provider.");
            return false;
        }
        participantActors.SubjectTarget(definition->subjectTarget);
        for (const auto& entry : participantActors.participants.Entries()) {
            const auto target = participantActors.participants.Target(entry, plan.subject);
            if (!participantActors.participants.Opposed(entry.id, target, plan.subject)) {
                Stop(Phase::Failed, "A participant factory declared a missing or same-team combat target.");
                return false;
            }
        }
        phaseTime = 0;
        Status("Preparing encounter; waiting for actor loading and protection checks.");
        return true;
    }

    bool DirectEncounter() {
        return mod::test_rules::DirectEncounter(snapshot.encounter);
    }

    bool HealingEncounter() {
        return mod::test_rules::IsExecution(snapshot.encounter, mod::test_rules::Execution::Healer);
    }

    std::optional<mod::combat_tests::primitive::Kind> PrimitiveKind() {
        const auto* definition = SuiteCase();
        const auto* kind = definition ? definition->Expectations<mod::combat_tests::primitive::Kind>() : nullptr;
        return kind ? std::optional{*kind} : std::nullopt;
    }

    float MaximumHealth(RE::Actor* actor) {
        return Maximum(actor, RE::ActorValue::kHealth);
    }

    void ObserveAlly(RE::Actor* enemy, float elapsed) {
        if (!AllyEncounter())
            return;
        auto* ally = Actor(plan.auxiliary);
        if (!OwnedEnemy(ally) || !ally->Is3DLoaded())
            return;
        const auto target = enemy->GetActorRuntimeData().currentCombatTarget.get();
        if (snapshot.phase == Phase::Running)
            tacticalMetrics.ObserveAlly(snapshot.elapsed, elapsed, Value(ally, RE::ActorValue::kHealth),
                                        target.get() == ally, target && target->GetFormID() == plan.subject);
        allyDefeated = mod::combat_tests::testing::Defeated(ally->IsDead(), ally->AsActorState()->IsBleedingOut(),
                                                            Value(ally, RE::ActorValue::kHealth));
    }

    bool Paused() {
        auto* ui = RE::UI::GetSingleton();
        const auto* main = RE::Main::GetSingleton();
        return !ui || ui->GameIsPaused() || !main || main->freezeTime;
    }

    void Finish(Phase result, const char* detail) {
        bool caseCleanupReady = true;
        if (suiteActive && !plan.subject) {
            const auto restored = testSession.RestoreCase();
            if (restored.state != mod::combat_tests::testing::State::Ready) {
                caseCleanupReady = false;
                result = Phase::Failed;
                detail = "Case cleanup remains unresolved; suite execution stopped.";
            }
        }
        mod::combat_tests::fixture_protection::End();
        observer_control::End();
        scene_control::End(snapshot.subject, ticket);
        snapshot.phase = result;
        snapshot.active = 0;
        active = false;
        Status(detail);
        metricTracker.Finish();
        snapshot.metrics = metricTracker.Read();
        std::ostringstream report;
        if (suiteActive) {
            report << "Participants:\n";
            for (const auto& entry : participantActors.participants.Entries())
                report << "  " << entry.spec.key << " [" << std::hex << entry.id << std::dec << "] "
                       << (entry.spec.role == mod::combat_tests::testing::ActorRole::Ally ? "ally" : "opponent")
                       << "; target " << entry.spec.target << "; incoming/outgoing " << entry.incoming << '/'
                       << entry.outgoing << "; attack starts " << entry.attackStarts << "; health lost "
                       << entry.healthLost << '\n';
        }
        const auto subjectID = snapshot.subject ? snapshot.subject : requestedActor;
        if (const auto* subject = Actor(subjectID))
            report << "Subject: " << std::hex << subjectID << std::dec << ' ' << subject->GetName() << '\n';
        if (NaturalEncounter()) {
            if (result == Phase::Aborted)
                encounterOutcome = natural::Outcome::Cancelled;
            else if (result != Phase::Complete || plan.subject)
                encounterOutcome = natural::Outcome::Invalid;
            if (result == Phase::Complete && !plan.subject)
                Status(natural::Name(encounterOutcome).data());
            report << "Encounter outcome: " << natural::Name(encounterOutcome) << '\n'
                   << natural::Details() << "Natural spellbook, equipment and resources; no forced casting hand.\n";
        }
        if (!HealingEncounter()) {
            report << "Defender tuning: " << snapshot.tuning.name << "; revision " << snapshot.tuning.revision << '\n';
            report << mod::combat_tests::tuning::Serialize(snapshot.tuning);
        }
        if (!NaturalEncounter())
            report << "Fixture spell isolation: " << fixtureSpellsRemoved << " removed entries across "
                   << fixtureSpellPasses << " passes; unexpected remaining " << unexpectedFixtureSpells << '\n';
        if (!NaturalEncounter()) {
            const auto protection = mod::combat_tests::fixture_protection::Read();
            report << "Fixture protection: health-damage filter; invulnerability disabled; suppressed "
                   << protection.blocked << " calls / " << protection.damage << " damage (including preflight).\n";
            report << "Health-write coverage: public " << protection.publicWrites << "; combat "
                   << protection.combatWrites << "; other engine writes " << protection.otherWrites << "; "
                   << mod::combat_tests::fixture_protection::Status() << '\n';
        }
        for (const auto& spell : fixtureSpellChanges)
            report << "  Removed " << std::hex << spell.id << std::dec << ' ' << spell.name << '\n';
        DefenderObservation defense = lastDefense;
        report << "\nHostility at combat start: subject to opponent " << tacticalMetrics.subjectHostile
               << "; opponent to subject " << tacticalMetrics.enemyHostile << ".";
        report << "\nStartup engagement: ally scenario " << startupCombat.allyScenario << "; ally ready "
               << startupCombat.allyReady << "; opponent hostile to ally " << startupCombat.opponentHostileToAlly
               << "; subject/opponent/auxiliary targets " << std::hex << startupCombat.subjectTarget << '/'
               << startupCombat.opponentTarget << '/' << startupCombat.auxiliaryTarget << "; auxiliary "
               << startupCombat.auxiliary << std::dec << '.';
        if (!HealingEncounter())
            report << "\nDefender challenge: available " << defense.tauntAvailable << "; requested/observed/unobserved "
                   << defense.tauntRequests << '/' << defense.tauntObserved << '/' << defense.tauntUnobserved
                   << "; released/cancelled " << defense.tauntReleased << '/' << defense.tauntCancelled
                   << "; target selections denied " << defense.tauntTargetRejections << "; last sampled phase "
                   << defense.tauntPhase << "; spell hit events " << tacticalMetrics.tauntHitEvents
                   << " (excluded from physical hit counts)."
                   << "\nPrecision eligibility: pre-hit observer " << defense.precisionPreCallbacks
                   << "; cumulative eligible outgoing candidates " << defense.precisionPreOutgoing
                   << "; subject/threat active " << defense.precisionActorActive << '/' << defense.precisionThreatActive
                   << "; threat controller hittable " << defense.precisionThreatControllerHittable
                   << " (-1 means unavailable).";

        if (const auto* phase = SuiteCase())
            report << "Suite case: " << phase->name << "; repetition " << SuitePlan().At(currentRun).repetition << "/"
                   << SuitePlan().CaseRepeats(phase->id) << "; fixture steps " << fixtureSteps << "\n";
        if (suiteActive)
            for (const auto& phase : testSession.current.Results())
                report << "Phase: " << phase.name << "; entered " << phase.entered << "; completed " << phase.completed
                       << "; measured " << phase.elapsed << " s\n";
        if (DirectEncounter()) {
            const auto verdict = result != Phase::Complete ? "Failed"
                                 : (testSession.context.completion && testSession.context.completion->verdict ==
                                                                          mod::combat_tests::testing::Verdict::Passed)
                                     ? "Passed"
                                     : "Inconclusive";
            report << detail << "\nDirect primitive: " << mod::test_rules::Scenarios[snapshot.encounter].name
                   << "\nVerdict: " << verdict << "\n"
                   << (testSession.context.completion ? testSession.context.completion->reason.c_str()
                                                      : "Primitive did not complete.")
                   << "\nAutomatic role; defender threat/stamina policy bypassed.";
            if (PrimitiveKind() == mod::combat_tests::primitive::Kind::RightAttack) {
                report << "\nRight attack requested/accepted/observed/completed/interrupted/rejected: "
                       << defense.rightAttackRequests << "/" << defense.rightAttackAccepted << "/"
                       << defense.rightAttackObserved << "/" << defense.rightAttackCompleted << "/"
                       << defense.rightAttackInterrupted << "/" << defense.rightAttackRejected
                       << "\nGraph starts/ends; native starts; recovery completions: " << defense.attackGraphStarts
                       << "/" << defense.attackGraphEnds << "; " << defense.attackNativeStarts << "; "
                       << defense.attackRecoveryCompletions
                       << "\nRight attack phase/timeouts: " << defense.rightAttackPhase << "/"
                       << defense.rightAttackTimeouts << "\n"
                       << defense.rightAttackStatus << "\nResolved event: " << defense.guardEvent;
            } else {
                report << "\nGuard requested/accepted/observed: " << defense.guardRequests << "/"
                       << defense.guardAccepted << "/" << defense.guardObserved
                       << "\nRelease requested/accepted/observed: " << defense.releaseRequests << "/"
                       << defense.releaseAccepted << "/" << defense.releaseObserved
                       << "\nBash requested/accepted/observed/completed/interrupted/rejected: "
                       << defense.nativeBashRequests << "/" << defense.nativeBashAccepted << "/"
                       << defense.nativeBashObserved << "/" << defense.nativeBashCompleted << "/"
                       << defense.nativeBashInterrupted << "/" << defense.nativeBashRejected
                       << "\nGraph starts/ends; native starts: " << defense.bashGraphStarts << "/"
                       << defense.bashGraphEnds << "; " << defense.bashNativeStarts
                       << "\nGuard/bash timeouts: " << defense.actionTimeouts << "/" << defense.nativeBashTimeouts
                       << "\nLast action: " << defense.actionStatus << "\nLast bash: " << defense.bashStatus;
            }
        } else if (HealingEncounter()) {
            report << detail << "\nParty pressure: " << snapshot.elapsed << " / " << snapshot.duration
                   << " active seconds; injury waves " << snapshot.injuryWaves << "; opponent attack starts "
                   << snapshot.attacks << "\nRestorative health-effect applications from healer: player "
                   << snapshot.healingEffects << "; self " << snapshot.selfHealingEffects
                   << "\nSampled player health gain " << snapshot.playerHealthGain
                   << " (includes regeneration); time below 70% health " << snapshot.injuredSeconds << " s";
            if (snapshot.firstHeal >= 0)
                report << "\nFirst observed ally-healing effect at " << snapshot.firstHeal << " s";
            else
                report << "\nHealing result: no ally-healing effect observed; inspect role states and CSV.";
            if (!snapshot.attacks)
                report << "\nPressure result: inconclusive (no opponent attack starts observed).";
            else if (snapshot.healingEffects)
                report << "\nAlly healing observed under melee pressure.";
        } else {
            report << detail << "\nMeasured " << snapshot.elapsed << " / " << snapshot.duration
                   << " active seconds; observed opponent attacks: " << snapshot.attacks
                   << "\nObserved block starts: " << snapshot.blocks << "; bash starts: " << snapshot.bashes
                   << "; health loss sampled: " << snapshot.damage;
            report << "\nEngine hit events received: " << hitCounts.incoming
                   << "; flagged blocked: " << hitCounts.blocked << "; power: " << hitCounts.power
                   << "; bash: " << hitCounts.bash << "; hits delivered to opponent: " << hitCounts.outgoing;
            report << "\nNative defense; validation hook " << defense.actionAvailable << "; action requests "
                   << defense.actionRequests << "; offensive requests " << defense.attackRequests
                   << "; rejected by YLIWF " << defense.attacksRejected << "; validation rejected "
                   << defense.engineRejected << "\nGuard requests/accepted/observed: " << defense.guardRequests << '/'
                   << defense.guardAccepted << '/' << defense.guardObserved << "; releases: " << defense.releaseRequests
                   << '/' << defense.releaseAccepted << '/' << defense.releaseObserved
                   << "; timeouts: " << defense.actionTimeouts
                   << "; verified recovered interruption timeouts: " << defense.recoveredInterruptTimeouts;
            report << "\nCompleted guard rejections: " << defense.guardRejected << "; dispatch outcome "
                   << defense.dispatch << "; process result " << defense.processResult << "; returned "
                   << defense.processReturned << "; flags " << defense.processFlags << "; validation mask "
                   << defense.validationMask;
            report << "\nResolved left actions: block " << defense.blockRequests << "; bash " << defense.bashRequests
                   << "; unknown " << defense.unknownLeftRequests
                   << "; last guard event: " << (defense.guardEvent[0] ? defense.guardEvent : "<unresolved>");
            if (defense.actionStatus[0])
                report << "\nNative defense: " << defense.actionStatus;
            report << "\nGuard diagnostics: suspended " << defense.guardSuspended << "; graph readable "
                   << defense.blockReadable << "; wants block " << defense.wantsBlock << "; last start recovering "
                   << defense.startRecovering << "; stamina fraction " << defense.startStaminaFraction
                   << "; rejected releases " << defense.releaseRejected << "; locomotion speed "
                   << defense.guardLocomotionSpeed << "; locomotion eligible " << defense.guardLocomotionReady;
            report << "\nDodge cooperation: deferred episodes " << defense.dodgeDeferrals << "; interrupted guards "
                   << defense.dodgeInterruptions << "; safe resumes " << defense.dodgeResumes;
            report << "\nExternal attack cooperation: deferred episodes " << defense.attackDeferrals
                   << "; interrupted guards " << defense.attackInterruptions << "; safe resumes "
                   << defense.attackResumes;
            report << "\nBlock reaction release: deferred episodes " << defense.releaseDeferrals << "; wait "
                   << defense.releaseWait << " s; peak wait " << defense.releaseWaitPeak
                   << " s; reaction readable/active " << defense.blockHitReadable << '/' << defense.blockHit
                   << "; graph iWantBlock readable/value " << defense.graphWantReadable << '/'
                   << defense.graphWantBlock;
            if (defense.guardSuspended)
                report << "\nGuard execution incomplete: requests suspended after an unobserved start; remaining "
                          "combat uses "
                          "styles and the selected recovery gate.";
        }
        if (!HealingEncounter()) {
            const auto& measured = snapshot.metrics;
            report << "\nExperiments: dodge budget "
                   << ((snapshot.experiments.flags & DodgeBudget) ? "enabled" : "disabled") << "; native bash "
                   << ((snapshot.experiments.flags & NativeBash) ? "enabled" : "disabled")
                   << "\nDamage per active second: " << (measured.seconds > 0 ? measured.damage / measured.seconds : 0)
                   << "; sampled effective health loss " << measured.damage
                   << "\nStamina below 20%: " << measured.lowStaminaSeconds
                   << " s; at or below 1: " << measured.zeroStaminaSeconds << " s"
                   << "\nRecovery entries/exits: " << measured.recoveryEntries << '/' << measured.recoveryExits
                   << "; total " << measured.recoverySeconds << " s; longest " << measured.longestRecovery << " s"
                   << "\nObserved dodge episodes: " << measured.dodgeEpisodes << "; sampled surrounding net loss "
                   << measured.dodgeNetLoss << "; clean subset " << measured.cleanDodgeEpisodes << " / "
                   << measured.cleanDodgeNetLoss << " stamina; partial episodes " << measured.partialDodgeEpisodes
                   << "; unavailable samples " << measured.dodgeUnavailableSamples
                   << ". Net changes include regeneration; they are not engine-attributed costs."
                   << "\nNative bash requested/accepted/observed/completed/rejected/timeouts: "
                   << defense.nativeBashRequests << '/' << defense.nativeBashAccepted << '/'
                   << defense.nativeBashObserved << '/' << defense.nativeBashCompleted << '/'
                   << defense.nativeBashRejected << '/' << defense.nativeBashTimeouts << "; outgoing bash hit events "
                   << hitCounts.outgoingBash << "\n"
                   << defense.bashStatus
                   << "\nPower bash subset requested/accepted/observed/completed/rejected/timeouts: "
                   << defense.powerBashRequests << '/' << defense.powerBashAccepted << '/' << defense.powerBashObserved
                   << '/' << defense.powerBashCompleted << '/' << defense.powerBashRejected << '/'
                   << defense.powerBashTimeouts;
            report << "\nNew offensive requests rejected during attack handoff: " << defense.handoffAttackRejections;
            report << "\nBash graph starts/ends; native starts: " << defense.bashGraphStarts << '/'
                   << defense.bashGraphEnds << "; " << defense.bashNativeStarts
                   << "\nInterrupted native/power bashes: " << defense.nativeBashInterrupted << '/'
                   << defense.powerBashInterrupted << "; excluded from completed totals.";
        }
        if (!stopReason.empty())
            report << "\nReason: " << stopReason;
        const auto interrupts = interference::Read();
        report << "\nPermaBlock interruption bypass: " << (snapshot.bypassInterruptions ? "enabled" : "disabled")
               << "; observer " << interference::Status(interrupts.support) << "\nPermaBlock requests: blockStop "
               << interrupts.blockStops << "; bashStop " << interrupts.bashStops << "; suppressed "
               << interrupts.suppressed;
        if (snapshot.bypassInterruptions && !interrupts.suppressed)
            report << "\nNo PermaBlock interruption was suppressed; this run does not demonstrate an intervention.";
        interference::End();
        const auto dodges = interference::und::Read();
        report << "\nUND dodge exclusion: " << (snapshot.bypassDodging ? "enabled" : "disabled") << "; "
               << interference::und::Status(dodges.support) << "\nUND checks: " << dodges.checks
               << "; forced exclusions: " << dodges.exclusions << "; eligibility checks: " << dodges.attempts
               << "; denied: " << dodges.denied;
        report << "\nDirected evasion: available " << defense.evadeAvailable
               << "; requests/accepted/observed/completed " << defense.evadeRequests << '/' << defense.evadeAccepted
               << '/' << defense.evadeObserved << '/' << defense.evadeCompleted << "; rejected "
               << defense.evadeRejected << "; timeouts " << defense.evadeTimeouts << "; last displacement "
               << defense.evadeDisplacement << " units.";
        if (!defense.evadeAvailable)
            report << " Backend unavailable, excluded, or disabled after missing evidence; use ordinary defense.";
        else if (!defense.evadeRequests)
            report << " No eligible clear evasion window; directed dodge effectiveness is untested.";
        if (snapshot.bypassDodging && !dodges.exclusions && !dodges.denied)
            report << "\nNo UND decision was overridden; this run does not demonstrate an intervention.";
        report << "\nDodge budget decisions: allowed " << dodges.budgetAllowed << "; reserve denied "
               << dodges.reserveDenied << "; cooldown denied " << dodges.cooldownDenied << "; snapshot unavailable "
               << dodges.snapshotDenied << "; recovery denied " << dodges.recoveryDenied
               << "; estimated maximum dodge cost " << dodges.estimatedCost << "; reserve " << dodges.reserve;
        interference::und::End();
        interference::external_attacks::End();
        report << interference::external_attacks::Report();
        mod::combat_tests::telemetry::End();
        report << mod::combat_tests::telemetry::Report(false);
        if (plan.subject)
            report << "\nRecovery references: subject " << std::hex << plan.subject << "; origin " << plan.origin
                   << "; opponent " << plan.enemy << std::dec;
        if (!HealingEncounter() && !NaturalEncounter() && !snapshot.attacks)
            report << "\nDefense result: inconclusive (no opponent attack starts observed).";
        else if (!HealingEncounter())
            report << "\nBlocking: " << (snapshot.blocks ? "observed" : "not observed")
                   << "; bashing: " << (snapshot.bashes ? "observed" : "not observed");
        for (const auto& [behavior, seconds] : behaviorTimes)
            report << "\n" << behavior << ": " << seconds << " s";
        if (!reportFile.empty())
            report << "\nCSV: " << reportFile;
        std::snprintf(snapshot.report, sizeof(snapshot.report), "%s", report.str().c_str());
        RunResult resultRow;
        resultRow.metrics = snapshot.metrics;
        if (DirectEncounter())
            resultRow.verdict = result != Phase::Complete ? mod::combat_tests::suite::Verdict::Failed
                                : (testSession.context.completion && testSession.context.completion->verdict ==
                                                                         mod::combat_tests::testing::Verdict::Passed)
                                    ? mod::combat_tests::suite::Verdict::Passed
                                    : mod::combat_tests::suite::Verdict::Inconclusive;
        if (suiteActive && !DirectEncounter() && !HealingEncounter() && !NaturalEncounter()) {
            using namespace mod::combat_tests::suite;
            const Evidence evidence{
                result == yliwf::testing::Phase::Complete,
                !plan.subject,
                defense.guardSuspended != 0,
                defense.actionTimeouts + defense.nativeBashTimeouts,
                snapshot.attacks,
                defense.guardObserved,
                hitCounts.blocked,
                defense.blockReadable != 0,
                fixtureSteps == SuiteCase()->Interventions(),
                defense.recoveredInterruptTimeouts,
                (snapshot.experiments.flags & NativeBash) != 0,
                (snapshot.experiments.flags & DodgeBudget) != 0 && !snapshot.bypassDodging,
                defense.nativeBashCompleted,
                dodges.budgetAllowed + dodges.reserveDenied + dodges.cooldownDenied + dodges.recoveryDenied,
                false,
                snapshot.metrics.recoveryExits};
            ParticipantCoverage coverage;
            for (const auto& entry : participantActors.participants.Entries()) {
                if (entry.spec.role != mod::combat_tests::testing::ActorRole::Opponent)
                    continue;
                ++coverage.opponents;
                coverage.attackingOpponents += entry.attackStarts > 0;
            }
            const auto* requirements = SuiteCase()->Expectations<Requirements>();
            const auto assessment =
                requirements ? Evaluate(*requirements, evidence, tacticalMetrics, coverage)
                             : Assessment{Verdict::Failed, "Case assessment requirements are missing.", nullptr};
            resultRow.verdict = assessment.verdict;
            if (defense.evadeTimeouts && resultRow.verdict == Verdict::Passed) {
                resultRow.verdict = Verdict::Inconclusive;
                report << "\nDirected evasion lacked completion evidence; the capability was disabled for this run.";
            }
            resultRow.powerHits = hitCounts.power;
            resultRow.dodges = mod::combat_tests::telemetry::Read().dodgeEvents;
            resultRow.attackHandoffs = interference::external_attacks::Read().accepted;
            resultRow.reactionWaits = defense.releaseDeferrals;
            report << "\nFunctional verdict: " << Name(resultRow.verdict) << "; " << assessment.reason;
            report << "\nTactics: approach " << tacticalMetrics.intents[1] << " s; guard " << tacticalMetrics.intents[2]
                   << " s; interrupt " << tacticalMetrics.intents[3] << " s; recover " << tacticalMetrics.intents[4]
                   << " s; counter " << tacticalMetrics.intents[5] << " s; reposition " << tacticalMetrics.intents[6]
                   << " s. Closest threat " << tacticalMetrics.closest << "; movement " << tacticalMetrics.moved
                   << "; maximum threats " << tacticalMetrics.maxThreats << "; ally hit events "
                   << tacticalMetrics.allyHits << "; friendly hits " << tacticalMetrics.friendlyHits
                   << "; opponent crossfire " << tacticalMetrics.crossfire << "; actual opponent casts "
                   << tacticalMetrics.casts << "; terminal inactivity " << tacticalMetrics.inactivity
                   << " s; longest inactivity " << tacticalMetrics.longestInactivity << " s; heavy-windup samples "
                   << tacticalMetrics.powerWindups;
            report << "\nEngagement: first within 256 units " << engagementMetrics.firstReach
                   << " s; first outgoing hit " << engagementMetrics.firstContact
                   << " s (-1 none); time outside 256 units " << engagementMetrics.outsideReach
                   << " s; sampled damage outside that range " << engagementMetrics.closingDamage
                   << "; selected-threat switches " << engagementMetrics.targetSwitches
                   << ".\nProjectile observation: readable " << engagementMetrics.flightReadableSeconds
                   << " s; quiet flight path " << engagementMetrics.projectileGapSeconds
                   << " s; incoming projectile pressure " << engagementMetrics.projectilePressureSeconds
                   << ".\nCasting geometry: active/preparing self-targeted pressure "
                   << engagementMetrics.castingPressure << " s; estimated initial aim-line exposure "
                   << engagementMetrics.aimLineExposure << " s; line exits " << engagementMetrics.aimLineExits
                   << ". The line is a 96-unit half-width corridor anchored to caster facing at episode start. "
                      "Exits are geometric evidence, not verified projectile or area-damage avoidance.";
            report << "\nPrepared setup: weapon " << comparisonSetup.weapon << "; shield " << comparisonSetup.shield
                   << "; opponent base " << comparisonSetup.opponentBase << "; subject health/stamina/magicka "
                   << comparisonSetup.health << '/' << comparisonSetup.stamina << '/' << comparisonSetup.magicka
                   << "; ally/opponent health " << comparisonSetup.allyHealth << '/' << comparisonSetup.enemyHealth;
            const auto statsLine = [&](const char* label, const mod::combat_tests::resources::Stats& stats) {
                report << '\n' << label << " maxima H/S/M; regeneration H/S/M; multipliers H/S/M:";
                for (const auto value : stats)
                    report << ' ' << value;
            };
            statsLine("Subject", comparisonSetup.subjectStats);
            statsLine("Opponent", comparisonSetup.enemyStats);
            if (AllyEncounter())
                statsLine("Ally", comparisonSetup.allyStats);
            report << "\nCombat resource-change samples " << fixtureDriftSamples
                   << "; caster-interrupt bash requests/completed " << defense.casterBashRequests << '/'
                   << defense.casterBashCompleted
                   << ". Changes from prepared resources can be combat effects and do not alone invalidate a pair. "
                      "Bash completion is not proof that a spell was cancelled."
                   << "\nPrepared equipment fingerprints subject/opponent/auxiliary "
                   << comparisonSetup.subjectEquipment << '/' << comparisonSetup.enemyEquipment << '/'
                   << comparisonSetup.allyEquipment;
            if (AllyEncounter())
                report << "\nRanged ally: finite starting health " << tacticalMetrics.allyInitialHealth
                       << "; remaining " << tacticalMetrics.allyHealth << "; sampled health loss "
                       << tacticalMetrics.allyDamage << "; defeated " << allyDefeated << "; enemy targeting ally "
                       << tacticalMetrics.allyExposure << " s; first opponent hit or attention shift "
                       << tacticalMetrics.firstIntervention
                       << " s (-1 means none); friendly target selections rejected "
                       << observer_control::FriendlyRejections() << '.';
            report << "\nMovement controller cumulative stalls/detours: " << defense.movementStalls << '/'
                   << defense.movementDetours << "; final status " << defense.movementStatus << "; progress "
                   << defense.movementProgress << "; guard preparation pause " << defense.guardPreparing;
            report << "\nPrecision: API " << defense.precisionAvailable << "; callback observer "
                   << defense.precisionCallbacks << "; weapon capsule " << defense.weaponReach << "; incoming contacts "
                   << defense.precisionIncoming << "; outgoing contacts " << defense.precisionOutgoing
                   << ". Contacts are separate from TESHitEvent and damage totals."
                   << "\nExecution: Skyrim/SCAR ordinary offense; native guard/bash in the selected test scope; "
                      "navmesh-projected movement."
                   << " Directed dodge availability and completion are reported separately. Guard-specific magic "
                      "protection remains unverified.";
            std::snprintf(snapshot.report, sizeof(snapshot.report), "%s", report.str().c_str());
        }
        resultRow.encounterOutcome = encounterOutcome;
        resultRow.phase = result;
        resultRow.elapsed = snapshot.elapsed;
        resultRow.damage = snapshot.damage;
        resultRow.attacks = snapshot.attacks;
        resultRow.blocks = snapshot.blocks;
        resultRow.bashes = snapshot.bashes;
        resultRow.incoming = hitCounts.incoming;
        resultRow.blocked = hitCounts.blocked;
        resultRow.healing = snapshot.healingEffects;
        resultRow.engagement = engagementMetrics;
        resultRow.setup = comparisonSetup;
        resultRow.challengeEnabled = snapshot.tuning.challengeEnabled;
        resultRow.comparisonEligible = !plan.subject && snapshot.elapsed > 0 && snapshot.attacks > 0 &&
                                       tacticalMetrics.allyThreatObserved && !tacticalMetrics.friendlyHits &&
                                       !tacticalMetrics.crossfire &&
                                       (result == Phase::Complete || recoverableDefeat || allyDefeated);
        resultRow.allyDefeated = allyDefeated;
        resultRow.allyDamage = tacticalMetrics.allyDamage;
        resultRow.allyExposure = tacticalMetrics.allyExposure;
        resultRow.firstIntervention = tacticalMetrics.firstIntervention;
        resultRow.fixtureDriftSamples = fixtureDriftSamples;
        resultRow.casterBashRequests = defense.casterBashRequests;
        resultRow.casterBashCompleted = defense.casterBashCompleted;
        resultRow.evadeRequests = defense.evadeRequests;
        resultRow.evadeCompleted = defense.evadeCompleted;
        resultRow.evadeTimeouts = defense.evadeTimeouts;
        if (suiteActive) {
            resultRow.phaseCount = static_cast<std::uint32_t>(testSession.current.Results().size());
            for (std::size_t i = 0; i < resultRow.phaseCount; ++i)
                resultRow.phases[i] = PhaseSummary(i);
        }
        std::snprintf(resultRow.report, sizeof(resultRow.report), "%s", report.str().c_str());
        if (results.size() < currentRun)
            results.push_back(resultRow);
        else
            results.back() = resultRow;
        seriesReport += "\n=== Run " + std::to_string(currentRun) + " ===\n" + report.str() + "\n";
        seriesReport += mod::combat_tests::telemetry::Report(true);
        snapshot.run = currentRun;
        snapshot.requestedRuns = requestedRuns;
        snapshot.finishedRuns = static_cast<std::uint32_t>(results.size());
        nextRun = mod::test_rules::ContinueSeries(
            snapshot.finishedRuns, requestedRuns,
            mod::test_rules::RepeatableRun(result == Phase::Complete, recoverableDefeat || allyDefeated, abortRequested,
                                           initialized && suiteActive && result == Phase::Failed),
            !plan.subject && caseCleanupReady,
            defenderBatch ? mod::combat_tests::testing::RunPlan{mod::combat_tests::suite::Catalog,
                                                                {mod::combat_tests::testing::MaxRepeats, suiteSelection,
                                                                 suiteExpandVariations}}
                                .Runs()
                          : mod::test_rules::MaxRuns);
        csv.flush();
        WriteSeriesReport(nextRun);
        if (nextRun) {
            active = true;
            snapshot.active = 1;
            Status("Run restored; preparing the next run.");
        } else {
            csv.close();
            if (suiteActive)
                testSession.Close();
            mod::combat_tests::observer_position::End();
            if (mod::combat_tests::observer_position::Pending())
                Status("Observer return unconfirmed; resolve the position restoration before starting another test.");
            mod::combat_tests::RequestMenu(true);
        }
    }

    void Restore() {
        snapshot.phase = Phase::Restoring;
        if (suiteActive) {
            const auto pending = participantActors.Pending();
            if (pending.state != mod::combat_tests::testing::State::Ready) {
                Status(pending.reason.c_str());
                return;
            }
            if (!restoreCombatStopped)
                participantActors.StopCombat();
        }
        auto* subject = Actor(plan.subject);
        auto* enemy = Actor(plan.enemy);
        auto* auxiliary = Actor(plan.auxiliary);
        if (!restoreCombatStopped) {
            if (OwnedEnemy(auxiliary))
                auxiliary->StopCombat();
            if (OwnedEnemy(enemy)) {
                enemy->StopCombat();
                enemy->InterruptCast(true);
            }
            if (NaturalEncounter())
                natural::CleanupSummons();
            if (subject)
                subject->StopCombat();
            restoreCombatStopped = true;
        }
        interference::external_attacks::End();
        if (plan.playerChanged) {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || player->IsDead() || player->GetCurrentScene()) {
                Status("Restoration waiting for the player before restoring captured health.");
                return;
            }
            RestoreValue(player, RE::ActorValue::kHealth, plan.playerHealth);
            plan.playerChanged = 0;
        }
        if (defenseBegun && host.observe) {
            DefenderObservation current;
            if (host.observe(plan.subject, &current) && current.actionMode == snapshot.mode)
                lastDefense = current;
        }
        const auto defenseEnd = HealingEncounter() ? 1
                                : host.endDefense  ? host.endDefense(plan.subject, plan.generation, plan.guardPhase)
                                                   : 2;
        if (!defenseEnd) {
            Status("Waiting for the owned guard to release; no command is replayed.");
            return;
        }
        const bool defenseFault = defenseEnd == 2;
        if (!defenseFault)
            plan.guardPhase = 0;
        scene_control::End(plan.subject, plan.generation);
        const bool unverifiedEnemy = (enemy && !OwnedEnemy(enemy)) || (auxiliary && !OwnedEnemy(auxiliary));
        if (suiteActive) {
            const auto restored = testSession.RestoreCase();
            if (restored.state != mod::combat_tests::testing::State::Ready) {
                if (restored.state == mod::combat_tests::testing::State::Failed)
                    Finish(Phase::Failed, restored.reason.c_str());
                else
                    Status(restored.reason.c_str());
                return;
            }
            if (!unverifiedEnemy)
                plan.enemy = plan.auxiliary = plan.enemyBase = 0;
        }
        if (!suiteActive && OwnedEnemy(auxiliary)) {
            auxiliary->InterruptCast(true);
            auxiliary->StopCombat();
            auxiliary->Disable();
            auxiliary->SetDelete(true);
            plan.auxiliary = 0;
        }
        if (!suiteActive && OwnedEnemy(enemy)) {
            enemy->InterruptCast(true);
            enemy->StopCombat();
            enemy->Disable();
            enemy->SetDelete(true);
            plan.enemy = 0;
            if (NaturalEncounter())
                natural::Release();
            if (!plan.auxiliary)
                plan.enemyBase = 0;
        }
        if (!subject || subject->IsDead()) {
            Finish(Phase::Failed, "Subject unavailable; restoration needs manual review.");
            return;
        }
        Subject state;
        if (!ReadSubject(state) || !state.allowed || subject->GetCurrentScene()) {
            Status("Restoration waiting for follower/quest permission. No movement is forced.");
            return;
        }
        subject->StopCombat();
        if (plan.followRequest) {
            const auto result = host.commandState(plan.followRequest);
            if (result == CommandState::Pending) {
                Status("Waiting for the original Follow command before restoring; no command is replayed.");
                return;
            }
            if (result == CommandState::Unknown) {
                Finish(Phase::Failed, "Follow receipt unavailable; recovery retained for manual review.");
                return;
            }
            if (!plan.waitRequest && Value(subject, RE::ActorValue::kWaitingForPlayer) == 0 &&
                (result == CommandState::Succeeded || result == CommandState::Failed)) {
                plan.waitRequest = host.setFollowing(plan.subject, false, plan.followRequest);
                if (!plan.waitRequest) {
                    Finish(Phase::Failed,
                           "Wait restoration rejected or superseded by another command; recovery retained.");
                    return;
                }
            }
            if (plan.waitRequest) {
                const auto restored = host.commandState(plan.waitRequest);
                if (restored == CommandState::Pending) {
                    Status("Waiting for Wait restoration to complete.");
                    return;
                }
                if (restored != CommandState::Succeeded) {
                    Finish(Phase::Failed, "Wait restoration completion unverified; recovery retained for review.");
                    return;
                }
            }
        }
        RestoreValue(subject, RE::ActorValue::kHealth, plan.health);
        RestoreValue(subject, RE::ActorValue::kStamina, plan.stamina);
        RestoreValue(subject, RE::ActorValue::kMagicka, plan.magicka);
        if (mod::test_rules::RestoreRole(plan.changedRole != 0, state.role, plan.encounter)) {
            plan.changedRole = 0;
            if (!host.setRole || !host.setRole(plan.subject, plan.role)) {
                Finish(Phase::Failed, "Original role needs manual restoration; recovery retained.");
                return;
            }
        }
        auto* origin = Reference(plan.origin);
        if (!origin || !Dynamic(origin) || !origin->GetBaseObject() || origin->GetBaseObject()->GetFormID() != 0x3B) {
            Finish(Phase::Failed, "Original position marker is unavailable; restoration needs review.");
            return;
        }
        // Native action teardown has finished. Drop our graph reference before
        // returning the actor to a cell that may unload its arena biped/3D.
        mod::combat_tests::telemetry::End();
        bool atOrigin = subject->GetParentCell() == origin->GetParentCell() &&
                        subject->GetPosition().GetDistance(origin->GetPosition()) < 32;
        if (mod::test_rules::RestorePosition(atOrigin, plan.restoreIssued != 0) ==
            mod::test_rules::PositionAction::Move) {
            plan.restoreIssued = 1;
            SKSE::log::info(
                "Combat tests: run {} returning subject {:08X}; cell {:08X} -> {:08X}; graph observer detached",
                currentRun, plan.subject, subject->GetParentCell() ? subject->GetParentCell()->GetFormID() : 0,
                origin->GetParentCell() ? origin->GetParentCell()->GetFormID() : 0);
            subject->MoveTo(origin);
            atOrigin = subject->GetParentCell() == origin->GetParentCell() &&
                       subject->GetPosition().GetDistance(origin->GetPosition()) < 32;
        }
        if (!atOrigin) {
            Status("Position restoration issued; waiting for its result without replaying it.");
            return;
        }
        if (unverifiedEnemy) {
            Finish(Phase::Failed,
                   "Opponent ownership is unverified; it was not deleted. Recovery references retained for review.");
            return;
        }
        if (defenseFault) {
            restoredForReview = true;
            plan.guardPhase = host.defenseState ? host.defenseState(plan.subject, plan.generation) : plan.guardPhase;
            Finish(Phase::Failed,
                   "Position/resources restored; native action cleanup remains unconfirmed. Recovery retained for "
                   "manual review.");
            return;
        }
        if (recoverableDefeat) {
            const auto recovery =
                mod::test_rules::ObserveDefeatRecovery(subject->AsActorState()->IsBleedingOut(),
                                                       Value(subject, RE::ActorValue::kHealth), defeatRecoveryElapsed);
            if (recovery == mod::test_rules::DefeatRecovery::Wait) {
                Status("Resources restored; waiting for bleedout recovery before continuing.");
                return;
            }
            if (recovery == mod::test_rules::DefeatRecovery::Review) {
                Finish(Phase::Failed,
                       "Defeat recovery unconfirmed; remaining runs stopped and recovery retained for review.");
                return;
            }
        }
        origin->Disable();
        origin->SetDelete(true);
        SKSE::log::info("Combat tests: run {} restoration complete; subject {:08X}; encounter {}; mode {}", currentRun,
                        plan.subject, plan.encounter, plan.mode);
        plan = {};
        Finish(outcome,
               outcome == Phase::Complete  ? "Test complete; position, resources and role restored."
               : outcome == Phase::Aborted ? "Test aborted; temporary changes restored."
               : allyDefeated              ? "Ally defeated; run failed and temporary changes restored."
               : recoverableDefeat
                   ? "Follower defeated; run failed, temporary changes restored and bleedout recovery confirmed."
                   : "Test invalidated; temporary changes restored.");
    }

    void Stop(Phase result, const char* reason) {
        outcome = result;
        if (stopReason.empty()) {
            stopReason = reason;
            if (initialized && (snapshot.phase == Phase::Running || snapshot.phase == Phase::Countdown)) {
                auto* subject = Actor(plan.subject);
                auto* enemy = Actor(plan.enemy);
                if (NaturalEncounter() && snapshot.phase == Phase::Running && subject) {
                    const float health = Value(subject, RE::ActorValue::kHealth);
                    snapshot.damage += std::max(previousHealth - health, 0.0f);
                    previousHealth = health;
                }
                if (subject && enemy)
                    WriteSample(subject, enemy, "stopping");
            }
        }
        Status(reason);
        if (beginPending) {
            Status("Stop requested; waiting for the combat-start callback before restoring.");
            return;
        }
        if (initialized)
            Restore();
        else
            Finish(result, reason);
    }

    void Initialize() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* start = Marker(0x811);
        auto* enemyStart = Marker(0x812);
        auto* subject = Actor(requestedActor);
        Subject state;
        if (!player || !start || !enemyStart || player->GetParentCell() != start->GetParentCell()) {
            Finish(Phase::Failed, "Enter YLIWFCombatArena before starting the test.");
            return;
        }
        const bool managed = ReadSubject(state);
        const auto startup = mod::test_rules::Startup(
            {subject != nullptr, managed, state.allowed != 0, subject && subject->IsDead(),
             subject && subject->IsDisabled(), subject && subject->IsOnMount(),
             subject && subject->AsActorState()->IsUnconscious(), subject && subject->AsActorState()->IsBleedingOut(),
             subject && subject->GetCurrentScene(), subject && subject->IsInCombat()},
            startupWait);
        if (startup == mod::test_rules::StartupDecision::WaitScene ||
            startup == mod::test_rules::StartupDecision::WaitCombat) {
            char message[256];
            const auto* scene = subject->GetCurrentScene();
            const auto* file = scene ? scene->GetFile(0) : nullptr;
            if (scene)
                std::snprintf(message, sizeof(message),
                              "Waiting for scene %08X (%s) to finish before setup: %.0f/%.0f s.", scene->GetFormID(),
                              file ? file->fileName : "runtime", startupWait, mod::test_rules::StartupSettleTimeout);
            else
                std::snprintf(message, sizeof(message), "Waiting for existing combat to end before setup: %.0f/%.0f s.",
                              startupWait, mod::test_rules::StartupSettleTimeout);
            Status(message);
            return;
        }
        if (startup == mod::test_rules::StartupDecision::Reject) {
            std::ostringstream reason;
            reason << "Subject unavailable at startup: managed " << managed << "; allowed " << state.allowed;
            if (subject) {
                const auto* scene = subject->GetCurrentScene();
                reason << "; dead " << subject->IsDead() << "; disabled " << subject->IsDisabled() << "; combat "
                       << subject->IsInCombat() << "; scene " << std::hex << (scene ? scene->GetFormID() : 0)
                       << std::dec << "; mounted " << subject->IsOnMount() << "; unconscious "
                       << subject->AsActorState()->IsUnconscious() << "; bleeding out "
                       << subject->AsActorState()->IsBleedingOut() << "; health "
                       << Value(subject, RE::ActorValue::kHealth);
            } else {
                reason << "; actor reference unavailable";
            }
            if (state.reason[0])
                reason << ". Host: " << state.reason;
            Finish(Phase::Failed, reason.str().c_str());
            return;
        }
        const auto arena = mod::combat_tests::testing::CheckArena(subject, start);
        if (arena.state != mod::combat_tests::testing::State::Ready) {
            Finish(Phase::Failed, arena.reason.c_str());
            return;
        }
        if (suiteActive) {
            const auto prepared = testSession.Prepare();
            if (prepared.state != mod::combat_tests::testing::State::Ready) {
                if (prepared.state == mod::combat_tests::testing::State::Failed)
                    Finish(Phase::Failed, prepared.reason.c_str());
                else
                    Status(prepared.reason.c_str());
                return;
            }
        } else if (!HealingEncounter() && !mod::combat_tests::observer_position::Begin()) {
            Finish(Phase::Failed, "Observer positioning unavailable; fixture setup was not started.");
            return;
        }
        plan = {subject->GetFormID(),
                0,
                0,
                0,
                state.role,
                Value(subject, RE::ActorValue::kHealth),
                std::max(Value(subject, RE::ActorValue::kStamina), 0.0f),
                std::max(Value(subject, RE::ActorValue::kMagicka), 0.0f),
                0,
                0};
        plan.generation = ticket;
        plan.mode = snapshot.mode;
        plan.encounter = snapshot.encounter;
        plan.fixture = SuiteCase() ? SuiteCase()->id : 0;
        plan.wasWaiting = Value(subject, RE::ActorValue::kWaitingForPlayer) != 0;
        plan.playerHealth = Value(player, RE::ActorValue::kHealth);
        plan.playerChanged = HealingEncounter() ? 1 : 0;
        if (HealingEncounter() &&
            (player->IsDead() || player->GetCurrentScene() || !std::isfinite(plan.playerHealth) ||
             plan.playerHealth <= 0 || mod::combat_tests::healer::InjuryTarget(MaximumHealth(player)) <= 0)) {
            plan = {};
            Finish(Phase::Failed, "Player must be alive, outside scenes and have valid health for Party pressure.");
            return;
        }
        auto initialPlan = plan;
        if (suiteActive)
            initialPlan.fixture = 0;
        if (!mod::test_rules::Valid(initialPlan)) {
            Finish(Phase::Failed, "Invalid subject resources; test not started.");
            plan = {};
            return;
        }
        auto* data = RE::TESDataHandler::GetSingleton();
        auto* markerBase = data->LookupForm<RE::TESObjectSTAT>(0x3B, "Skyrim.esm");
        auto marker = markerBase ? subject->PlaceObjectAtMe(markerBase, true) : RE::NiPointer<RE::TESObjectREFR>{};
        if (!marker) {
            Finish(Phase::Failed, "Could not capture the subject's original position.");
            plan = {};
            return;
        }
        marker->data.angle = subject->GetAngle();
        plan.origin = marker->GetFormID();
        initialized = true;
        if (!interference::Begin(subject, snapshot.bypassInterruptions != 0)) {
            Stop(Phase::Failed, "Requested interruption bypass is unavailable for this actor or graph hook.");
            return;
        }
        if (!interference::und::Begin(subject, snapshot.bypassDodging != 0,
                                      (snapshot.experiments.flags & DodgeBudget) != 0,
                                      snapshot.experiments.reserveFraction, snapshot.experiments.dodgeCooldown)) {
            Stop(Phase::Failed, "Requested UND exclusion is unavailable for the installed DLL or hooks.");
            return;
        }
        if (plan.wasWaiting) {
            plan.followRequest = host.setFollowing(plan.subject, true, 0);
            if (!plan.followRequest) {
                Stop(Phase::Failed, "Follow command was rejected before summoning.");
                return;
            }
        }
        subject->MoveTo(HealingEncounter() ? Marker(0x813) : start);
        if (!scene_control::Begin(plan.subject, plan.generation, HealingEncounter())) {
            Stop(Phase::Failed, "Arena movement control unavailable; install matching core, add-on DLL and arena ESP.");
            return;
        }
        phaseTime = 0;
        Status("Summoning follower; waiting for loading and Follow completion.");
    }

    void PrepareSubject(float delta) {
        phaseTime += delta;
        if (suiteActive && casePreparationStarted) {
            PrepareCaseActors();
            return;
        }
        auto* subject = Actor(plan.subject);
        auto* start = Marker(0x811);
        Subject state;
        if (!subject || !start || !ReadSubject(state) || !state.allowed || subject->IsDead() || subject->IsDisabled() ||
            subject->GetCurrentScene() || subject->IsInCombat()) {
            Stop(Phase::Failed, "Follower became unavailable during setup.");
            return;
        }
        const auto command = plan.followRequest ? host.commandState(plan.followRequest) : CommandState::Succeeded;
        if (command == CommandState::Unknown || command == CommandState::Failed) {
            Stop(Phase::Failed, "Follow completion failed or is unavailable; no command was replayed.");
            return;
        }
        if (command == CommandState::Pending || !subject->Is3DLoaded() ||
            subject->GetParentCell() != start->GetParentCell()) {
            if (phaseTime > 30)
                Stop(Phase::Failed, "Follower loading or Follow completion timed out; restoring.");
            return;
        }
        const auto requiredRole = mod::test_rules::EncounterRole(snapshot.encounter);
        if (!(state.eligibleRoles & (1u << mod::test_rules::EquipmentRole(snapshot.encounter))) ||
            Value(subject, RE::ActorValue::kWaitingForPlayer) != 0) {
            Stop(Phase::Failed, "Loaded follower lacks the required equipment/spells or is no longer following.");
            return;
        }
        auto* data = RE::TESDataHandler::GetSingleton();
        auto* enemyStart = Marker(0x812);
        if (!data || !enemyStart) {
            Stop(Phase::Failed, "Arena setup markers are unavailable.");
            return;
        }
        if (state.role != requiredRole) {
            if (!host.setRole || !host.setRole(plan.subject, requiredRole)) {
                Stop(Phase::Failed, "Encounter role assignment was rejected.");
                return;
            }
            plan.changedRole = 1;
        }
        if (!HealingEncounter() &&
            !(DirectEncounter() ? host.beginDirect && host.beginDirect(plan.subject, plan.generation)
                                : host.beginDefense && host.beginDefense(plan.subject, plan.mode, plan.generation))) {
            Stop(Phase::Failed,
                 "Native defense unavailable or already owned; verify the supported runtime and action hooks.");
            return;
        }
        defenseBegun = !HealingEncounter();
        if (defenseBegun &&
            (!host.setDefenderTuning || !host.setDefenderTuning(plan.subject, plan.generation, &snapshot.tuning))) {
            Stop(Phase::Failed, "Defender tuning profile was rejected by the core.");
            return;
        }
        auto* movementAnchor = data->LookupForm<RE::TESObjectREFR>(0x811, ArenaPlugin);
        if (defenseBegun && !mod::test_rules::DirectEncounter(snapshot.encounter))
            if (const auto* backend = interference::und::DirectedBackend(); backend && host.setEvadeBackend)
                host.setEvadeBackend(plan.subject, plan.generation, backend);
        if (defenseBegun &&
            (!movementAnchor || !host.setMovementBounds ||
             !host.setMovementBounds(plan.subject, plan.generation, movementAnchor->GetFormID(), 4096))) {
            Stop(Phase::Failed, "Bounded test movement configuration was rejected.");
            return;
        }
        const bool powerBash = (snapshot.experiments.flags & NativePowerBash) != 0;
        auto* bashIdle = powerBash && data ? data->LookupForm<RE::TESIdleForm>(PowerBashIdle, ArenaPlugin) : nullptr;
        auto* bashGate = powerBash ? RE::TESForm::LookupByID<RE::TESIdleForm>(PowerAttackRoot) : nullptr;
        if (defenseBegun && (snapshot.experiments.flags & NativeBash) &&
            (!host.setDefensiveBash ||
             !host.setDefensiveBash(plan.subject, plan.generation, powerBash ? 2 : 1,
                                    bashIdle ? bashIdle->GetFormID() : 0, bashGate ? bashGate->GetFormID() : 0))) {
            Stop(Phase::Failed,
                 "Native bash configuration unavailable; install matching core, add-on DLL and arena "
                 "ESP.");
            return;
        }
        if (defenseBegun)
            interference::external_attacks::Begin(plan.subject, plan.generation, host.setAttackCommitment);
        if (defenseBegun && PrimitiveKind() == mod::combat_tests::primitive::Kind::RightAttack) {
            mod::combat_tests::telemetry::ObserveAttacks(plan.subject, plan.generation, host);
            mod::combat_tests::telemetry::Attach(subject);
        }
        if (defenseBegun && (snapshot.experiments.flags & NativeBash)) {
            mod::combat_tests::telemetry::ObserveBashes(plan.subject, plan.generation, host);
            mod::combat_tests::telemetry::Attach(subject);
        }
        for (const auto value : {RE::ActorValue::kHealth, RE::ActorValue::kStamina, RE::ActorValue::kMagicka})
            subject->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, value, 1000000);
        casePreparationStarted = true;
        PrepareCaseActors();
    }

    void Tick() {
        std::scoped_lock lock(mutex);
        const auto now = Clock::now();
        const auto delta = mod::test_rules::ActiveDelta(std::chrono::duration<float>(now - lastTick).count(), Paused());
        lastTick = now;
        if (!ready || delta == 0)
            return;
        scene_control::Refresh();
        observer_control::Refresh();
        spell_diagnostic::Tick(delta, host);
        travel::Update(ready,
                       active || unsupported || plan.subject != 0 || recheckRequested || spell_diagnostic::Busy());
        if (travelMenuPending && !travel::NeedsUpdate()) {
            travelMenuPending = false;
            mod::combat_tests::RequestMenu(true);
        }
        if (recheckRequested.exchange(false) &&
            mod::test_rules::CanRecheckRecovery(ready, active, unsupported, plan.subject != 0, restoredForReview,
                                                beginPending)) {
            const auto result = host.endDefense ? host.endDefense(plan.subject, plan.generation, plan.guardPhase) : 2;
            auto* origin = Reference(plan.origin);
            const bool ownedOrigin =
                origin && Dynamic(origin) && origin->GetBaseObject() && origin->GetBaseObject()->GetFormID() == 0x3B;
            if (result == 1 && ownedOrigin) {
                origin->Disable();
                origin->SetDelete(true);
                plan = {};
                restoredForReview = false;
                Finish(Phase::Failed, "Recovery verified; new tests are available. No gameplay command was replayed.");
            } else {
                Status(
                    "Recovery remains unresolved. No command was replayed; inspect the last report or load the "
                    "pre-test save.");
                mod::combat_tests::RequestMenu(true);
            }
        }
        if (!active)
            return;
        if (nextRun) {
            nextRun = false;
            if (abortRequested) {
                active = false;
                snapshot.active = 0;
                snapshot.phase = Phase::Aborted;
                Status("Remaining repeats cancelled; completed runs are restored.");
                WriteSeriesReport(false, "Remaining repeats cancelled after restoration.");
                csv.close();
                mod::combat_tests::observer_position::End();
                mod::combat_tests::RequestMenu(true);
                return;
            }
            active = false;
            ++currentRun;
            if (!StartRun(snapshot.subject, snapshot.encounter)) {
                snapshot.active = 0;
                snapshot.phase = Phase::Failed;
                Status("Next repeat could not start; completed results are retained.");
                WriteSeriesReport(false, "Next repeat could not start.");
                csv.close();
                mod::combat_tests::observer_position::End();
                mod::combat_tests::RequestMenu(true);
            }
            return;
        }
        if (!initialized) {
            startupWait += delta;
            if (abortRequested)
                Finish(Phase::Aborted, "Test cancelled before setup.");
            else
                Initialize();
            return;
        }
        if (snapshot.phase == Phase::Restoring) {
            auto* subject = Actor(plan.subject);
            if (recoverableDefeat && subject && subject->AsActorState()->IsBleedingOut())
                defeatRecoveryElapsed += delta;
            Restore();
            return;
        }
        if (abortRequested || !stopReason.empty()) {
            Stop(abortRequested && outcome != Phase::Failed ? Phase::Aborted : outcome,
                 "Stop requested; restoring the current run.");
            return;
        }
        if (!plan.enemy || (suiteActive && !testSession.current.Prepared())) {
            PrepareSubject(delta);
            return;
        }
        auto* subject = Actor(plan.subject);
        auto* enemy = Actor(plan.enemy);
        if (snapshot.phase == Phase::Preparing && subject && enemy &&
            (!subject->Is3DLoaded() || !enemy->Is3DLoaded())) {
            phaseTime += delta;
            if (phaseTime > 15)
                Stop(Phase::Failed, "Actors did not finish loading into the arena.");
            return;
        }
        Subject state;
        if (!subject || subject->IsDead() || subject->AsActorState()->IsBleedingOut() ||
            Value(subject, RE::ActorValue::kHealth) <= 0 || subject->IsDisabled() || subject->GetCurrentScene()) {
            recoverableDefeat = mod::test_rules::RecoverableDefeat(
                snapshot.phase == Phase::Running, subject != nullptr, subject && subject->IsDead(),
                subject && (subject->AsActorState()->IsBleedingOut() || Value(subject, RE::ActorValue::kHealth) <= 0),
                subject && subject->IsDisabled(), subject && subject->GetCurrentScene());
            if (NaturalEncounter() && recoverableDefeat) {
                encounterOutcome = natural::Resolve(true, true, false, false);
                Stop(Phase::Complete, "Defender defeated; restoring before the next run.");
                return;
            }
            Stop(Phase::Failed,
                 mod::combat_tests::metrics::Unavailable(
                     subject != nullptr, subject && subject->IsDead(),
                     subject &&
                         (subject->AsActorState()->IsBleedingOut() || Value(subject, RE::ActorValue::kHealth) <= 0),
                     subject && subject->IsDisabled(), subject && subject->GetCurrentScene(), true, true, true));
            return;
        }
        if (NaturalEncounter() && snapshot.phase == Phase::Running && OwnedEnemy(enemy) &&
            (enemy->IsDead() || enemy->AsActorState()->IsBleedingOut() || Value(enemy, RE::ActorValue::kHealth) <= 0)) {
            encounterOutcome = natural::Resolve(true, false, true, false);
            Stop(Phase::Complete, "Opponent defeated; restoring.");
            return;
        }
        if (!OwnedEnemy(enemy) || enemy->IsDead() || enemy->AsActorState()->IsBleedingOut()) {
            Stop(Phase::Failed, "Test opponent became unavailable or downed.");
            return;
        }
        if (!ReadSubject(state) || !state.allowed || state.role != mod::test_rules::EncounterRole(plan.encounter) ||
            !(state.eligibleRoles & (1u << mod::test_rules::EquipmentRole(plan.encounter)))) {
            const auto* reason = mod::combat_tests::metrics::Unavailable(
                true, false, false, false, false,
                (state.eligibleRoles & (1u << mod::test_rules::EquipmentRole(plan.encounter))) != 0,
                state.role == mod::test_rules::EncounterRole(plan.encounter), state.allowed != 0);
            Stop(Phase::Failed, reason ? reason : "Test opponent became unavailable or downed.");
            return;
        }
        mod::combat_tests::telemetry::Attach(subject);
        interference::external_attacks::Sample(subject);
        DefenderObservation defense;
        const bool observed = !HealingEncounter() && host.observe && host.observe(plan.subject, &defense);
        if (observed)
            lastDefense = defense;
        const auto threatKind = static_cast<ThreatKind>(defense.threatKind);
        const bool emergency = observed && defense.available && defense.age >= 0 && defense.age <= .3f &&
                               defense.targeting && defense.threatVisible &&
                               (defense.threatAttacking || defense.threatPreparing) &&
                               (threatKind == ThreatKind::HeavyMelee || threatKind == ThreatKind::Projectile ||
                                threatKind == ThreatKind::Concentration || threatKind == ThreatKind::Area);
        interference::und::Sample(subject, observed && defense.recovering, emergency);
        if (observed && snapshot.phase == Phase::Running && defense.actionFault) {
            Stop(Phase::Failed,
                 defense.actionStatus[0] ? defense.actionStatus : "Native guard acknowledgement failed.");
            return;
        }
        auto* start = Marker(0x811);
        if (!start || RE::PlayerCharacter::GetSingleton()->GetParentCell() != start->GetParentCell()) {
            Stop(Phase::Aborted, "Player left the arena.");
            return;
        }
        if (auto* processes = RE::ProcessLists::GetSingleton())
            for (const auto& handle : processes->highActorHandles)
                if (auto other = handle.get();
                    other && other.get() != subject && other.get() != enemy && other->GetFormID() != plan.auxiliary &&
                    other.get() != RE::PlayerCharacter::GetSingleton() &&
                    (!suiteActive || !participantActors.participants.Find(other->GetFormID())) && !other->IsDead() &&
                    !other->IsDisabled() && other->GetParentCell() == start->GetParentCell()) {
                    if (NaturalEncounter() &&
                        natural::Participant(other->GetFormID(), plan.subject, plan.enemy) != other->GetFormID())
                        continue;
                    Stop(Phase::Failed, "Another actor entered the arena; run invalidated.");
                    return;
                }
        if (snapshot.phase == Phase::Preparing || (snapshot.phase == Phase::Countdown && !beginIssued))
            WriteSample(subject, enemy, snapshot.phase == Phase::Preparing ? "preparing" : "countdown");
        phaseTime += delta;
        if (snapshot.phase == Phase::Preparing) {
            auto* extra = Actor(plan.auxiliary);
            const bool extraLoaded = suiteActive
                                         ? participantActors.Loaded(start)
                                         : !plan.auxiliary || (OwnedEnemy(extra) && extra->Is3DLoaded() &&
                                                               extra->GetParentCell() == start->GetParentCell());
            if (!subject->Is3DLoaded() || !enemy->Is3DLoaded() || !extraLoaded ||
                subject->GetParentCell() != start->GetParentCell()) {
                if (phaseTime > 15)
                    Stop(Phase::Failed, "Actors did not finish loading into the arena.");
                return;
            }
            if (phaseTime < 2 && !configured)
                return;
            if (suiteActive) {
                const auto preparation = participantActors.Prepare(ticket);
                if (preparation.state == mod::combat_tests::testing::State::Failed) {
                    Stop(Phase::Failed, preparation.reason.c_str());
                    return;
                }
                if (preparation.state == mod::combat_tests::testing::State::Pending) {
                    if (phaseTime > 30)
                        Stop(Phase::Failed, "Participant setup receipt timed out; waiting for cleanup.");
                    return;
                }
                if (!configured) {
                    configured = true;
                    phaseTime = 0;
                    fixtureSettling = {};
                }
                const auto settled = fixtureSettling.Observe(delta, ResourceStats(enemy), ResourceStats(extra),
                                                             ParticipantFingerprint(), normalized ? 1.0f : 4.0f);
                if (settled == mod::combat_tests::fixture_startup::Result::Expired) {
                    Stop(Phase::Failed, "Participant equipment and resources did not settle.");
                    return;
                }
                if (settled != mod::combat_tests::fixture_startup::Result::Ready)
                    return;
                const auto normalization = participantActors.Normalize(ticket);
                if (normalization.state == mod::combat_tests::testing::State::Failed) {
                    Stop(Phase::Failed, normalization.reason.c_str());
                    return;
                }
                if (normalization.state == mod::combat_tests::testing::State::Pending) {
                    if (phaseTime > 30)
                        Stop(Phase::Failed, "Participant normalization receipt timed out; waiting for cleanup.");
                    return;
                }
                if (!normalized) {
                    normalized = true;
                    phaseTime = 0;
                    fixtureSettling = {};
                    return;
                }
                if (phaseTime < 1)
                    return;
                const auto failure = participantActors.Validate(false);
                const auto resources = FixtureResourceFailure();
                if (!failure.empty() || !resources.empty()) {
                    Stop(Phase::Failed, !failure.empty() ? failure.c_str() : resources.c_str());
                    return;
                }
            }
            if (suiteActive) {
                if (const auto failure = participantActors.ProbeProtection(); !failure.empty()) {
                    Stop(Phase::Failed, failure.c_str());
                    return;
                }
            }
            if (NaturalEncounter())
                natural::Capture();
            CapturePreparedSetup(subject, enemy);
            if (suiteActive)
                participantActors.Capture();
            enemy->StopCombat();
            subject->StopCombat();
            snapshot.phase = Phase::Countdown;
            phaseTime = 0;
            Status(NaturalEncounter() ? "Mage ready. Test starts in three active seconds."
                                      : "Protection verified. Test starts in three active seconds.");
            return;
        }
        if (snapshot.phase == Phase::Countdown) {
            if (suiteActive && beginIssued && beginPending) {
                const auto result = participantActors.Start(plan.subject, ticket);
                if (result.state == mod::combat_tests::testing::State::Failed) {
                    beginPending = false;
                    Stop(Phase::Failed, result.reason.c_str());
                    return;
                }
                if (result.state == mod::combat_tests::testing::State::Pending) {
                    if (phaseTime > 30) {
                        beginPending = false;
                        Stop(Phase::Failed, "Participant combat receipt timed out; waiting for cleanup.");
                    }
                    return;
                }
                beginPending = false;
                phaseTime = 0;
            }
            if (beginIssued) {
                if (beginPending)
                    return;
                const auto spellFailure = InspectFixtureSpellbooks();
                ObserveAlly(enemy, 0);
                if (allyDefeated) {
                    Stop(Phase::Failed, "Ranged ally defeated during combat startup; restoring before the next run.");
                    return;
                }
                const bool equipped = OpponentEquipped(enemy) && AllyReady() && spellFailure.empty();
                const bool subjectCombat = subject->IsInCombat();
                const bool enemyCombat = enemy->IsInCombat();
                tacticalMetrics.subjectHostile = subject->IsHostileToActor(enemy);
                tacticalMetrics.enemyHostile = enemy->IsHostileToActor(subject);
                auto* auxiliary = Actor(plan.auxiliary);
                const auto targetID = [](RE::Actor* actor) {
                    const auto target = actor ? actor->GetActorRuntimeData().currentCombatTarget.get() : nullptr;
                    const auto id = target ? target->GetFormID() : 0;
                    return NaturalEncounter() ? natural::Participant(id, plan.subject, plan.enemy) : id;
                };
                const auto* observer = RE::PlayerCharacter::GetSingleton();
                startupCombat = {plan.subject,
                                 plan.enemy,
                                 plan.auxiliary,
                                 observer ? observer->GetFormID() : 0,
                                 targetID(subject),
                                 targetID(enemy),
                                 targetID(auxiliary),
                                 AllyEncounter(),
                                 AllyReady(),
                                 NaturalEncounter(),
                                 tacticalMetrics.subjectHostile != 0,
                                 tacticalMetrics.enemyHostile != 0,
                                 auxiliary && enemy->IsHostileToActor(auxiliary)};
                const auto engagement =
                    suiteActive ? ParticipantEngagement() : mod::combat_tests::startup_combat::Observe(startupCombat);
                WriteSample(subject, enemy, "starting");
                if (engagement == mod::combat_tests::startup_combat::State::UnsafeTarget) {
                    Stop(Phase::Failed, "Combat startup targeted the observer or an actor outside the fixture.");
                    return;
                }
                const auto decision =
                    mod::test_rules::ObserveStart(phaseTime, equipped, subjectCombat, enemyCombat,
                                                  engagement == mod::combat_tests::startup_combat::State::Ready);
                if (decision == mod::test_rules::StartDecision::Ready) {
                    snapshot.phase = Phase::Running;
                    phaseTime = 0;
                    if (suiteActive) {
                        testSession.context.observation = defense;
                        const auto entry = testSession.current.Enter();
                        UpdatePhaseSummary();
                        if (entry.state == mod::combat_tests::testing::State::Failed) {
                            Stop(Phase::Failed, entry.reason.c_str());
                            return;
                        }
                    }
                    previousHealth = Value(subject, RE::ActorValue::kHealth);
                    bool dodging = false;
                    const bool dodgeReadable = subject->GetGraphVariableBool("bIsDodging", dodging);
                    DefenderObservation initial;
                    const bool initialRead = host.observe && host.observe(plan.subject, &initial);
                    metricTracker.Observe({0, Value(subject, RE::ActorValue::kStamina),
                                           Maximum(subject, RE::ActorValue::kStamina), previousHealth, dodging,
                                           subject->IsAttacking(), initial.recovering != 0, hitCounts.incoming,
                                           hitCounts.outgoing, fixtureSteps, dodgeReadable,
                                           initialRead && initial.available && initial.requested && initial.age >= 0 &&
                                               initial.age <= .3f,
                                           snapshot.tuning.recoveryEnter, snapshot.tuning.recoveryExit});
                    enemyDamage =
                        enemy->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth);
                    if (auto* extra = Actor(plan.auxiliary))
                        auxiliaryDamage =
                            extra->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth);
                    previousPlayerHealth = Value(RE::PlayerCharacter::GetSingleton(), RE::ActorValue::kHealth);
                    ObserveAlly(enemy, 0);
                    Status("Encounter running. Open the menu to pause; Abort restores temporary changes.");
                } else if (decision == mod::test_rules::StartDecision::Failed) {
                    if (!spellFailure.empty()) {
                        Stop(Phase::Failed, spellFailure.c_str());
                        return;
                    }
                    const auto* weapon = enemy->GetEquippedObject(false);
                    char reason[512];
                    std::snprintf(
                        reason, sizeof(reason),
                        "Combat startup timed out: loadout %s; follower combat %s; opponent combat %s; weapon "
                        "%08X %.64s; hostility subject/opponent %d/%d; opponent aggression %.1f; "
                        "ally ready %d; opponent hostile to ally %d; opponent target %08X; ally %08X.",
                        equipped ? "yes" : "no", subjectCombat ? "yes" : "no", enemyCombat ? "yes" : "no",
                        weapon ? weapon->GetFormID() : 0, weapon && weapon->GetName() ? weapon->GetName() : "None",
                        tacticalMetrics.subjectHostile, tacticalMetrics.enemyHostile,
                        Value(enemy, RE::ActorValue::kAggression), startupCombat.allyReady,
                        startupCombat.opponentHostileToAlly, startupCombat.opponentTarget, startupCombat.auxiliary);
                    Stop(Phase::Failed, reason);
                }
                return;
            }
            if (phaseTime < mod::test_rules::Countdown)
                return;
            // Ordinary following/sandboxing can move either actor during setup.
            // Establish the same start positions immediately before combat.
            subject->MoveTo(HealingEncounter() ? Marker(0x813) : start);
            enemy->MoveTo(Marker(0x812));
            if (suiteActive) {
                participantActors.Position(subject);
                auto allies = participantActors.IDs(mod::combat_tests::testing::ActorRole::Ally);
                const auto opponents = participantActors.IDs(mod::combat_tests::testing::ActorRole::Opponent);
                allies.push_back(plan.subject);
                if (!observer_control::Begin(allies, opponents, plan.generation) ||
                    !scene_control::StartCombat(plan.subject, plan.generation)) {
                    Stop(Phase::Failed, "Participant team or movement isolation could not start.");
                    return;
                }
                beginIssued = beginPending = true;
                const auto result = participantActors.Start(plan.subject, ticket);
                if (result.state == mod::combat_tests::testing::State::Failed) {
                    beginPending = false;
                    Stop(Phase::Failed, result.reason.c_str());
                }
                return;
            }
            Stop(Phase::Failed, "No suite owns the encounter startup.");
            return;
        }
        snapshot.elapsed += delta;
        if (suiteActive) {
            testSession.context.elapsed = snapshot.elapsed;
            testSession.context.observation = defense;
            const auto phase = testSession.current.Tick(delta);
            UpdatePhaseSummary();
            if (phase.state == mod::combat_tests::testing::State::Failed) {
                Stop(Phase::Failed, phase.reason.c_str());
                return;
            }
        }
        ObserveAlly(enemy, delta);
        if (allyDefeated) {
            Stop(Phase::Failed, "Ranged ally defeated; restoring before the next run.");
            return;
        }
        if (const auto failure = InspectFixtureSpellbooks(); !failure.empty()) {
            Stop(Phase::Failed, failure.c_str());
            return;
        }
        if (phaseTime > 3 && (!subject->IsInCombat() || !enemy->IsInCombat())) {
            WriteSample(subject, enemy, "combat-ended");
            char reason[192];
            std::snprintf(
                reason, sizeof(reason),
                "Combat did not remain active: follower %s; opponent %s. No combat-start command was replayed.",
                subject->IsInCombat() ? "in combat" : "out of combat",
                enemy->IsInCombat() ? "in combat" : "out of combat");
            Stop(Phase::Failed, reason);
            return;
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        const auto target = enemy->GetActorRuntimeData().currentCombatTarget.get();
        const bool allyTarget = AllyEncounter() && target && target->GetFormID() == plan.auxiliary;
        const bool fixtureTarget = target && (suiteActive ? participantActors.participants.Opposed(
                                                                plan.enemy, target->GetFormID(), plan.subject)
                                                          : plan.auxiliary && target->GetFormID() == plan.auxiliary);
        if (suiteActive && ParticipantEngagement() == mod::combat_tests::startup_combat::State::UnsafeTarget) {
            Stop(Phase::Failed, "A participant selected an actor outside its opposing team.");
            return;
        }
        const bool summonedTarget = NaturalEncounter() && target &&
                                    natural::Participant(target->GetFormID(), plan.subject, plan.enemy) == plan.subject;
        bool castingAtObserver = false;
        if (target.get() == player)
            for (auto hand : {RE::MagicSystem::CastingSource::kLeftHand, RE::MagicSystem::CastingSource::kRightHand})
                if (const auto* caster = enemy->GetMagicCaster(hand);
                    caster && caster->state == RE::MagicCaster::State::kCasting)
                    castingAtObserver = true;
        auto* extraOpponent =
            !suiteActive && Fixture() == mod::combat_tests::suite::Fixture::Multiple ? Actor(plan.auxiliary) : nullptr;
        const auto extraTarget =
            extraOpponent ? extraOpponent->GetActorRuntimeData().currentCombatTarget.get() : nullptr;
        bool extraCastingAtObserver = false;
        if (extraOpponent && extraTarget.get() == player)
            for (const auto hand :
                 {RE::MagicSystem::CastingSource::kLeftHand, RE::MagicSystem::CastingSource::kRightHand})
                if (const auto* caster = extraOpponent->GetMagicCaster(hand);
                    caster && caster->state == RE::MagicCaster::State::kCasting)
                    extraCastingAtObserver = true;
        const bool wrongPrimary = castingAtObserver || (enemy->IsAttacking() && target.get() != subject &&
                                                        !allyTarget && !fixtureTarget && !summonedTarget);
        const bool wrongExtra =
            extraCastingAtObserver || (extraOpponent && extraOpponent->IsAttacking() && extraTarget.get() != subject);
        if (player->IsAttacking() || wrongPrimary || wrongExtra) {
            WriteSample(subject, enemy, "invalidated");
            if (player->IsAttacking()) {
                Stop(Phase::Failed, "Observing player attacked; run invalidated.");
            } else {
                const auto* offender = wrongPrimary ? enemy : extraOpponent;
                const auto* victim = wrongPrimary ? target.get() : extraTarget.get();
                char reason[256];
                std::snprintf(
                    reason, sizeof(reason),
                    "Opponent %08X attacked target %08X outside its allowed participants; observer positioned %s. "
                    "Run invalidated.",
                    offender->GetFormID(), victim ? victim->GetFormID() : 0,
                    mod::combat_tests::observer_position::Positioned() ? "yes" : "no");
                Stop(Phase::Failed, reason);
            }
            return;
        }
        if (!NaturalEncounter() &&
            (!OpponentEquipped(enemy) || std::abs(Value(enemy, RE::ActorValue::kWeaponSpeedMult) - 1) > 0.01f)) {
            const auto* weapon = enemy->GetEquippedObject(false);
            char reason[256];
            std::snprintf(reason, sizeof(reason),
                          "Opponent weapon or speed changed: weapon %08X %.64s; speed %.3f. Run invalidated.",
                          weapon ? weapon->GetFormID() : 0, weapon && weapon->GetName() ? weapon->GetName() : "None",
                          Value(enemy, RE::ActorValue::kWeaponSpeedMult));
            WriteSample(subject, enemy, "invalidated");
            Stop(Phase::Failed, reason);
            return;
        }
        if (!suiteActive && !NaturalEncounter() &&
            (!mod::combat_tests::fixture_protection::Protected(plan.enemy) || enemy->GetActorBase()->IsInvulnerable() ||
             !mod::combat_tests::resources::Undamaged(
                 enemyDamage,
                 enemy->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth)))) {
            Stop(Phase::Failed, "Opponent health damage changed despite protection; run invalidated.");
            return;
        }
        if (!suiteActive && plan.auxiliary) {
            auto* auxiliary = Actor(plan.auxiliary);
            const bool ally = AllyEncounter();
            if (!OwnedEnemy(auxiliary) || auxiliary->IsDead() || auxiliary->IsDisabled() || !auxiliary->Is3DLoaded() ||
                auxiliary->GetActorBase()->IsInvulnerable() ||
                mod::combat_tests::fixture_protection::Protected(plan.auxiliary) == ally) {
                Stop(Phase::Failed, "Additional fixture actor lost availability or protection.");
                return;
            }
            if (!ally && !mod::combat_tests::resources::Undamaged(
                             auxiliaryDamage, auxiliary->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage,
                                                                               RE::ActorValue::kHealth))) {
                Stop(Phase::Failed, "Additional opponent health damage changed despite protection; run invalidated.");
                return;
            }
        }
        tacticalMetrics.Contacts(hitCounts.incoming, hitCounts.outgoing);
        if (!NaturalEncounter() &&
            (!mod::combat_tests::resources::Stable(comparisonSetup.enemyStats, ResourceStats(enemy)) ||
             !mod::combat_tests::resources::Stable(comparisonSetup.allyStats, ResourceStats(Actor(plan.auxiliary)))))
            ++fixtureDriftSamples;
        tacticalMetrics.Observe(delta, defense.intent, defense.threatKind, defense.threatCount, defense.distance,
                                subject->GetPositionX(), subject->GetPositionY());
        tacticalMetrics.powerWindups +=
            defense.threatKind == 2 && enemy->AsActorState()->GetAttackState() == RE::ATTACK_STATE_ENUM::kDraw;
        const bool blocking = subject->IsBlocking();
        const bool bashing = subject->AsActorState()->GetAttackState() == RE::ATTACK_STATE_ENUM::kBash;
        const bool attacking = enemy->IsAttacking();
        snapshot.blocks += blocking && !previousBlock;
        snapshot.bashes += bashing && !previousBash;
        if (suiteActive) {
            snapshot.attacks = 0;
            for (const auto& entry : participantActors.participants.Entries())
                if (entry.spec.role == mod::combat_tests::testing::ActorRole::Opponent)
                    snapshot.attacks += entry.attackStarts;
        } else
            snapshot.attacks += attacking && !previousAttack;
        previousBlock = blocking;
        previousBash = bashing;
        previousAttack = attacking;
        const auto health = Value(subject, RE::ActorValue::kHealth);
        engagementMetrics.Observe(snapshot.elapsed, delta, subject->GetPosition().GetDistance(enemy->GetPosition()),
                                  std::max(previousHealth - health, 0.0f), hitCounts.outgoing > 0, defense.threat,
                                  subject->GetPositionX(), subject->GetPositionY(), enemy->GetPositionX(),
                                  enemy->GetPositionY(), enemy->GetAngleZ(),
                                  defense.threat == plan.enemy && defense.threatVisible &&
                                      (defense.threatAttacking || defense.threatPreparing) && defense.threatKind >= 4 &&
                                      enemy->GetActorRuntimeData().currentCombatTarget.get().get() == subject);
        engagementMetrics.ObserveFlight(delta, defense.flightReadable != 0, defense.projectileGap != 0,
                                        defense.incomingProjectiles != 0);
        snapshot.damage += std::max(previousHealth - health, 0.0f);
        previousHealth = health;
        if (HealingEncounter()) {
            const auto playerHealth = Value(player, RE::ActorValue::kHealth);
            const auto maximum = MaximumHealth(player);
            const auto injuryTarget = mod::combat_tests::healer::InjuryTarget(maximum);
            if (player->IsDead() || player->GetCurrentScene() || injuryTarget <= 0 || !std::isfinite(playerHealth)) {
                Stop(Phase::Failed, "Player state or health changed; Party pressure invalidated.");
                return;
            }
            snapshot.playerHealthGain += std::max(playerHealth - previousPlayerHealth, 0.0f);
            snapshot.injuredSeconds += playerHealth < maximum * 0.7f ? delta : 0;
            previousPlayerHealth = Value(player, RE::ActorValue::kHealth);
        }
        behaviorTimes[state.behavior[0] ? state.behavior : "No role status"] += delta;
        WriteSample(subject, enemy, "running", state.behavior);
        if (testSession.context.completion) {
            const auto& completion = *testSession.context.completion;
            Stop(completion.verdict == mod::combat_tests::testing::Verdict::Failed ? Phase::Failed : Phase::Complete,
                 completion.reason.c_str());
        } else if (suiteActive ? testSession.current.Done() : snapshot.elapsed >= snapshot.duration) {
            if (NaturalEncounter())
                encounterOutcome = natural::Resolve(true, false, false, true);
            Stop(Phase::Complete, "Duration reached.");
        }
    }

    std::uint32_t Configure(const Host* value) {
        std::scoped_lock lock(mutex);
        if (!value || value->size < sizeof(Host) || value->observationSize != sizeof(DefenderObservation) ||
            value->version != Version || !value->inspect || !value->setRole || !value->observe ||
            !value->beginDefense || !value->endDefense || !value->defenseState || !value->attackRequest ||
            !value->attackAnimation || !value->beginDirect || !value->requestDirect || !value->setFollowing ||
            !value->commandState || !value->setAttackCommitment || !value->setDefensiveBash || !value->bashRequest ||
            !value->bashAnimation || !value->combatPackage)
            return 0;
        host = *value;
        return 1;
    }

    std::uint32_t Read(Snapshot* output) {
        std::scoped_lock lock(mutex);
        if (!output || output->size < sizeof(*output))
            return 0;
        *output = snapshot;
        output->interference = interference::Read();
        output->bypassInterruptions = bypassInterruptions;
        output->und = interference::und::Read();
        output->bypassDodging = bypassDodging;
        output->experiments = experiments;
        output->tuning = active ? snapshot.tuning : tuningProfile;
        std::snprintf(output->tuningStatus, sizeof(output->tuningStatus), "%s", tuningStatus.c_str());
        std::snprintf(output->opponent, sizeof(output->opponent), "%s", natural::Description().c_str());
        output->dodgeTelemetry = mod::combat_tests::telemetry::Read();
        const auto blocked = mod::test_rules::BlockStart(ready && host.inspect, active || spell_diagnostic::Busy(),
                                                         unsupported, plan.subject != 0);
        output->canStart = blocked == mod::test_rules::StartBlock::None;
        const auto arena = travel::Read();
        if (output->canStart && (!arena.atArena || arena.pending)) {
            output->canStart = 0;
            std::snprintf(output->startBlockedReason, sizeof(output->startBlockedReason), "%s",
                          arena.pending ? "Finish arena travel before starting a test."
                                        : "Enter the arena before starting a test.");
        }
        output->recoveryPending = plan.subject != 0;
        if (mod::combat_tests::observer_position::Pending()) {
            output->canStart = 0;
            std::snprintf(output->startBlockedReason, sizeof(output->startBlockedReason),
                          "Observer position restoration remains unconfirmed.");
        }
        output->canRecheckRecovery = mod::test_rules::CanRecheckRecovery(
            ready, active, unsupported, plan.subject != 0, restoredForReview, beginPending || recheckRequested);
        if (blocked != mod::test_rules::StartBlock::None)
            std::snprintf(output->startBlockedReason, sizeof(output->startBlockedReason), "%s",
                          mod::test_rules::StartReason(blocked));
        if (unsupported) {
            output->phase = Phase::Failed;
            std::snprintf(output->status, sizeof(output->status),
                          "Unsupported or invalid recovery record retained; new tests are disabled.");
        }
        return 1;
    }

    std::uint32_t StartRun(std::uint32_t actor, std::uint32_t encounter) {
        std::scoped_lock lock(mutex);
        if (mod::combat_tests::observer_position::Pending()) {
            Status("Observer return remains unconfirmed; no teleport was replayed.");
            return 0;
        }
        if (travel::NeedsUpdate()) {
            Status("Finish arena travel before starting a test.");
            return 0;
        }
        const auto blocked = mod::test_rules::BlockStart(ready && host.inspect, active || spell_diagnostic::Busy(),
                                                         unsupported, plan.subject != 0);
        if (blocked != mod::test_rules::StartBlock::None) {
            Status(mod::test_rules::StartReason(blocked));
            return 0;
        }
        if (!actor || !mod::test_rules::ValidEncounter(encounter)) {
            Status("Select a follower and a valid encounter before starting.");
            return 0;
        }
        selectedEncounter = encounter;
        if (currentRun == 1) {
            const auto closed = testSession.Close();
            if (closed.state != mod::combat_tests::testing::State::Ready) {
                Status(closed.reason.c_str());
                return 0;
            }
            if (suiteActive) {
                testSession.context.execute = ExecuteTestCommand;
                if (!testSession.Begin(*mod::test_rules::Scenarios[encounter].suite, tuningProfile)) {
                    Status("Test suite definition or resource state is invalid.");
                    return 0;
                }
            }
        }
        snapshot = {};
        encounterOutcome = natural::Outcome::NotMeasured;
        snapshot.experiments = experiments;
        if (mod::test_rules::IsExecution(encounter, mod::test_rules::Execution::Healer))
            snapshot.experiments.flags &= DodgeBudget;
        if (!mod::test_rules::IsExecution(encounter, mod::test_rules::Execution::Healer) &&
            !mod::test_rules::DirectEncounter(encounter))
            snapshot.experiments.flags |= NativeBash | NativePowerBash;
        snapshot.tuning = tuningProfile;
        const auto scheduled = SuitePlan().At(currentRun);

        if (mod::test_rules::DirectEncounter(encounter)) {
            const auto kind = PrimitiveKind();
            if (!kind) {
                Status("Primitive case action requirements are missing.");
                return 0;
            }
            snapshot.experiments.flags =
                mod::combat_tests::primitive::UsesBash(*kind)
                    ? NativeBash | (*kind == mod::combat_tests::primitive::Kind::PowerBash ? NativePowerBash : 0)
                    : 0;
        }
        metricTracker = {};
        snapshot.subject = requestedActor = actor;
        interference::Reset();
        interference::und::Reset();
        interference::external_attacks::Begin(0, 0, nullptr);
        mod::combat_tests::telemetry::Reset();
        snapshot.duration = mod::test_rules::DirectEncounter(encounter) ? 20
                            : SuiteCase()                               ? SuiteCase()->Seconds()
                                                                        : mod::test_rules::Duration;
        snapshot.suite = defenderBatch;
        snapshot.suiteRepeats = suiteRepeats;
        snapshot.suiteSelection = suiteSelection;
        snapshot.expandVariations = suiteExpandVariations;
        fixtureSteps = 0;
        lastDefense = {};
        tacticalMetrics = {};
        fixtureDriftSamples = 0;
        if (currentRun == 1)
            seriesAllyHealth = 0;
        engagementMetrics = {};
        startupCombat = {};
        comparisonSetup = {};
        fixtureSpellsRemoved = fixtureSpellPasses = unexpectedFixtureSpells = 0;
        fixtureSpellChanges.clear();
        snapshot.phase = Phase::Preparing;
        snapshot.mode = mod::test_rules::Control(encounter);
        snapshot.encounter = encounter;
        if (NaturalEncounter())
            natural::BeginRun();
        snapshot.active = 1;
        snapshot.bypassInterruptions = bypassInterruptions;
        snapshot.bypassDodging = bypassDodging;
        snapshot.run = currentRun;
        snapshot.requestedRuns = requestedRuns;
        snapshot.finishedRuns = static_cast<std::uint32_t>(results.size());
        ticket = ++session;
        testSession.context.subject = actor;
        casePreparationStarted = false;
        participantActors.Reset();
        if (suiteActive &&
            !testSession.BeginCase(SuiteCase()->id, scheduled.repetition, ticket,
                                   scheduled.parameterized ? std::optional{scheduled.variant} : std::nullopt)) {
            Status("Case definition or previous restoration is unavailable.");
            return 0;
        }
        if (suiteActive) {
            if (const auto* tuning = testSession.context.Settings<DefenderTuning>())
                snapshot.tuning = *tuning;
            else {
                Status("Suite configuration type is invalid.");
                return 0;
            }
        }
        initialized = abortRequested = beginPending = beginIssued = false;
        configured = normalized = false;
        fixtureSettling = {};
        restoreCombatStopped = false;
        defenseBegun = false;
        restoredForReview = false;
        recheckRequested = false;
        recoverableDefeat = allyDefeated = false;
        defeatRecoveryElapsed = 0;
        previousBlock = previousBash = previousAttack = false;
        behaviorTimes.clear();
        hitCounts = {};
        outcome = Phase::Complete;
        phaseTime = 0;
        startupWait = 0;
        stopReason.clear();
        if (currentRun == 1) {
            reportFile.clear();
            if (auto directory = SKSE::log::log_directory()) {
                std::error_code error;
                const auto folder = *directory / "YLIWF" / "CombatTests";
                std::filesystem::create_directories(folder, error);
                const auto name = "YLIWF-CombatTest-" +
                                  std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".csv";
                const auto path = folder / name;
                csv.open(path);
                if (csv) {
                    reportFile = (std::filesystem::path("YLIWF") / "CombatTests" / name).string();
                    csv << "run,elapsed,subject_health,subject_stamina,blocking,bashing,opponent_attacking,opponent_"
                           "health,"
                           "phase,phase_elapsed,opponent_weapon,subject_attacking,subject_attack_state,opponent_attack_"
                           "state,"
                           "subject_staggered,subject_recoil,shield_equipped,defender_available,defender_requested,"
                           "style_"
                           "owned,style_yielded,"
                           "stance,combat_style,live_combat_style,threat,distance,heading_degrees,threat_targets_"
                           "subject,"
                           "incoming_melee,"
                           "guard_committed,"
                           "guard_remaining,recovering,counter_opportunity,stamina_max,decision_age,incoming_hit_"
                           "events,"
                           "blocked_hit_events,"
                           "power_hit_events,bash_hit_events,outgoing_hit_events,defender_reason,action_hook,control_"
                           "mode,"
                           "guard_phase,action_fault,"
                           "action_requests,attack_requests,attacks_rejected,engine_rejected,guard_requests,guard_"
                           "accepted,"
                           "guard_observed,"
                           "release_requests,release_accepted,release_observed,action_timeouts,last_action,action_"
                           "status,"
                           "guard_rejected,dispatch_outcome,process_result,process_returned,process_flags,validation_"
                           "mask,block_requests,bash_requests,unknown_left_requests,guard_event,guard_suspended,"
                           "block_readable,wants_block,start_recovering,start_stamina_fraction,release_rejected,"
                           "encounter,"
                           "injury_waves,"
                           "ally_healing_effects,self_healing_effects,sampled_player_health_gain,player_injured_"
                           "seconds,"
                           "player_health,subject_magicka,player_distance,role_behavior,"
                           "fnpb_bypass,fnpb_observer_armed,fnpb_block_stops,fnpb_bash_stops,fnpb_suppressed,"
                           "und_bypass,und_armed,und_checks,und_exclusions,und_attempts,und_denied,"
                           "dodge_readable,dodge_flags,dodge_sink_attached,graph_events,dodge_events,guard_events,"
                           "graph_trace_overwritten,dodging_seconds,last_graph_event,"
                           "guard_dodge_deferrals,guard_dodge_interruptions,guard_dodge_resumes,"
                           "graph_request_observer,graph_notify_requests,graph_notify_completed,graph_notify_false,"
                           "graph_notify_suppressed,guard_attack_deferrals,guard_attack_interruptions,guard_attack_"
                           "resumes,"
                           "external_attack_proposals,external_attack_accepted,external_attack_rejected,"
                           "external_reservations_cleared,external_attack_superseded,external_attack_ended_active,"
                           "external_attack_active,release_reaction_deferrals,release_reaction_wait,block_hit_readable,"
                           "block_hit,graph_want_readable,graph_want_block,release_reaction_wait_peak,suite,suite_"
                           "phase,repetition,fixture_steps,recovered_interrupt_timeouts,perma_attack_proposals,"
                           "perma_attack_accepted,perma_attack_rejected,experiment_flags,native_bash_requests,native_"
                           "bash_accepted,"
                           "native_bash_observed,native_bash_completed,native_bash_rejected,native_bash_timeouts,"
                           "native_bash_phase,"
                           "outgoing_bash_hits,low_stamina_seconds,zero_stamina_seconds,dodge_episodes,dodge_net_loss,"
                           "recovery_entries,recovery_exits,recovery_seconds,subject_bleeding_out,subject_dead,subject_"
                           "disabled,"
                           "subject_scene,und_reserve_denied,und_cooldown_denied,und_snapshot_denied,"
                           "power_bash_requests,power_bash_accepted,power_bash_observed,power_bash_completed,"
                           "power_bash_rejected,power_bash_timeouts,external_animation_clears,external_recovery_clears,"
                           "external_rejection_clears,external_handoffs,external_finishing,handoff_attack_rejections,"
                           "native_bash_interrupted,power_bash_interrupted,bash_graph_starts,bash_graph_ends,"
                           "bash_native_starts,right_attack_requests,right_attack_accepted,right_attack_observed,"
                           "right_attack_completed,right_attack_interrupted,right_attack_rejected,right_attack_"
                           "timeouts,"
                           "right_attack_phase,attack_graph_starts,attack_graph_ends,attack_native_starts,attack_"
                           "recovery_completions,tactical_intent,tactical_motion,threat_kind,threat_count,"
                           "moving,movement_requests,movement_failures,precision_api,precision_callbacks,weapon_"
                           "capsule,"
                           "precision_incoming_cumulative,precision_outgoing_cumulative,movement_distance,ally_hit_"
                           "events,subject_x,subject_y,subject_z,opponent_x,opponent_y,opponent_z,destination_x,"
                           "destination_y,destination_z,"
                           "movement_progress,movement_wait,movement_stalls_cumulative,movement_detours_cumulative,"
                           "movement_status,guard_preparing,"
                           "terminal_inactivity,fixture_friendly_hits,opponent_crossfire,opponent_casts,opponent_cast_"
                           "state,opponent_current_spell,"
                           "opponent_can_cast,opponent_cannot_cast_reason,opponent_magicka,guard_locomotion_speed,"
                           "guard_locomotion_ready,threat_visible,threat_attacking,threat_preparing,tuning_revision,"
                           "tuning_name";
                    for (const auto& field : TuningFields)
                        csv << ",tuning_" << field.key;
                    csv << ",tuning_challenge_enabled";
                    csv << ",fixture_spells_removed,fixture_spell_passes,fixture_unexpected_spells,opponent_left_cast_"
                           "state,opponent_left_spell,opponent_level,opponent_base,subject_package,requested_package,"
                           "subject_target,opponent_target,observer_target_rejections,subject_ai_enabled,opponent_ai_"
                           "enabled,subject_in_combat,opponent_in_combat,subject_combat_ignoring,subject_combat_"
                           "inactive,"
                           "subject_combat_stopped,und_recovery_denied,ally_initial_health,ally_health,ally_sampled_"
                           "damage,ally_target_exposure,ally_first_intervention,ally_defeated,friendly_target_"
                           "rejections,subject_hostile_to_opponent,opponent_hostile_to_subject,opponent_ghost,"
                           "taunt_available,taunt_phase,taunt_enemy,taunt_requests,taunt_observed,taunt_unobserved,"
                           "taunt_released,taunt_cancelled,taunt_target_rejections,taunt_effect_present,taunt_"
                           "remaining,"
                           "taunt_cooldown,precision_pre_callbacks,precision_pre_outgoing_cumulative,precision_last_"
                           "candidate,"
                           "precision_subject_active,precision_threat_active,precision_threat_controller_hittable,"
                           "taunt_hit_events,fixture_health_protected,fixture_damage_suppressed,fixture_damage_"
                           "amount,fixture_public_health_writes,fixture_combat_health_writes,fixture_other_health_"
                           "writes,first_melee_reach,first_outgoing_contact,outside_melee_reach,damage_outside_reach,"
                           "casting_pressure,estimated_aim_line_exposure,estimated_aim_line_exits,selected_target_"
                           "switches,observer_free_camera,auxiliary_target,caster_bash_requests,caster_bash_completed,"
                           "fixture_resource_drift_samples";
                    for (const auto* prefix : {"opponent", "auxiliary"})
                        for (const auto* stat :
                             {"maximum_health", "maximum_stamina", "maximum_magicka", "heal_rate", "stamina_rate",
                              "magicka_rate", "heal_rate_mult", "stamina_rate_mult", "magicka_rate_mult"})
                            csv << ',' << prefix << '_' << stat;
                    csv << ",observer_positioned,opponent_health_damage_modifier,auxiliary_health_damage_modifier,"
                           "flight_readable,projectile_gap,incoming_projectiles,projectiles_examined,"
                           "evade_available,evade_phase,evade_requests,evade_accepted,evade_observed,evade_completed,"
                           "evade_rejected,evade_timeouts,evade_displacement,startup_ally_ready,"
                           "startup_opponent_hostile_to_ally,startup_opponent_target,startup_auxiliary,"
                           "test_phase,test_phase_elapsed,test_phase_completed,participants\n";
                }
            }
        }
        if (!csv.is_open()) {
            snapshot.active = 0;
            snapshot.phase = Phase::Failed;
            Status("Could not create the combat report. Check the SKSE log folder before starting.");
            return 0;
        }
        lastTick = Clock::now();
        active = true;
        Status("Start requested; preparing the encounter.");
        return 1;
    }

    std::uint32_t StartSeries(std::uint32_t actor, std::uint32_t encounter, std::uint32_t runs) {
        std::scoped_lock lock(mutex);
        if (runs < 1 || runs > mod::test_rules::MaxRuns || active || spell_diagnostic::Busy() || plan.subject ||
            unsupported || !ready || !host.inspect || travel::NeedsUpdate() || !travel::Read().atArena || !actor ||
            !mod::test_rules::ValidEncounter(encounter))
            return 0;
        suiteActive = true;
        defenderBatch = false;
        requestedRuns = runs;
        currentRun = 1;
        nextRun = false;
        results.clear();
        seriesReport.clear();
        const auto accepted = StartRun(actor, encounter);
        if (accepted)
            mod::combat_tests::RequestMenu(false);
        return accepted;
    }

    std::uint32_t StartDefenderSuite(std::uint32_t actor, mod::combat_tests::testing::RunOptions options) {
        std::scoped_lock lock(mutex);
        const mod::combat_tests::testing::RunPlan run{mod::combat_tests::suite::Catalog, options};
        if (!run.Valid() || active || spell_diagnostic::Busy() || plan.subject || unsupported || !ready ||
            !host.inspect || travel::NeedsUpdate() || !travel::Read().atArena || !actor)
            return 0;
        suiteActive = defenderBatch = true;
        suiteRepeats = options.repeats;
        suiteSelection = options.selection;
        suiteExpandVariations = options.expandVariations;
        requestedRuns = run.Runs();
        currentRun = 1;
        nextRun = false;
        results.clear();
        seriesReport.clear();
        const auto accepted = StartRun(actor, 0);
        if (accepted)
            mod::combat_tests::RequestMenu(false);
        return accepted;
    }

    std::uint32_t StartEncounter(std::uint32_t actor, std::uint32_t encounter) {
        return StartSeries(actor, encounter, 1);
    }

    std::uint32_t SetDodgeBypass(bool enabled) {
        std::scoped_lock lock(mutex);
        if (active || plan.subject || (enabled && interference::und::Read().support != interference::Support::Ready))
            return 0;
        bypassDodging = enabled;
        return 1;
    }

    std::uint32_t SetExperiments(const Experiments* value) {
        std::scoped_lock lock(mutex);
        if (!value || active || plan.subject || (value->flags & ~(DodgeBudget | NativeBash | NativePowerBash)) ||
            ((value->flags & NativePowerBash) && !(value->flags & NativeBash)) ||
            !interference::dodge_budget_rules::Valid(value->reserveFraction, value->dodgeCooldown) ||
            ((value->flags & DodgeBudget) && interference::und::Read().support != interference::Support::Ready))
            return 0;
        experiments = *value;
        experiments.flags |= NativeBash | NativePowerBash;
        return 1;
    }

    bool TuningEditable() {
        return !active && !plan.subject && !unsupported && !travel::NeedsUpdate();
    }

    std::uint32_t SetTuning(const DefenderTuning* value) {
        std::scoped_lock lock(mutex);
        if (!value || !TuningEditable())
            return 0;
        if (const auto* error = TuningError(*value)) {
            tuningStatus = std::string("Invalid profile: ") + error;
            return 0;
        }
        if (tuningRevision == std::numeric_limits<std::uint32_t>::max()) {
            tuningStatus = "Profile revision limit reached.";
            return 0;
        }
        tuningProfile = *value;
        tuningProfile.revision = ++tuningRevision;
        tuningStatus = "Profile applied for the next test series.";
        return 1;
    }

    std::optional<std::filesystem::path> TuningPath() {
        const auto directory = SKSE::log::log_directory();
        return directory ? std::optional{*directory / "YLIWF" / "CombatTests" / "defender-tuning.ini"} : std::nullopt;
    }

    std::uint32_t SaveTuning() {
        std::scoped_lock lock(mutex);
        if (!TuningEditable())
            return 0;
        const auto path = TuningPath();
        std::error_code error;
        if (!path) {
            tuningStatus = "SKSE log directory is unavailable.";
            return 0;
        }
        std::filesystem::create_directories(path->parent_path(), error);
        if (error) {
            tuningStatus = "Could not create the profile directory.";
            return 0;
        }
        auto temporary = *path;
        temporary += ".tmp";
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        file << mod::combat_tests::tuning::Serialize(tuningProfile);
        file.close();
        if (!file ||
            !MoveFileExW(temporary.c_str(), path->c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            std::filesystem::remove(temporary, error);
            tuningStatus = "Could not save the profile; the previous file was retained.";
            return 0;
        }
        tuningStatus = "Saved SKSE/YLIWF/CombatTests/defender-tuning.ini.";
        return 1;
    }

    std::uint32_t ReloadTuning() {
        std::scoped_lock lock(mutex);
        if (!TuningEditable())
            return 0;
        const auto path = TuningPath();
        std::ifstream file;
        if (path)
            file.open(*path, std::ios::binary);
        if (!file) {
            tuningStatus = "Could not open SKSE/YLIWF/CombatTests/defender-tuning.ini.";
            return 0;
        }
        std::array<char, 8193> text{};
        file.read(text.data(), static_cast<std::streamsize>(text.size()));
        if (file.bad()) {
            tuningStatus = "Could not read the profile; the applied profile was retained.";
            return 0;
        }
        const auto profile =
            mod::combat_tests::tuning::Parse(std::string_view(text.data(), static_cast<std::size_t>(file.gcount())));
        if (!profile) {
            tuningStatus = "Invalid profile file; the applied profile was retained.";
            return 0;
        }
        return SetTuning(&*profile);
    }

    std::uint32_t SetInterruptionBypass(bool enabled) {
        std::scoped_lock lock(mutex);
        if (active || plan.subject || (enabled && interference::Read().support != interference::Support::Ready))
            return 0;
        bypassInterruptions = enabled;
        return 1;
    }

    std::uint32_t Start(std::uint32_t actor) {
        return StartEncounter(actor, 0);
    }

    std::uint32_t RecheckRecovery() {
        std::scoped_lock lock(mutex);
        if (!mod::test_rules::CanRecheckRecovery(ready, active, unsupported, plan.subject != 0, restoredForReview,
                                                 beginPending || recheckRequested))
            return 0;
        recheckRequested = true;
        Status("Recovery recheck requested; verifying without replaying commands.");
        mod::combat_tests::RequestMenu(false);
        return 1;
    }

    void Abort() {
        std::scoped_lock lock(mutex);
        if (active) {
            abortRequested = true;
            if (outcome != Phase::Failed)
                outcome = Phase::Aborted;
            if (stopReason.empty())
                stopReason = "User requested Abort.";
            Status("Abort requested; restoring temporary changes.");
            mod::combat_tests::RequestMenu(false);
        }
    }

    bool Current(RE::StaticFunctionTag*, std::int32_t id) {
        std::scoped_lock lock(mutex);
        return mod::test_rules::CurrentTicket(static_cast<std::uint32_t>(id), ticket, ready, active,
                                              abortRequested || !stopReason.empty());
    }

    void Reset() {
        testSession.ResetForLoad();
        participantActors.Reset();
        mod::combat_tests::observer_position::Reset();
        mod::combat_tests::fixture_protection::Reset();
        spell_diagnostic::Reset();
        observer_control::End();
        scene_control::Reset();
        natural::Reset();
        interference::Reset();
        bypassInterruptions = false;
        interference::und::Reset();
        bypassDodging = false;
        experiments = {};
        if (interference::und::Read().support == interference::Support::Ready)
            experiments.flags |= DodgeBudget;
        metricTracker = {};
        interference::external_attacks::End();
        mod::combat_tests::telemetry::Reset();
        ++session;
        active = false;
        plan = {};
        snapshot = {};
        mod::combat_tests::fixture_bases::Reset();
        initialized = abortRequested = beginPending = beginIssued = unsupported = false;
        configured = normalized = false;
        fixtureSettling = {};
        recoverableDefeat = allyDefeated = false;
        defeatRecoveryElapsed = 0;
        originalRecords.clear();
        defenseBegun = false;
        restoredForReview = false;
        recheckRequested = false;
        reportFile.clear();
        seriesReport.clear();
        results.clear();
        nextRun = travelMenuPending = suiteActive = defenderBatch = false;

        requestedRuns = currentRun = 1;
        mod::combat_tests::ClearMenuRequest();
        hitCounts = {};
        csv.close();
        behaviorTimes.clear();
        stopReason.clear();
        travel::Reset();
    }

    void Save(SKSE::SerializationInterface* serialization) {
        std::scoped_lock lock(mutex);
        travel::Save(serialization);
        if (suiteActive)
            return;
        if (unsupported) {
            for (const auto& record : originalRecords)
                serialization->WriteRecord(record.type, record.version, record.bytes.data(),
                                           static_cast<std::uint32_t>(record.bytes.size()));
        } else if (plan.subject) {
            if (plan.encounter != 1 && host.defenseState)
                plan.guardPhase = host.defenseState(plan.subject, plan.generation);
            if (!serialization->WriteRecord(PlanRecord, 5, &plan, sizeof(plan)))
                SKSE::log::error("Combat tests: could not save the active restore plan; retain the pre-test save.");
        }
    }

    void Load(SKSE::SerializationInterface* serialization) {
        std::scoped_lock lock(mutex);
        Reset();
        std::uint32_t type, version, length;
        std::uint32_t recordCount = 0;
        while (serialization->GetNextRecordInfo(type, version, length)) {
            if (length > 4096 || ++recordCount > 8) {
                unsupported = true;
                continue;
            }
            Record record{type, version, std::vector<std::uint8_t>(length)};
            if (serialization->ReadRecordData(record.bytes.data(), length) != length) {
                unsupported = true;
                continue;
            }
            if (type == travel::Record) {
                travel::Load(serialization, version, record.bytes);
                unsupported = unsupported || travel::InvalidRecord();
                continue;
            }
            originalRecords.push_back(std::move(record));
        }
        if (originalRecords.empty())
            return;
        const auto& record = originalRecords.front();
        const auto decoded = mod::test_rules::DecodePlan(record.version, record.bytes);
        if (unsupported || originalRecords.size() != 1 || record.type != PlanRecord || !decoded) {
            unsupported = true;
            Status("Unsupported recovery record retained; combat tests are disabled.");
            return;
        }
        plan = *decoded;
        if (!mod::test_rules::Valid(plan)) {
            unsupported = true;
            plan = {};
            return;
        }
        // Saved Papyrus continuations must never share the new session's ticket,
        // including when loading a save in a freshly started Skyrim process.
        session = std::max(session.load(), plan.generation) + 1;
        for (auto* id : {&plan.subject, &plan.enemy, &plan.enemyBase, &plan.origin, &plan.auxiliary}) {
            if (*id) {
                std::uint32_t resolved = 0;
                if (!serialization->ResolveFormID(*id, resolved)) {
                    unsupported = true;
                    plan = {};
                    break;
                }
                *id = resolved;
            }
        }
    }

    class HitSink final : public RE::BSTEventSink<RE::TESHitEvent> {
        RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* event,
                                              RE::BSTEventSource<RE::TESHitEvent>*) override {
            if (!event || !active || !event->target || !event->cause)
                return RE::BSEventNotifyControl::kContinue;
            const auto target = event->target->GetFormID(), cause = event->cause->GetFormID();
            const auto flags = event->flags;
            const auto source = event->source;
            const auto generation = session.load();
            if (auto* tasks = SKSE::GetTaskInterface())
                tasks->AddTask([target, cause, flags, source, generation] {
                    std::scoped_lock lock(mutex);
                    if (generation != session.load() || !active || snapshot.phase != Phase::Running)
                        return;
                    if (source && source == lastDefense.tauntSpell) {
                        ++tacticalMetrics.tauntHitEvents;
                        return;
                    }
                    if (suiteActive) {
                        auto& registry = participantActors.participants;
                        const auto participantTarget =
                            NaturalEncounter() ? natural::Participant(target, plan.subject, plan.enemy) : target;
                        const auto participantCause =
                            NaturalEncounter() ? natural::Participant(cause, plan.subject, plan.enemy) : cause;
                        if (HealingEncounter() && participantTarget == 0x14 && participantCause == plan.subject)
                            return;
                        auto* victim = registry.Find(participantTarget);
                        auto* attacker = registry.Find(participantCause);
                        if (victim)
                            ++victim->incoming;
                        if (attacker)
                            ++attacker->outgoing;
                        if (registry.Opposed(participantCause, participantTarget, plan.subject)) {
                            if (participantTarget == plan.subject) {
                                ++hitCounts.incoming;
                                hitCounts.blocked += flags.any(RE::TESHitEvent::Flag::kHitBlocked);
                                hitCounts.power += flags.any(RE::TESHitEvent::Flag::kPowerAttack);
                                hitCounts.bash += flags.any(RE::TESHitEvent::Flag::kBashAttack);
                            } else if (participantCause == plan.subject) {
                                ++hitCounts.outgoing;
                                hitCounts.outgoingBash += flags.any(RE::TESHitEvent::Flag::kBashAttack);
                                if (tacticalMetrics.firstIntervention < 0)
                                    tacticalMetrics.firstIntervention = snapshot.elapsed;
                            } else if (victim && victim->spec.role == mod::combat_tests::testing::ActorRole::Ally)
                                ++tacticalMetrics.allyHits;
                        } else if ((victim || target == plan.subject) && (attacker || cause == plan.subject))
                            ++tacticalMetrics.friendlyHits;
                        else if (victim || attacker || target == plan.subject || cause == plan.subject)
                            Stop(Phase::Failed, "A foreign actor hit or was hit by a test participant.");
                        return;
                    }
                    const auto kind = mod::test_rules::ClassifyFixtureHit(
                        plan.subject, plan.enemy, plan.auxiliary, AllyEncounter(),
                        NaturalEncounter() ? natural::Participant(target, plan.subject, plan.enemy) : target,
                        NaturalEncounter() ? natural::Participant(cause, plan.subject, plan.enemy) : cause);
                    using mod::test_rules::HitKind;
                    if (kind == HitKind::AllyIncoming) {
                        ++tacticalMetrics.allyHits;
                        return;
                    }
                    if (kind == HitKind::AllyOutgoing)
                        return;
                    if (kind == HitKind::Friendly) {
                        ++tacticalMetrics.friendlyHits;
                        return;
                    }
                    if (kind == HitKind::Crossfire) {
                        ++tacticalMetrics.crossfire;
                        return;
                    }
                    if (kind == mod::test_rules::HitKind::Incoming) {
                        ++hitCounts.incoming;
                        hitCounts.blocked += flags.any(RE::TESHitEvent::Flag::kHitBlocked);
                        hitCounts.power += flags.any(RE::TESHitEvent::Flag::kPowerAttack);
                        hitCounts.bash += flags.any(RE::TESHitEvent::Flag::kBashAttack);
                    } else if (kind == mod::test_rules::HitKind::Outgoing) {
                        ++hitCounts.outgoing;
                        if (AllyEncounter() && tacticalMetrics.firstIntervention < 0)
                            tacticalMetrics.firstIntervention = snapshot.elapsed;
                        hitCounts.outgoingBash += flags.any(RE::TESHitEvent::Flag::kBashAttack);
                    } else if (kind == mod::test_rules::HitKind::Foreign) {
                        char reason[192];
                        std::snprintf(reason, sizeof(reason),
                                      "Unexpected hit: cause %08X target %08X; subject %08X enemy %08X auxiliary %08X.",
                                      cause, target, plan.subject, plan.enemy, plan.auxiliary);
                        Stop(Phase::Failed, reason);
                    }
                });
            return RE::BSEventNotifyControl::kContinue;
        }
    } hitSink;

    class SpellSink final : public RE::BSTEventSink<RE::TESSpellCastEvent> {
        RE::BSEventNotifyControl ProcessEvent(const RE::TESSpellCastEvent* event,
                                              RE::BSTEventSource<RE::TESSpellCastEvent>*) override {
            if (!event || !event->object || !active)
                return RE::BSEventNotifyControl::kContinue;
            const auto actor = event->object->GetFormID(), spell = event->spell, generation = session.load();
            if (auto* tasks = SKSE::GetTaskInterface())
                tasks->AddTask([actor, spell, generation] {
                    std::scoped_lock lock(mutex);
                    if (!active || generation != session.load() || actor != plan.enemy ||
                        (!NaturalEncounter() && OpponentLoadout() < 3) ||
                        (snapshot.phase != Phase::Countdown && snapshot.phase != Phase::Running))
                        return;
                    if (NaturalEncounter() || spell == FixtureItem(OpponentLoadout()))
                        ++tacticalMetrics.casts;
                });
            return RE::BSEventNotifyControl::kContinue;
        }
    } spellSink;

    class HealingSink final : public RE::BSTEventSink<RE::TESMagicEffectApplyEvent> {
        RE::BSEventNotifyControl ProcessEvent(const RE::TESMagicEffectApplyEvent* event,
                                              RE::BSTEventSource<RE::TESMagicEffectApplyEvent>*) override {
            if (!event || !active || !event->target || !event->caster)
                return RE::BSEventNotifyControl::kContinue;
            const auto target = event->target->GetFormID(), caster = event->caster->GetFormID(),
                       effect = event->magicEffect;
            const auto generation = session.load();
            if (auto* tasks = SKSE::GetTaskInterface())
                tasks->AddTask([generation, target, caster, effect] {
                    std::scoped_lock lock(mutex);
                    if (generation != session.load() || !active || !HealingEncounter() ||
                        snapshot.phase != Phase::Running || caster != plan.subject)
                        return;
                    auto* base = RE::TESForm::LookupByID<RE::EffectSetting>(effect);
                    if (!base || base->IsHostile() || base->IsDetrimental() ||
                        (base->data.primaryAV != RE::ActorValue::kHealth &&
                         base->data.secondaryAV != RE::ActorValue::kHealth))
                        return;
                    if (target == 0x14) {
                        ++snapshot.healingEffects;
                        if (snapshot.firstHeal < 0)
                            snapshot.firstHeal = snapshot.elapsed;
                    } else if (target == plan.subject)
                        ++snapshot.selfHealingEffects;
                });
            return RE::BSEventNotifyControl::kContinue;
        }
    } healingSink;

    void OnMessage(SKSE::MessagingInterface::Message* message) {
        std::scoped_lock lock(mutex);
        if (message->type == SKSE::MessagingInterface::kPostLoad) {
            const auto module = GetModuleHandleA(mod::info::DllFile);
            const auto queryHost =
                module
                    ? reinterpret_cast<yliwf::testing::QueryHost>(GetProcAddress(module, yliwf::testing::QueryExport))
                    : nullptr;
            const auto queryMenu =
                module ? reinterpret_cast<yliwf::menu::QueryAPI>(GetProcAddress(module, yliwf::menu::QueryExport))
                       : nullptr;
            if (!queryHost || !Configure(queryHost(yliwf::testing::Version)) || !queryMenu ||
                !scene_control::Register(host) || !mod::combat_tests::RegisterMenu(queryMenu(yliwf::menu::Version)))
                SKSE::log::warn("Combat tests: compatible host services or menu registration unavailable");
            mod::combat_tests::RegisterMenuControl();
            return;
        }
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            mod::combat_tests::fixture_bases::Configure();
            mod::combat_tests::fixture_spells::Configure();
            observer_control::Install();
            scene_control::Install();
            mod::combat_tests::fixture_protection::Install();
            interference::Install();
            interference::und::Install();
            if (auto* source = RE::ScriptEventSourceHolder::GetSingleton()) {
                source->AddEventSink<RE::TESSpellCastEvent>(&spellSink);
                source->AddEventSink<RE::TESHitEvent>(&hitSink);
                source->AddEventSink<RE::TESMagicEffectApplyEvent>(&healingSink);
            }
            return;
        }
        if (message->type == SKSE::MessagingInterface::kPreLoadGame) {
            testSession.ResetForLoad();
            participantActors.Reset();
            mod::combat_tests::observer_position::End();
            mod::combat_tests::observer_position::Reset();
            mod::combat_tests::fixture_protection::Reset();
            spell_diagnostic::Reset();
            observer_control::End();
            scene_control::Reset();
            interference::End();
            interference::und::End();
            interference::external_attacks::End();
            mod::combat_tests::telemetry::End();
            if (active)
                WriteSeriesReport(false, "Interrupted by save/load; remaining repeats cancelled.");
            mod::combat_tests::ClearMenuRequest();
            travelMenuPending = false;
            ready = false;
            ++session;
            active = false;
            csv.close();
        } else if (message->type == SKSE::MessagingInterface::kPostLoadGame ||
                   message->type == SKSE::MessagingInterface::kNewGame) {
            if (message->type == SKSE::MessagingInterface::kPostLoadGame && !message->data)
                return;
            ready = true;
            if (plan.subject && !unsupported) {
                ticket = ++session;
                snapshot = {};
                snapshot.subject = plan.subject;
                snapshot.phase = Phase::Restoring;
                if (suiteActive) {
                    const auto pending = participantActors.Pending();
                    if (pending.state != mod::combat_tests::testing::State::Ready) {
                        Status(pending.reason.c_str());
                        return;
                    }
                    if (!restoreCombatStopped)
                        participantActors.StopCombat();
                }
                snapshot.mode = plan.mode;
                snapshot.encounter = plan.encounter;
                snapshot.active = 1;
                initialized = true;
                beginPending = false;
                restoreCombatStopped = false;
                outcome = Phase::Aborted;
                stopReason = "Interrupted by save/load; combat was not resumed.";
                active = true;
                lastTick = Clock::now();
                Status("Interrupted test found; restoring instead of restarting combat.");
            }
        }
    }
}

const yliwf::testing::API* mod::combat_tests::RunnerAPI() {
    static const yliwf::testing::API api{
        sizeof(api),
        yliwf::testing::Version,
        Configure,
        Read,
        Start,
        Abort,
        RecheckRecovery,
        [](travel::Snapshot* output) -> std::uint32_t {
            std::scoped_lock lock(mutex);
            if (!output || output->size < sizeof(*output))
                return 0;
            *output = travel::Read();
            if (!ready || active || spell_diagnostic::Busy() || unsupported || plan.subject || recheckRequested)
                output->canEnter = output->canReturn = 0;
            return 1;
        },
        [](bool returning) -> std::uint32_t {
            std::scoped_lock lock(mutex);
            const bool accepted = ready && !active && !spell_diagnostic::Busy() && !unsupported && !plan.subject &&
                                  !recheckRequested && travel::Request(returning);
            if (accepted) {
                travelMenuPending = true;
                mod::combat_tests::RequestMenu(false);
            }
            return accepted;
        },
        StartEncounter,
        StartSeries,
        [](std::uint32_t index, RunResult* output) -> std::uint32_t {
            std::scoped_lock lock(mutex);
            if (!output || index >= results.size())
                return 0;
            *output = results[index];
            return 1;
        },
        SetInterruptionBypass,
        SetDodgeBypass,
        StartDefenderSuite,
        SetExperiments,
        SetTuning,
        SaveTuning,
        ReloadTuning,
        []() -> std::uint32_t {
            std::scoped_lock lock(mutex);
            if (!TuningEditable())
                return 0;
            natural::Reroll();
            SKSE::log::info("Combat tests: opponent selection cleared; no actor spawned or deleted");
            return 1;
        },
        [](std::uint32_t actor) -> std::uint32_t {
            std::scoped_lock lock(mutex);
            if (!ready || active || plan.subject || unsupported || travel::NeedsUpdate() || !travel::Read().atArena)
                return 0;
            const bool accepted = spell_diagnostic::Request(actor);
            if (accepted)
                mod::combat_tests::RequestMenu(false);
            return accepted;
        },
        [](spell_diagnostic::Snapshot* output) -> std::uint32_t {
            std::scoped_lock lock(mutex);
            if (!output || output->size < sizeof(*output))
                return 0;
            *output = spell_diagnostic::Read();
            return 1;
        },
        []() {
            std::scoped_lock lock(mutex);
            spell_diagnostic::Abort();
            mod::combat_tests::RequestMenu(false);
        }};
    return &api;
}

extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);
    auto* messages = SKSE::GetMessagingInterface();
    auto* serialization = SKSE::GetSerializationInterface();
    auto* papyrus = SKSE::GetPapyrusInterface();
    if (!messages || !serialization || !papyrus || !messages->RegisterListener(OnMessage))
        return false;
    serialization->SetUniqueID(SerializationID);
    serialization->SetSaveCallback(Save);
    serialization->SetLoadCallback(Load);
    serialization->SetRevertCallback([](auto*) {
        std::scoped_lock lock(mutex);
        Reset();
    });
    papyrus->Register([](RE::BSScript::IVirtualMachine* vm) {
        vm->RegisterFunction("IsCurrent", "YLIWF_CombatTests", Current);
        return true;
    });
    static std::jthread worker([](std::stop_token stop) {
        std::uint32_t idleTicks = 0;
        while (!stop.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (!ready)
                continue;
            if (!active && !spell_diagnostic::Busy() && !recheckRequested && !travel::NeedsUpdate() &&
                ++idleTicks % 10 != 0)
                continue;
            if (tickQueued.exchange(true))
                continue;
            if (auto* tasks = SKSE::GetTaskInterface()) {
                const auto generation = session.load();
                tasks->AddTask([generation] {
                    if (generation == session.load())
                        Tick();
                    tickQueued = false;
                });
            } else
                tickQueued = false;
        }
    });
    return true;
}
