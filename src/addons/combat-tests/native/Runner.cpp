#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include "Runner.h"
#include "PrimitiveScenario.h"
#include "TacticalMetrics.h"
#include "interference/ExternalAttacks.h"
#include "interference/DodgeBudgetRules.h"
#include "Menu.h"
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

namespace {
    using namespace yliwf::testing;
    using mod::test_rules::Plan;
    namespace travel = mod::combat_tests::travel;
    namespace interference = mod::combat_tests::interference;
    using Clock = std::chrono::steady_clock;
    static_assert(mod::addon::RequiredAdapterAPIVersion == yliwf::testing::Version);
    constexpr char ArenaPlugin[] = "YouLeadIWillFollow.CombatArena.esp";
    constexpr std::uint32_t PowerBashIdle = 0x820, PowerAttackRoot = 0x13384;
    constexpr std::array<RE::FormID, 6> FixtureItems{0x12EB7, 0x1359D, 0x13985, 0x12FD0, 0x12FCD, 0x1C789};
    constexpr char OpponentName[] = "YLIWF Combat Test Opponent";
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
    bool restoreCombatStopped = false;
    bool defenseBegun = false;
    bool restoredForReview = false;
    bool recoverableDefeat = false;
    float defeatRecoveryElapsed = 0;
    bool previousBlock = false, previousBash = false, previousAttack = false, unsupported = false;
    Phase outcome = Phase::Complete;
    float phaseTime = 0, previousHealth = 0, enemyHealth = 0;
    float previousPlayerHealth = 0, nextInjury = 0;
    RE::TESNPC* runtimeBase = nullptr;
    std::array<RE::TESNPC*, 6> fixtureBases{};
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
    bool defenderSuite = false;
    std::uint32_t suiteRepeats = mod::combat_tests::suite::DefaultRepeats;
    std::uint32_t suiteSelection = mod::combat_tests::suite::AllPhases;
    std::uint32_t fixtureSteps = 0;
    DefenderObservation lastDefense;
    mod::combat_tests::tactics::Metrics tacticalMetrics;
    mod::combat_tests::primitive::Scenario primitiveScenario;
    Experiments experiments;
    mod::combat_tests::metrics::Tracker metricTracker;

    const mod::combat_tests::suite::Phase* SuitePhase() {
        return defenderSuite ? mod::combat_tests::suite::ForRun(currentRun, suiteRepeats, suiteSelection) : nullptr;
    }

    void PositionOpponent(RE::Actor* enemy, RE::Actor* subject, bool initial) {
        const auto* phase = SuitePhase();
        if (!phase)
            return;
        auto position = subject->GetPosition();
        if (initial && (phase->fixture == mod::combat_tests::suite::Fixture::Approach ||
                        phase->fixture == mod::combat_tests::suite::Fixture::Archer ||
                        phase->fixture == mod::combat_tests::suite::Fixture::Projectile)) {
            position.y += 1500;
            enemy->SetPosition(position, true);
        } else if (!initial && phase->fixture == mod::combat_tests::suite::Fixture::Reposition) {
            const float offset = fixtureSteps == 0 ? -220.0f : 220.0f;
            position.x = std::clamp(position.x + std::cos(subject->GetAngleZ()) * offset, -1800.0f, 256.0f);
            position.y = std::clamp(position.y - std::sin(subject->GetAngleZ()) * offset, -1800.0f, 1800.0f);
            enemy->SetPosition(position, true);
        }
    }

    void ApplySuiteFixture(RE::Actor* subject, RE::Actor* enemy) {
        const auto* phase = SuitePhase();
        if (!phase)
            return;
        using mod::combat_tests::suite::Fixture;
        const bool low = phase->fixture == Fixture::LowStamina &&
                         mod::combat_tests::suite::StepDue(phase->fixture, fixtureSteps, snapshot.elapsed);
        const bool wave = phase->fixture == Fixture::RecoveryWaves &&
                          mod::combat_tests::suite::StepDue(phase->fixture, fixtureSteps, snapshot.elapsed);
        if (low || wave) {
            const auto maximum =
                subject->AsActorValueOwner()->GetPermanentActorValue(RE::ActorValue::kStamina) +
                subject->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kStamina);
            const auto current = subject->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina);
            if (!std::isfinite(maximum) || maximum <= 0 || !std::isfinite(current))
                return;
            const float target = std::max(0.0f, maximum * 0.15f);
            if (current > target)
                subject->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage,
                                                                RE::ActorValue::kStamina, target - current);
            ++fixtureSteps;
        }
        if (phase->fixture == Fixture::Reposition &&
            mod::combat_tests::suite::StepDue(phase->fixture, fixtureSteps, snapshot.elapsed)) {
            PositionOpponent(enemy, subject, false);
            ++fixtureSteps;
        }
    }

    bool bypassInterruptions = false;
    bool bypassDodging = false;
    Clock::time_point lastTick;

    std::uint32_t OpponentLoadout();

    std::uint32_t StartRun(std::uint32_t actor, std::uint32_t encounter, std::uint32_t mode);

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

    void RestoreValue(RE::Actor* actor, RE::ActorValue value, float target) {
        actor->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, value,
                                                      target - Value(actor, value));
    }

    void Status(const char* text) {
        std::snprintf(snapshot.status, sizeof(snapshot.status), "%s", text);
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
        if (defenderSuite)
            output << "Suite phase,Repetition,Verdict,";
        else if (mod::test_rules::DirectEncounter(snapshot.encounter))
            output << "Verdict,";
        output << "Run,Result,Seconds,Health lost,Opponent attacks,Block starts,Bash starts,Incoming hits,Blocked "
                  "hits,Ally heals,Damage per second,Dodge episodes,Dodge net loss,Recovery entries,Recovery exits\n";
        for (std::size_t i = 0; i < results.size(); ++i) {
            const auto& row = results[i];
            if (defenderSuite) {
                const auto run = static_cast<std::uint32_t>(i + 1);
                output << mod::combat_tests::suite::ForRun(run, suiteRepeats, suiteSelection)->name << ','
                       << mod::combat_tests::suite::Repetition(run, suiteRepeats) << ','
                       << mod::combat_tests::suite::Name(row.verdict) << ',';
            } else if (mod::test_rules::DirectEncounter(snapshot.encounter))
                output << mod::combat_tests::suite::Name(row.verdict) << ',';
            output << i + 1 << ','
                   << (row.phase == Phase::Complete  ? "Complete"
                       : row.phase == Phase::Aborted ? "Aborted"
                                                     : "Failed")
                   << ',' << row.elapsed << ',' << row.damage << ',' << row.attacks << ',' << row.blocks << ','
                   << row.bashes << ',' << row.incoming << ',' << row.blocked << ',' << row.healing << ','
                   << (row.metrics.seconds > 0 ? row.metrics.damage / row.metrics.seconds : 0) << ','
                   << row.metrics.dodgeEpisodes << ',' << row.metrics.dodgeNetLoss << ',' << row.metrics.recoveryEntries
                   << ',' << row.metrics.recoveryExits << '\n';
        }
        if (defenderSuite) {
            using namespace mod::combat_tests::suite;
            output << "\nCOMBINED DEFENDER SUITE - " << suiteRepeats << " repetitions per phase\n";
            for (std::uint32_t phase = 0; phase < Phases.size(); ++phase) {
                if (!(suiteSelection & (1u << phase)))
                    continue;
                std::uint32_t passed = 0, failed = 0, inconclusive = 0, completed = 0;
                float damage = 0;
                std::uint32_t incoming = 0, blocked = 0;
                for (std::size_t i = 0; i < results.size(); ++i) {
                    if (PhaseIndex(static_cast<std::uint32_t>(i + 1), suiteRepeats, suiteSelection) != phase)
                        continue;
                    ++completed;
                    passed += results[i].verdict == Verdict::Passed;
                    failed += results[i].verdict == Verdict::Failed;
                    inconclusive += results[i].verdict == Verdict::Inconclusive;
                    damage += results[i].damage;
                    incoming += results[i].incoming;
                    blocked += results[i].blocked;
                }
                output << Phases[phase].name << ": " << completed << "/" << suiteRepeats << "; passed " << passed
                       << "; failed " << failed << "; inconclusive " << inconclusive;
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
                   << "\nThis is a functional/coverage verdict, not proof of superiority over Styles only.\n";
        }
        output << seriesReport;
        if (!output)
            SKSE::log::error("Combat tests: report could not be written to {}", path.string());
        std::snprintf(snapshot.reportPath, sizeof(snapshot.reportPath), "%s", path.string().c_str());
    }

    void WriteSample(RE::Actor* subject, RE::Actor* enemy, const char* phase, const char* behavior = "") {
        if (!csv)
            return;
        std::int32_t castState = -1, castable = -1, cannotCast = -1;
        RE::FormID currentSpell = 0;
        if (enemy && enemy->Is3DLoaded() && OpponentLoadout() >= 3) {
            auto* caster = enemy->GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand);
            auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(FixtureItems[OpponentLoadout()]);
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
        if (host.observe)
            host.observe(subject->GetFormID(), &observation);
        mod::combat_tests::telemetry::Sample(subject, snapshot.elapsed, phase, observation);
        if (std::string_view(phase) == "running" || std::string_view(phase) == "stopping") {
            const auto dodge = mod::combat_tests::telemetry::Read();
            metricTracker.Observe({snapshot.elapsed, Value(subject, RE::ActorValue::kStamina),
                                   observation.maximumStamina, Value(subject, RE::ActorValue::kHealth),
                                   (dodge.readable & 1) && (dodge.flags & 1), subject->IsAttacking(),
                                   observation.recovering != 0, hitCounts.incoming, hitCounts.outgoing, fixtureSteps,
                                   (dodge.readable & 1) != 0});
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
            << ',' << observation.releaseWaitPeak << ',' << defenderSuite << ','
            << (defenderSuite ? mod::combat_tests::suite::PhaseIndex(currentRun, suiteRepeats, suiteSelection) + 1 : 0)
            << ',' << (defenderSuite ? mod::combat_tests::suite::Repetition(currentRun, suiteRepeats) : currentRun)
            << ',' << fixtureSteps << ',' << observation.recoveredInterruptTimeouts << ','
            << externalAttack.permaProposals << ',' << externalAttack.permaAccepted << ','
            << externalAttack.permaRejected << ',' << snapshot.experiments.flags << ','
            << observation.nativeBashRequests << ',' << observation.nativeBashAccepted << ','
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
            << '\n';
    }

    bool Dynamic(const RE::TESForm* form) {
        return form && (form->GetFormID() & 0xFF000000) == 0xFF000000;
    }

    bool OwnedEnemy(RE::Actor* actor) {
        const auto* base = actor ? actor->GetActorBase() : nullptr;
        return base && Dynamic(actor) && Dynamic(base) && base->GetFormID() == plan.enemyBase && base->GetName() &&
               std::strcmp(base->GetName(), OpponentName) == 0;
    }

    mod::combat_tests::suite::Fixture Fixture() {
        return static_cast<mod::combat_tests::suite::Fixture>(plan.fixture);
    }

    std::uint32_t OpponentLoadout() {
        return static_cast<std::uint32_t>(mod::combat_tests::suite::Equipment(Fixture()));
    }

    bool OpponentEquipped(RE::Actor* enemy) {
        const auto loadout = OpponentLoadout();

        const auto* item = enemy ? enemy->GetEquippedObject(false) : nullptr;
        return loadout < FixtureItems.size() && item && item->GetFormID() == FixtureItems[loadout];
    }

    bool ReadSubject(Subject& state) {
        return host.inspect && host.inspect(plan.subject ? plan.subject : requestedActor, &state) && state.managed;
    }

    bool DirectEncounter() {
        return mod::test_rules::DirectEncounter(snapshot.encounter);
    }

    bool HealingEncounter() {
        return snapshot.encounter == static_cast<std::uint32_t>(mod::test_rules::Encounter::PartyPressure);
    }

    float MaximumHealth(RE::Actor* actor) {
        return actor->AsActorValueOwner()->GetPermanentActorValue(RE::ActorValue::kHealth) +
               actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth);
    }

    bool Paused() {
        auto* ui = RE::UI::GetSingleton();
        const auto* main = RE::Main::GetSingleton();
        return !ui || ui->GameIsPaused() || !main || main->freezeTime;
    }

    void Finish(Phase result, const char* detail) {
        snapshot.phase = result;
        snapshot.active = 0;
        active = false;
        Status(detail);
        metricTracker.Finish();
        snapshot.metrics = metricTracker.Read();
        std::ostringstream report;
        DefenderObservation defense = lastDefense;

        if (const auto* phase = SuitePhase())
            report << "Suite phase: " << phase->name << "; repetition "
                   << mod::combat_tests::suite::Repetition(currentRun, suiteRepeats) << "/" << suiteRepeats
                   << "; fixture steps " << fixtureSteps << "\n";
        if (DirectEncounter()) {
            const auto verdict = result != Phase::Complete ? "Failed"
                                 : primitiveScenario.Result() == mod::combat_tests::primitive::Step::Passed
                                     ? "Passed"
                                     : "Inconclusive";
            report << detail << "\nDirect primitive: " << mod::test_rules::Scenarios[snapshot.encounter].name
                   << "\nVerdict: " << verdict << "\n"
                   << primitiveScenario.Reason() << "\nAutomatic role; defender threat/stamina policy bypassed.";
            if (snapshot.encounter == 5) {
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
            report << "\nControl mode " << snapshot.mode << "; native validation hook " << defense.actionAvailable
                   << "; action requests " << defense.actionRequests << "; offensive requests "
                   << defense.attackRequests << "; rejected by YLIWF " << defense.attacksRejected
                   << "; validation rejected " << defense.engineRejected
                   << "\nGuard requests/accepted/observed: " << defense.guardRequests << '/' << defense.guardAccepted
                   << '/' << defense.guardObserved << "; releases: " << defense.releaseRequests << '/'
                   << defense.releaseAccepted << '/' << defense.releaseObserved
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
                report
                    << "\nGuard mode incomplete: requests suspended after an unobserved start; remaining combat uses "
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
        if (snapshot.bypassDodging && !dodges.exclusions && !dodges.denied)
            report << "\nNo UND decision was overridden; this run does not demonstrate an intervention.";
        report << "\nDodge budget decisions: allowed " << dodges.budgetAllowed << "; reserve denied "
               << dodges.reserveDenied << "; cooldown denied " << dodges.cooldownDenied << "; snapshot unavailable "
               << dodges.snapshotDenied << "; estimated maximum dodge cost " << dodges.estimatedCost << "; reserve "
               << dodges.reserve;
        interference::und::End();
        interference::external_attacks::End();
        report << interference::external_attacks::Report();
        mod::combat_tests::telemetry::End();
        report << mod::combat_tests::telemetry::Report(false);
        if (plan.subject)
            report << "\nRecovery references: subject " << std::hex << plan.subject << "; origin " << plan.origin
                   << "; opponent " << plan.enemy << std::dec;
        if (!HealingEncounter() && !snapshot.attacks)
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
                                : primitiveScenario.Result() == mod::combat_tests::primitive::Step::Passed
                                    ? mod::combat_tests::suite::Verdict::Passed
                                    : mod::combat_tests::suite::Verdict::Inconclusive;
        if (defenderSuite) {
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
                fixtureSteps == ExpectedSteps(SuitePhase()->fixture),
                defense.recoveredInterruptTimeouts,
                (snapshot.experiments.flags & NativeBash) != 0 && BashCoverage(SuitePhase()->fixture),
                (snapshot.experiments.flags & DodgeBudget) != 0 && BashCoverage(SuitePhase()->fixture),
                defense.nativeBashCompleted,
                dodges.budgetAllowed + dodges.reserveDenied + dodges.cooldownDenied};
            resultRow.verdict = Assess(evidence);
            const char* tacticalMissing = nullptr;
            if (mod::combat_tests::tactics::Tactical(SuitePhase()->fixture)) {
                tacticalMissing = mod::combat_tests::tactics::Missing(SuitePhase()->fixture, tacticalMetrics);
                if (SuitePhase()->fixture == Fixture::Archer && !snapshot.attacks)
                    tacticalMissing =
                        "The archer never began an attack; approach was exercised without ranged pressure.";
                resultRow.verdict = AssessTactical(evidence, tacticalMissing);
            }
            resultRow.powerHits = hitCounts.power;
            resultRow.dodges = mod::combat_tests::telemetry::Read().dodgeEvents;
            resultRow.attackHandoffs = interference::external_attacks::Read().accepted;
            resultRow.reactionWaits = defense.releaseDeferrals;
            report << "\nFunctional verdict: " << Name(resultRow.verdict) << "; "
                   << (mod::combat_tests::tactics::Tactical(SuitePhase()->fixture)
                           ? TacticalReason(evidence, tacticalMissing)
                           : Reason(evidence));
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
            report << "\nMovement controller cumulative stalls/detours: " << defense.movementStalls << '/'
                   << defense.movementDetours << "; final status " << defense.movementStatus << "; progress "
                   << defense.movementProgress << "; guard preparation pause " << defense.guardPreparing;
            report << "\nPrecision: API " << defense.precisionAvailable << "; callback observer "
                   << defense.precisionCallbacks << "; weapon capsule " << defense.weaponReach << "; incoming contacts "
                   << defense.precisionIncoming << "; outgoing contacts " << defense.precisionOutgoing
                   << ". Contacts are separate from TESHitEvent and damage totals."
                   << "\nExecution: Skyrim/SCAR ordinary offense; native guard/bash in the selected test scope; "
                      "navmesh-projected movement."
                   << " Deliberate UND dodge request and guard-specific magic protection are unavailable; spacing is "
                      "the fallback.";
            std::snprintf(snapshot.report, sizeof(snapshot.report), "%s", report.str().c_str());
        }
        resultRow.phase = result;
        resultRow.elapsed = snapshot.elapsed;
        resultRow.damage = snapshot.damage;
        resultRow.attacks = snapshot.attacks;
        resultRow.blocks = snapshot.blocks;
        resultRow.bashes = snapshot.bashes;
        resultRow.incoming = hitCounts.incoming;
        resultRow.blocked = hitCounts.blocked;
        resultRow.healing = snapshot.healingEffects;
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
            mod::test_rules::RepeatableRun(result == Phase::Complete, recoverableDefeat, abortRequested), !plan.subject,
            defenderSuite ? mod::combat_tests::suite::Runs(mod::combat_tests::suite::MaxRepeats)
                          : mod::test_rules::MaxRuns);
        csv.flush();
        WriteSeriesReport(nextRun);
        if (nextRun) {
            active = true;
            snapshot.active = 1;
            Status("Run restored; preparing the next repeat.");
        } else {
            csv.close();
            mod::combat_tests::RequestMenu(true);
        }
    }

    void Restore() {
        snapshot.phase = Phase::Restoring;
        auto* subject = Actor(plan.subject);
        auto* enemy = Actor(plan.enemy);
        auto* auxiliary = Actor(plan.auxiliary);
        if (!restoreCombatStopped) {
            if (OwnedEnemy(auxiliary))
                auxiliary->StopCombat();
            if (OwnedEnemy(enemy))
                enemy->StopCombat();
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
        const auto defenseEnd = plan.encounter == 1 ? 1
                                : host.endDefense   ? host.endDefense(plan.subject, plan.generation, plan.guardPhase)
                                                    : 2;
        if (!defenseEnd) {
            Status("Waiting for the owned guard to release; no command is replayed.");
            return;
        }
        const bool defenseFault = defenseEnd == 2;
        if (!defenseFault)
            plan.guardPhase = 0;
        const bool unverifiedEnemy = (enemy && !OwnedEnemy(enemy)) || (auxiliary && !OwnedEnemy(auxiliary));
        if (OwnedEnemy(auxiliary)) {
            auxiliary->StopCombat();
            auxiliary->Disable();
            auxiliary->SetDelete(true);
            plan.auxiliary = 0;
        }
        if (OwnedEnemy(enemy)) {
            enemy->StopCombat();
            enemy->Disable();
            enemy->SetDelete(true);
            plan.enemy = 0;
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
        bool atOrigin = subject->GetParentCell() == origin->GetParentCell() &&
                        subject->GetPosition().GetDistance(origin->GetPosition()) < 32;
        if (mod::test_rules::RestorePosition(atOrigin, plan.restoreIssued != 0) ==
            mod::test_rules::PositionAction::Move) {
            plan.restoreIssued = 1;
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
                Status("Defeated run failed; resources restored, waiting for bleedout recovery before continuing.");
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
        plan = {};
        Finish(outcome,
               outcome == Phase::Complete  ? "Test complete; position, resources and role restored."
               : outcome == Phase::Aborted ? "Test aborted; temporary changes restored."
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
                if (subject && enemy)
                    WriteSample(subject, enemy, "stopping");
            }
        }
        Status(reason);
        if (beginPending) {
            abortRequested = true;
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
        if (!ReadSubject(state) || !state.allowed || !subject || subject->IsDead() || subject->IsDisabled() ||
            subject->IsInCombat() || subject->GetCurrentScene() || subject->IsOnMount() ||
            subject->AsActorState()->IsUnconscious() || subject->AsActorState()->IsBleedingOut()) {
            Finish(Phase::Failed,
                   state.reason[0] ? state.reason : "Follower must be alive, available and outside combat/scenes.");
            return;
        }
        if (auto* processes = RE::ProcessLists::GetSingleton())
            for (const auto& handle : processes->highActorHandles)
                if (auto other = handle.get(); other && other.get() != player && other.get() != subject &&
                                               !other->IsDead() && !other->IsDisabled() &&
                                               other->GetParentCell() == start->GetParentCell()) {
                    Finish(Phase::Failed, "Other actors are in the arena. Have other followers wait outside.");
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
        plan.fixture = SuitePhase() ? static_cast<std::uint32_t>(SuitePhase()->fixture) : 0;
        plan.wasWaiting = Value(subject, RE::ActorValue::kWaitingForPlayer) != 0;
        plan.playerHealth = Value(player, RE::ActorValue::kHealth);
        plan.playerChanged = HealingEncounter() ? 1 : 0;
        if (HealingEncounter() &&
            (player->IsDead() || player->GetCurrentScene() || !std::isfinite(plan.playerHealth) ||
             plan.playerHealth <= 0 || mod::test_rules::InjuryTarget(MaximumHealth(player)) <= 0)) {
            plan = {};
            Finish(Phase::Failed, "Player must be alive, outside scenes and have valid health for Party pressure.");
            return;
        }
        if (!mod::test_rules::Valid(plan)) {
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
                                      !HealingEncounter() && (snapshot.experiments.flags & DodgeBudget),
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
        phaseTime = 0;
        Status("Summoning follower; waiting for loading and Follow completion.");
    }

    void PrepareSubject(float delta) {
        phaseTime += delta;
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
            Stop(Phase::Failed, "Native defense mode unavailable or already owned; use Styles only as the baseline.");
            return;
        }
        defenseBegun = !HealingEncounter();
        auto* movementAnchor = data->LookupForm<RE::TESObjectREFR>(0x811, ArenaPlugin);
        if (defenseBegun &&
            (!movementAnchor || !host.setMovementBounds ||
             !host.setMovementBounds(plan.subject, plan.generation, movementAnchor->GetFormID(), 4096))) {
            Stop(Phase::Failed, "Bounded test movement configuration was rejected.");
            return;
        }
        const bool powerBash = snapshot.encounter == 4 || (snapshot.experiments.flags & NativePowerBash) != 0;
        auto* bashIdle = powerBash && data ? data->LookupForm<RE::TESIdleForm>(PowerBashIdle, ArenaPlugin) : nullptr;
        auto* bashGate = powerBash ? RE::TESForm::LookupByID<RE::TESIdleForm>(PowerAttackRoot) : nullptr;
        if (defenseBegun && (snapshot.experiments.flags & NativeBash) &&
            (!host.setDefensiveBash ||
             !host.setDefensiveBash(plan.subject, plan.generation, powerBash ? 2 : 1,
                                    bashIdle ? bashIdle->GetFormID() : 0, bashGate ? bashGate->GetFormID() : 0))) {
            Stop(Phase::Failed,
                 "Native bash configuration unavailable; use Guard/Combined with matching core, add-on DLL and arena "
                 "ESP.");
            return;
        }
        if (defenseBegun && (plan.mode & 2))
            interference::external_attacks::Begin(plan.subject, plan.generation, host.setAttackCommitment);
        if (defenseBegun && snapshot.encounter == 5) {
            mod::combat_tests::telemetry::ObserveAttacks(plan.subject, plan.generation, host);
            mod::combat_tests::telemetry::Attach(subject);
        }
        if (defenseBegun && (snapshot.experiments.flags & NativeBash)) {
            mod::combat_tests::telemetry::ObserveBashes(plan.subject, plan.generation, host);
            mod::combat_tests::telemetry::Attach(subject);
        }
        for (const auto value : {RE::ActorValue::kHealth, RE::ActorValue::kStamina, RE::ActorValue::kMagicka})
            subject->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, value, 1000000);
        runtimeBase = fixtureBases[OpponentLoadout()];
        if (!runtimeBase) {
            const auto sourceID =
                mod::combat_tests::suite::Template(static_cast<mod::combat_tests::suite::Loadout>(OpponentLoadout()));
            auto* source = data->LookupForm<RE::TESNPC>(sourceID, "Skyrim.esm");
            auto* duplicate = source ? source->CreateDuplicateForm(false, nullptr) : nullptr;
            runtimeBase = duplicate ? duplicate->As<RE::TESNPC>() : nullptr;
            if (runtimeBase) {
                runtimeBase->fullName = OpponentName;
                runtimeBase->actorData.actorBaseFlags.set(RE::ACTOR_BASE_DATA::Flag::kInvulnerable);
                runtimeBase->actorData.templateUseFlags.reset(RE::ACTOR_BASE_DATA::TEMPLATE_USE_FLAG::kSpells);
                // Fixture spells must never edit a copied/template spell array.
                runtimeBase->actorEffects = new RE::TESSpellList::SpellData();
                fixtureBases[OpponentLoadout()] = runtimeBase;
            }
        }
        if (!runtimeBase || !Dynamic(runtimeBase) || !runtimeBase->IsInvulnerable()) {
            Stop(Phase::Failed, "Could not create a private invulnerable opponent base.");
            return;
        }
        auto* templateBase = data->LookupForm<RE::TESNPC>(0xC3CA0, "Skyrim.esm");
        runtimeBase->combatStyle = OpponentLoadout() >= 3   ? data->LookupForm<RE::TESCombatStyle>(0x831, ArenaPlugin)
                                   : OpponentLoadout() == 2 ? data->LookupForm<RE::TESCombatStyle>(0x830, ArenaPlugin)
                                   : templateBase           ? templateBase->combatStyle
                                                            : nullptr;
        plan.enemyBase = runtimeBase->GetFormID();
        const auto spawned = enemyStart->PlaceObjectAtMe(runtimeBase, true);
        auto* enemy = spawned ? spawned->As<RE::Actor>() : nullptr;
        if (!enemy) {
            Stop(Phase::Failed, "Could not spawn the test opponent.");
            return;
        }
        plan.enemy = enemy->GetFormID();
        if (!OwnedEnemy(enemy)) {
            // This reference was created in this call, before publishing it to any other operation.
            enemy->Disable();
            enemy->SetDelete(true);
            plan.enemy = plan.enemyBase = 0;
            Stop(Phase::Failed, "Spawned actor did not retain the private protected base.");
            return;
        }
        if (mod::combat_tests::suite::AdditionalActor(Fixture())) {
            const auto extra = enemyStart->PlaceObjectAtMe(runtimeBase, true);
            auto* auxiliary = extra ? extra->As<RE::Actor>() : nullptr;
            if (!auxiliary || !OwnedEnemy(auxiliary)) {
                if (extra) {
                    extra->Disable();
                    extra->SetDelete(true);
                }
                Stop(Phase::Failed, "Could not create the private additional fixture actor.");
                return;
            }
            plan.auxiliary = auxiliary->GetFormID();
        }
        enemy->AsActorValueOwner()->SetBaseActorValue(RE::ActorValue::kAggression, 0);
        phaseTime = 0;
        Status("Preparing encounter; waiting for actor loading and protection checks.");
    }

    class Begun final : public RE::BSScript::IStackCallbackFunctor {
    public:
        explicit Begun(std::uint32_t value) : expected(value) {}

        void operator()(RE::BSScript::Variable result) override {
            const auto code = result.IsInt() ? result.GetSInt() : -1;
            if (auto* tasks = SKSE::GetTaskInterface())
                tasks->AddTask([id = expected, code] {
                    std::scoped_lock lock(mutex);
                    if (!active || ticket != id)
                        return;
                    beginPending = false;
                    if (abortRequested) {
                        Stop(outcome, "Combat startup cancelled; restoring.");
                        return;
                    }
                    if (code != static_cast<std::int32_t>(mod::test_rules::BeginResult::Accepted)) {
                        const char* reason = "Combat helper returned an unexpected result; install the matching PEX.";
                        switch (static_cast<mod::test_rules::BeginResult>(code)) {
                            case mod::test_rules::BeginResult::Expired:
                                reason = "Combat helper rejected an expired session ticket.";
                                break;
                            case mod::test_rules::BeginResult::MissingSword:
                                reason = "Combat helper could not find the fixture weapon, ammunition or spell.";
                                break;
                            case mod::test_rules::BeginResult::MissingActor:
                                reason = "Combat helper received a missing subject or opponent.";
                                break;
                            default:
                                break;
                        }
                        Stop(Phase::Failed, reason);
                        return;
                    }
                    phaseTime = 0;
                    Status("Combat requested; waiting for the fixture loadout and combat state.");
                });
        }

        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

    private:
        std::uint32_t expected;
    };

    void Tick() {
        std::scoped_lock lock(mutex);
        const auto now = Clock::now();
        const auto delta = mod::test_rules::ActiveDelta(std::chrono::duration<float>(now - lastTick).count(), Paused());
        lastTick = now;
        if (!ready || delta == 0)
            return;
        travel::Update(ready, active || unsupported || plan.subject != 0 || recheckRequested);
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
                mod::combat_tests::RequestMenu(true);
                return;
            }
            active = false;
            ++currentRun;
            if (!StartRun(snapshot.subject, snapshot.encounter, snapshot.mode)) {
                snapshot.active = 0;
                snapshot.phase = Phase::Failed;
                Status("Next repeat could not start; completed results are retained.");
                WriteSeriesReport(false, "Next repeat could not start.");
                csv.close();
                mod::combat_tests::RequestMenu(true);
            }
            return;
        }
        if (!initialized) {
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
        if (abortRequested) {
            Stop(outcome == Phase::Failed ? outcome : Phase::Aborted, "Abort requested.");
            return;
        }
        if (!plan.enemy) {
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
            Stop(Phase::Failed,
                 mod::combat_tests::metrics::Unavailable(
                     subject != nullptr, subject && subject->IsDead(),
                     subject &&
                         (subject->AsActorState()->IsBleedingOut() || Value(subject, RE::ActorValue::kHealth) <= 0),
                     subject && subject->IsDisabled(), subject && subject->GetCurrentScene(), true, true, true));
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
        interference::und::Sample(subject);
        interference::external_attacks::Sample(subject);
        DefenderObservation defense;
        const bool observed = !HealingEncounter() && host.observe && host.observe(plan.subject, &defense);
        if (observed)
            lastDefense = defense;
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
                    other.get() != RE::PlayerCharacter::GetSingleton() && !other->IsDead() && !other->IsDisabled() &&
                    other->GetParentCell() == start->GetParentCell()) {
                    Stop(Phase::Failed, "Another actor entered the arena; controlled run invalidated.");
                    return;
                }
        phaseTime += delta;
        if (snapshot.phase == Phase::Preparing) {
            if (!subject->Is3DLoaded() || !enemy->Is3DLoaded() || subject->GetParentCell() != start->GetParentCell()) {
                if (phaseTime > 15)
                    Stop(Phase::Failed, "Actors did not finish loading into the arena.");
                return;
            }
            if (phaseTime < 2)
                return;
            const float health = Value(enemy, RE::ActorValue::kHealth);
            for (const float damage : {1000000.0f, -1000000.0f}) {
                enemy->HandleHealthDamage(nullptr, damage);
                if (!mod::test_rules::ProtectionPassed(enemy->GetActorBase()->IsInvulnerable(), !enemy->IsDead(),
                                                       health, Value(enemy, RE::ActorValue::kHealth))) {
                    Stop(Phase::Failed, "Opponent damage-protection preflight failed; test was not started.");
                    return;
                }
            }
            enemy->StopCombat();
            subject->StopCombat();
            snapshot.phase = Phase::Countdown;
            phaseTime = 0;
            Status("Protection verified. Test starts in three active seconds.");
            return;
        }
        if (snapshot.phase == Phase::Countdown) {
            if (beginIssued) {
                if (beginPending)
                    return;
                const bool equipped = OpponentEquipped(enemy);
                const bool subjectCombat = subject->IsInCombat();
                const bool enemyCombat = enemy->IsInCombat();
                WriteSample(subject, enemy, "starting");
                const auto decision = mod::test_rules::ObserveStart(phaseTime, equipped, subjectCombat, enemyCombat);
                if (decision == mod::test_rules::StartDecision::Ready) {
                    snapshot.phase = Phase::Running;
                    phaseTime = 0;
                    ApplySuiteFixture(subject, enemy);
                    previousHealth = Value(subject, RE::ActorValue::kHealth);
                    bool dodging = false;
                    const bool dodgeReadable = subject->GetGraphVariableBool("bIsDodging", dodging);
                    metricTracker.Observe({0, Value(subject, RE::ActorValue::kStamina), defense.maximumStamina,
                                           previousHealth, dodging, subject->IsAttacking(), defense.recovering != 0,
                                           hitCounts.incoming, hitCounts.outgoing, fixtureSteps, dodgeReadable});
                    enemyHealth = Value(enemy, RE::ActorValue::kHealth);
                    previousPlayerHealth = Value(RE::PlayerCharacter::GetSingleton(), RE::ActorValue::kHealth);
                    nextInjury = 0;
                    Status("Encounter running. Open the menu to pause; Abort restores temporary changes.");
                } else if (decision == mod::test_rules::StartDecision::Failed) {
                    const auto* weapon = enemy->GetEquippedObject(false);
                    char reason[256];
                    std::snprintf(
                        reason, sizeof(reason),
                        "Combat startup timed out: loadout %s; follower combat %s; opponent combat %s; weapon "
                        "%08X %.64s.",
                        equipped ? "yes" : "no", subjectCombat ? "yes" : "no", enemyCombat ? "yes" : "no",
                        weapon ? weapon->GetFormID() : 0, weapon && weapon->GetName() ? weapon->GetName() : "None");
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
            PositionOpponent(enemy, subject, true);
            auto* auxiliary = Actor(plan.auxiliary);
            if (auxiliary) {
                auxiliary->MoveTo(subject);
                auto position = subject->GetPosition();
                position.x -= 256;
                position.y += Fixture() == mod::combat_tests::suite::Fixture::AllyPressure ? 512 : -256;
                auxiliary->SetPosition(position, true);
            }
            beginIssued = beginPending = true;
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{new Begun(ticket)};
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (!vm || !vm->DispatchStaticCall(
                           "YLIWF_CombatTests", "Begin",
                           RE::MakeFunctionArguments(static_cast<RE::Actor*>(subject), static_cast<RE::Actor*>(enemy),
                                                     static_cast<int>(ticket), static_cast<int>(OpponentLoadout()),
                                                     static_cast<RE::Actor*>(auxiliary),
                                                     Fixture() == mod::combat_tests::suite::Fixture::AllyPressure),
                           callback)) {
                beginPending = false;
                Stop(Phase::Failed, "Combat helper could not be dispatched; install the matching PEX.");
            }
            return;
        }
        snapshot.elapsed += delta;
        ApplySuiteFixture(subject, enemy);
        if (phaseTime > 3 && (!subject->IsInCombat() || !enemy->IsInCombat())) {
            Stop(Phase::Failed, "Combat did not remain active; no combat-start command was replayed.");
            return;
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        const auto target = enemy->GetActorRuntimeData().currentCombatTarget.get();
        const bool allyTarget = Fixture() == mod::combat_tests::suite::Fixture::AllyPressure && target &&
                                target->GetFormID() == plan.auxiliary;
        const bool fixtureTarget = target && plan.auxiliary && target->GetFormID() == plan.auxiliary;
        if (player->IsAttacking() ||
            (enemy->IsAttacking() && target.get() != subject && !allyTarget && !fixtureTarget)) {
            Stop(Phase::Failed,
                 "Player interference or an opponent attack targeting another actor invalidated the run.");
            return;
        }
        if (!OpponentEquipped(enemy) || std::abs(Value(enemy, RE::ActorValue::kWeaponSpeedMult) - 1) > 0.01f) {
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
        if (!enemy->GetActorBase()->IsInvulnerable() || Value(enemy, RE::ActorValue::kHealth) < enemyHealth - 0.01f) {
            Stop(Phase::Failed, "Opponent lost health despite protection; run invalidated.");
            return;
        }
        if (plan.auxiliary) {
            auto* auxiliary = Actor(plan.auxiliary);
            if (!OwnedEnemy(auxiliary) || auxiliary->IsDead() || auxiliary->IsDisabled() || !auxiliary->Is3DLoaded() ||
                !auxiliary->GetActorBase()->IsInvulnerable()) {
                Stop(Phase::Failed, "Additional fixture actor lost availability or protection.");
                return;
            }
        }
        tacticalMetrics.Contacts(hitCounts.incoming, hitCounts.outgoing);
        tacticalMetrics.Observe(delta, defense.intent, defense.threatKind, defense.threatCount, defense.distance,
                                subject->GetPositionX(), subject->GetPositionY());
        tacticalMetrics.powerWindups +=
            defense.threatKind == 2 && enemy->AsActorState()->GetAttackState() == RE::ATTACK_STATE_ENUM::kDraw;
        const bool blocking = subject->IsBlocking();
        const bool bashing = subject->AsActorState()->GetAttackState() == RE::ATTACK_STATE_ENUM::kBash;
        const bool attacking = enemy->IsAttacking();
        snapshot.blocks += blocking && !previousBlock;
        snapshot.bashes += bashing && !previousBash;
        snapshot.attacks += attacking && !previousAttack;
        previousBlock = blocking;
        previousBash = bashing;
        previousAttack = attacking;
        const auto health = Value(subject, RE::ActorValue::kHealth);
        snapshot.damage += std::max(previousHealth - health, 0.0f);
        previousHealth = health;
        if (HealingEncounter()) {
            const auto playerHealth = Value(player, RE::ActorValue::kHealth);
            const auto maximum = MaximumHealth(player);
            const auto injuryTarget = mod::test_rules::InjuryTarget(maximum);
            if (player->IsDead() || player->GetCurrentScene() || injuryTarget <= 0 || !std::isfinite(playerHealth)) {
                Stop(Phase::Failed, "Player state or health changed; Party pressure invalidated.");
                return;
            }
            snapshot.playerHealthGain += std::max(playerHealth - previousPlayerHealth, 0.0f);
            snapshot.injuredSeconds += playerHealth < maximum * 0.7f ? delta : 0;
            if (snapshot.elapsed >= nextInjury && snapshot.injuryWaves < 5) {
                if (playerHealth > injuryTarget) {
                    plan.playerChanged = 1;
                    RestoreValue(player, RE::ActorValue::kHealth, injuryTarget);
                }
                ++snapshot.injuryWaves;
                nextInjury += 12;
            }
            previousPlayerHealth = Value(player, RE::ActorValue::kHealth);
        }
        behaviorTimes[state.behavior[0] ? state.behavior : "No role status"] += delta;
        WriteSample(subject, enemy, "running", state.behavior);
        if (DirectEncounter()) {
            using namespace mod::combat_tests::primitive;
            const auto kind = snapshot.encounter == 2   ? Kind::GuardRelease
                              : snapshot.encounter == 3 ? Kind::Bash
                              : snapshot.encounter == 4 ? Kind::PowerBash
                                                        : Kind::RightAttack;
            const auto step = primitiveScenario.Advance(delta, kind, defense);
            if (step == Step::Guard || step == Step::Release || step == Step::Bash || step == Step::Attack) {
                const auto action = step == Step::Attack      ? PrimitiveAction::RightAttack
                                    : step == Step::Guard     ? PrimitiveAction::Guard
                                    : step == Step::Release   ? PrimitiveAction::Release
                                    : kind == Kind::PowerBash ? PrimitiveAction::PowerBash
                                                              : PrimitiveAction::Bash;
                if (!host.requestDirect(plan.subject, plan.generation, action)) {
                    primitiveScenario.Reject();
                    Stop(Phase::Complete, primitiveScenario.Reason());
                }
            } else if (step == Step::Passed || step == Step::Inconclusive || step == Step::Failed)
                Stop(step == Step::Failed ? Phase::Failed : Phase::Complete, primitiveScenario.Reason());
        } else if (snapshot.elapsed >= snapshot.duration)
            Stop(Phase::Complete, "Duration reached.");
    }

    std::uint32_t Configure(const Host* value) {
        std::scoped_lock lock(mutex);
        if (!value || value->size < sizeof(Host) || value->observationSize != sizeof(DefenderObservation) ||
            value->version != Version || !value->inspect || !value->setRole || !value->observe ||
            !value->beginDefense || !value->endDefense || !value->defenseState || !value->attackRequest ||
            !value->attackAnimation || !value->beginDirect || !value->requestDirect || !value->setFollowing ||
            !value->commandState || !value->setAttackCommitment || !value->setDefensiveBash || !value->bashRequest ||
            !value->bashAnimation)
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
        output->dodgeTelemetry = mod::combat_tests::telemetry::Read();
        const auto blocked = mod::test_rules::BlockStart(ready && host.inspect, active, unsupported, plan.subject != 0);
        output->canStart = blocked == mod::test_rules::StartBlock::None;
        const auto arena = travel::Read();
        if (output->canStart && (!arena.atArena || arena.pending)) {
            output->canStart = 0;
            std::snprintf(output->startBlockedReason, sizeof(output->startBlockedReason), "%s",
                          arena.pending ? "Finish arena travel before starting a test."
                                        : "Enter the arena before starting a test.");
        }
        output->recoveryPending = plan.subject != 0;
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

    std::uint32_t StartRun(std::uint32_t actor, std::uint32_t encounter, std::uint32_t mode) {
        std::scoped_lock lock(mutex);
        if (travel::NeedsUpdate()) {
            Status("Finish arena travel before starting a test.");
            return 0;
        }
        const auto blocked = mod::test_rules::BlockStart(ready && host.inspect, active, unsupported, plan.subject != 0);
        if (blocked != mod::test_rules::StartBlock::None) {
            Status(mod::test_rules::StartReason(blocked));
            return 0;
        }
        if (!actor || !mod::test_rules::ValidConfiguration(encounter, mode)) {
            Status("Select a follower and a valid control mode before starting.");
            return 0;
        }
        if (encounter == 0 && (experiments.flags & NativeBash) && !(mode & 2)) {
            Status("Native defensive bashes require Guard or Combined mode.");
            return 0;
        }
        snapshot = {};
        snapshot.experiments = experiments;
        primitiveScenario = {};
        if (mod::test_rules::DirectEncounter(encounter))
            snapshot.experiments.flags =
                (encounter == 2 || encounter == 5) ? 0 : NativeBash | (encounter == 4 ? NativePowerBash : 0);
        metricTracker = {};
        snapshot.subject = requestedActor = actor;
        interference::Reset();
        interference::und::Reset();
        interference::external_attacks::Begin(0, 0, nullptr);
        mod::combat_tests::telemetry::Reset();
        snapshot.duration = mod::test_rules::DirectEncounter(encounter) ? 20
                            : SuitePhase()                              ? SuitePhase()->seconds
                                                                        : mod::test_rules::Duration;
        snapshot.suite = defenderSuite;
        snapshot.suiteRepeats = suiteRepeats;
        snapshot.suiteSelection = suiteSelection;
        fixtureSteps = 0;
        lastDefense = {};
        tacticalMetrics = {};
        snapshot.phase = Phase::Preparing;
        snapshot.mode = mode;
        snapshot.encounter = encounter;
        snapshot.active = 1;
        snapshot.bypassInterruptions = bypassInterruptions;
        snapshot.bypassDodging = bypassDodging;
        snapshot.run = currentRun;
        snapshot.requestedRuns = requestedRuns;
        snapshot.finishedRuns = static_cast<std::uint32_t>(results.size());
        ticket = ++session;
        initialized = abortRequested = beginPending = beginIssued = false;
        restoreCombatStopped = false;
        defenseBegun = false;
        restoredForReview = false;
        recheckRequested = false;
        recoverableDefeat = false;
        defeatRecoveryElapsed = 0;
        previousBlock = previousBash = previousAttack = false;
        behaviorTimes.clear();
        hitCounts = {};
        outcome = Phase::Complete;
        phaseTime = 0;
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
                           "guard_locomotion_ready,threat_visible,threat_attacking,threat_preparing\n";
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

    std::uint32_t StartSeries(std::uint32_t actor, std::uint32_t encounter, std::uint32_t mode, std::uint32_t runs) {
        std::scoped_lock lock(mutex);
        if (runs < 1 || runs > mod::test_rules::MaxRuns || active || plan.subject || unsupported || !ready ||
            !host.inspect || travel::NeedsUpdate() || !travel::Read().atArena || !actor ||
            !mod::test_rules::ValidConfiguration(encounter, mode))
            return 0;
        defenderSuite = false;
        requestedRuns = runs;
        currentRun = 1;
        nextRun = false;
        results.clear();
        seriesReport.clear();
        const auto accepted = StartRun(actor, encounter, mode);
        if (accepted)
            mod::combat_tests::RequestMenu(false);
        return accepted;
    }

    std::uint32_t StartDefenderSuite(std::uint32_t actor, std::uint32_t repeats, std::uint32_t selection) {
        std::scoped_lock lock(mutex);
        if (!mod::combat_tests::suite::ValidRepeats(repeats) || !mod::combat_tests::suite::ValidSelection(selection) ||
            active || plan.subject || unsupported || !ready || !host.inspect || travel::NeedsUpdate() ||
            !travel::Read().atArena || !actor)
            return 0;
        defenderSuite = true;
        suiteRepeats = repeats;
        suiteSelection = selection;
        requestedRuns = mod::combat_tests::suite::Runs(repeats, selection);
        currentRun = 1;
        nextRun = false;
        results.clear();
        seriesReport.clear();
        const auto accepted = StartRun(actor, 0, 3);
        if (accepted)
            mod::combat_tests::RequestMenu(false);
        return accepted;
    }

    std::uint32_t StartEncounter(std::uint32_t actor, std::uint32_t encounter, std::uint32_t mode) {
        return StartSeries(actor, encounter, mode, 1);
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
        return 1;
    }

    std::uint32_t SetInterruptionBypass(bool enabled) {
        std::scoped_lock lock(mutex);
        if (active || plan.subject || (enabled && interference::Read().support != interference::Support::Ready))
            return 0;
        bypassInterruptions = enabled;
        return 1;
    }

    std::uint32_t Start(std::uint32_t actor) {
        return StartEncounter(actor, 0, 0);
    }

    std::uint32_t StartConfigured(std::uint32_t actor, std::uint32_t mode) {
        return StartEncounter(actor, 0, mode);
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
        return mod::test_rules::CurrentTicket(static_cast<std::uint32_t>(id), ticket, ready, active, abortRequested);
    }

    void Reset() {
        interference::Reset();
        bypassInterruptions = false;
        interference::und::Reset();
        bypassDodging = false;
        experiments = {};
        metricTracker = {};
        interference::external_attacks::End();
        mod::combat_tests::telemetry::Reset();
        ++session;
        active = false;
        plan = {};
        snapshot = {};
        runtimeBase = nullptr;
        fixtureBases = {};
        initialized = abortRequested = beginPending = beginIssued = unsupported = false;
        recoverableDefeat = false;
        defeatRecoveryElapsed = 0;
        originalRecords.clear();
        defenseBegun = false;
        restoredForReview = false;
        recheckRequested = false;
        reportFile.clear();
        seriesReport.clear();
        results.clear();
        nextRun = travelMenuPending = defenderSuite = false;
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
            const auto generation = session.load();
            if (auto* tasks = SKSE::GetTaskInterface())
                tasks->AddTask([target, cause, flags, generation] {
                    std::scoped_lock lock(mutex);
                    if (generation != session.load() || !active || snapshot.phase != Phase::Running)
                        return;
                    const auto kind = mod::test_rules::ClassifyFixtureHit(
                        plan.subject, plan.enemy, plan.auxiliary,
                        Fixture() == mod::combat_tests::suite::Fixture::AllyPressure, target, cause);
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
                    if (!active || generation != session.load() || actor != plan.enemy || OpponentLoadout() < 3 ||
                        (snapshot.phase != Phase::Countdown && snapshot.phase != Phase::Running))
                        return;
                    if (spell == FixtureItems[OpponentLoadout()])
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
                !mod::combat_tests::RegisterMenu(queryMenu(yliwf::menu::Version)))
                SKSE::log::warn("Combat tests: compatible host services or menu registration unavailable");
            mod::combat_tests::RegisterMenuControl();
            return;
        }
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
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
    static const yliwf::testing::API api{sizeof(api),
                                         yliwf::testing::Version,
                                         Configure,
                                         Read,
                                         Start,
                                         Abort,
                                         StartConfigured,
                                         RecheckRecovery,
                                         [](travel::Snapshot* output) -> std::uint32_t {
                                             std::scoped_lock lock(mutex);
                                             if (!output || output->size < sizeof(*output))
                                                 return 0;
                                             *output = travel::Read();
                                             if (!ready || active || unsupported || plan.subject || recheckRequested)
                                                 output->canEnter = output->canReturn = 0;
                                             return 1;
                                         },
                                         [](bool returning) -> std::uint32_t {
                                             std::scoped_lock lock(mutex);
                                             const bool accepted = ready && !active && !unsupported && !plan.subject &&
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
                                         SetExperiments};
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
            if (!active && !recheckRequested && !travel::NeedsUpdate() && ++idleTicks % 10 != 0)
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
