#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include "Runner.h"
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
    using Clock = std::chrono::steady_clock;
    static_assert(mod::addon::RequiredAdapterAPIVersion == yliwf::testing::Version);
    constexpr char ArenaPlugin[] = "YouLeadIWillFollow.CombatArena.esp";
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
    bool previousBlock = false, previousBash = false, previousAttack = false, unsupported = false;
    Phase outcome = Phase::Complete;
    float phaseTime = 0, previousHealth = 0, enemyHealth = 0;
    float previousPlayerHealth = 0, nextInjury = 0;
    RE::TESNPC* runtimeBase = nullptr;
    std::map<std::string, float> behaviorTimes;

    struct HitCounts {
        std::uint32_t incoming = 0, blocked = 0, power = 0, bash = 0, outgoing = 0;
    } hitCounts;

    std::ofstream csv;
    std::string reportFile, stopReason;
    Clock::time_point lastTick;

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

    void WriteSample(RE::Actor* subject, RE::Actor* enemy, const char* phase, const char* behavior = "") {
        if (!csv)
            return;
        const auto* weapon = enemy->GetEquippedObject(false);
        char weaponID[9];
        std::snprintf(weaponID, sizeof(weaponID), "%08X", weapon ? weapon->GetFormID() : 0);
        DefenderObservation observation;
        if (host.observe)
            host.observe(subject->GetFormID(), &observation);
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
        csv << "\"\n";
    }

    bool Dynamic(const RE::TESForm* form) {
        return form && (form->GetFormID() & 0xFF000000) == 0xFF000000;
    }

    bool OwnedEnemy(RE::Actor* actor) {
        const auto* base = actor ? actor->GetActorBase() : nullptr;
        return base && Dynamic(actor) && Dynamic(base) && base->GetFormID() == plan.enemyBase && base->GetName() &&
               std::strcmp(base->GetName(), OpponentName) == 0;
    }

    bool ReadSubject(Subject& state) {
        return host.inspect && host.inspect(plan.subject ? plan.subject : requestedActor, &state) && state.managed;
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
        std::ostringstream report;
        DefenderObservation defense;
        if (defenseBegun && host.observe)
            host.observe(snapshot.subject, &defense);
        if (HealingEncounter()) {
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
                   << "; timeouts: " << defense.actionTimeouts;
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
                   << "; rejected releases " << defense.releaseRejected;
            if (defense.guardSuspended)
                report
                    << "\nGuard mode incomplete: requests suspended after an unobserved start; remaining combat uses "
                       "styles and the selected recovery gate.";
        }
        if (!stopReason.empty())
            report << "\nReason: " << stopReason;
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
        csv.close();
        if (!reportFile.empty()) {
            if (auto directory = SKSE::log::log_directory()) {
                auto path = *directory / reportFile;
                path.replace_extension(".txt");
                std::ofstream textReport(path);
                textReport << report.str() << '\n';
            }
        }
    }

    void Restore() {
        snapshot.phase = Phase::Restoring;
        auto* subject = Actor(plan.subject);
        auto* enemy = Actor(plan.enemy);
        if (!restoreCombatStopped) {
            if (OwnedEnemy(enemy))
                enemy->StopCombat();
            if (subject)
                subject->StopCombat();
            restoreCombatStopped = true;
        }
        if (plan.playerChanged) {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || player->IsDead() || player->GetCurrentScene()) {
                Status("Restoration waiting for the player before restoring captured health.");
                return;
            }
            RestoreValue(player, RE::ActorValue::kHealth, plan.playerHealth);
            plan.playerChanged = 0;
        }
        const auto defenseEnd = plan.encounter != 0 ? 1
                                : host.endDefense   ? host.endDefense(plan.subject, plan.generation, plan.guardPhase)
                                                    : 2;
        if (!defenseEnd) {
            Status("Waiting for the owned guard to release; no command is replayed.");
            return;
        }
        const bool defenseFault = defenseEnd == 2;
        const bool unverifiedEnemy = enemy && !OwnedEnemy(enemy);
        if (OwnedEnemy(enemy)) {
            enemy->StopCombat();
            enemy->Disable();
            enemy->SetDelete(true);
            plan.enemy = 0;
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
        RestoreValue(subject, RE::ActorValue::kHealth, plan.health);
        RestoreValue(subject, RE::ActorValue::kStamina, plan.stamina);
        RestoreValue(subject, RE::ActorValue::kMagicka, plan.magicka);
        if (mod::test_rules::RestoreRole(plan.changedRole != 0, state.role, plan.encounter)) {
            if (!host.setRole || !host.setRole(plan.subject, plan.role)) {
                Status("Original role could not be restored; waiting for eligibility. No role change is replayed.");
                // Do not retry a rejected or uncertain write on later ticks.
                plan.changedRole = 0;
                Finish(Phase::Failed, "Original role needs manual restoration; position/resources restored.");
                return;
            }
        }
        if (unverifiedEnemy) {
            Finish(Phase::Failed,
                   "Opponent ownership is unverified; it was not deleted. Recovery references retained for review.");
            return;
        }
        if (defenseFault) {
            restoredForReview = true;
            plan.guardPhase = host.defenseState ? host.defenseState(plan.subject, plan.generation) : plan.guardPhase;
            Finish(
                Phase::Failed,
                "Position/resources restored; guard release remains unconfirmed. Recovery retained for manual review.");
            return;
        }
        origin->Disable();
        origin->SetDelete(true);
        plan = {};
        Finish(outcome, outcome == Phase::Complete  ? "Test complete; position, resources and role restored."
                        : outcome == Phase::Aborted ? "Test aborted; temporary changes restored."
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
        const auto requiredRole = mod::test_rules::EncounterRole(snapshot.encounter);
        if (!ReadSubject(state) || !state.allowed || !(state.eligibleRoles & (1u << requiredRole)) || !subject ||
            !subject->Is3DLoaded() || subject->IsDead() || subject->IsDisabled() || subject->IsInCombat() ||
            subject->GetCurrentScene() || subject->IsOnMount() || subject->AsActorState()->IsUnconscious() ||
            Value(subject, RE::ActorValue::kWaitingForPlayer) != 0) {
            Finish(Phase::Failed, state.reason[0]
                                      ? state.reason
                                      : "Follower must be loaded, following, outside combat/scenes and available.");
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
        if (state.role != requiredRole) {
            if (!host.setRole || !host.setRole(plan.subject, requiredRole)) {
                Stop(Phase::Failed, "Encounter role assignment was rejected.");
                return;
            }
            plan.changedRole = 1;
        }
        if (!HealingEncounter() &&
            (!host.beginDefense || !host.beginDefense(plan.subject, plan.mode, plan.generation))) {
            Stop(Phase::Failed, "Native defense mode unavailable or already owned; use Styles only as the baseline.");
            return;
        }
        defenseBegun = !HealingEncounter();
        for (const auto value : {RE::ActorValue::kHealth, RE::ActorValue::kStamina, RE::ActorValue::kMagicka})
            subject->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, value, 1000000);
        if (!runtimeBase) {
            auto* source = data->LookupForm<RE::TESNPC>(0xC3CA0, "Skyrim.esm");
            auto* duplicate = source ? source->CreateDuplicateForm(false, nullptr) : nullptr;
            runtimeBase = duplicate ? duplicate->As<RE::TESNPC>() : nullptr;
            if (runtimeBase) {
                runtimeBase->fullName = OpponentName;
                runtimeBase->actorData.actorBaseFlags.set(RE::ACTOR_BASE_DATA::Flag::kInvulnerable);
            }
        }
        if (!runtimeBase || !Dynamic(runtimeBase) || !runtimeBase->IsInvulnerable()) {
            Stop(Phase::Failed, "Could not create a private invulnerable opponent base.");
            return;
        }
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
        enemy->AsActorValueOwner()->SetBaseActorValue(RE::ActorValue::kAggression, 0);
        subject->MoveTo(HealingEncounter() ? Marker(0x813) : start);
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
                                reason = "Combat helper could not find the vanilla Iron Sword.";
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
                    Status("Combat requested; waiting for the equipped sword and both actors to enter combat.");
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
            }
        }
        if (!active)
            return;
        if (!initialized) {
            if (abortRequested)
                Finish(Phase::Aborted, "Test cancelled before setup.");
            else
                Initialize();
            return;
        }
        if (snapshot.phase == Phase::Restoring) {
            Restore();
            return;
        }
        if (abortRequested) {
            Stop(outcome == Phase::Failed ? outcome : Phase::Aborted, "Abort requested.");
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
        if (!subject || subject->IsDead() || subject->AsActorState()->IsBleedingOut() || !OwnedEnemy(enemy) ||
            enemy->IsDead() || enemy->AsActorState()->IsBleedingOut() || !ReadSubject(state) || !state.allowed ||
            state.role != mod::test_rules::EncounterRole(plan.encounter) ||
            !(state.eligibleRoles & (1u << mod::test_rules::EncounterRole(plan.encounter)))) {
            Stop(Phase::Failed, "Actor, equipment, role or quest eligibility changed.");
            return;
        }
        DefenderObservation defense;
        if (!HealingEncounter() && snapshot.phase == Phase::Running && host.observe &&
            host.observe(plan.subject, &defense) && defense.actionFault) {
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
                if (auto other = handle.get(); other && other.get() != subject && other.get() != enemy &&
                                               other.get() != RE::PlayerCharacter::GetSingleton() && !other->IsDead() &&
                                               !other->IsDisabled() &&
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
                auto* data = RE::TESDataHandler::GetSingleton();
                const auto* sword = data->LookupForm<RE::TESObjectWEAP>(0x12EB7, "Skyrim.esm");
                const bool equipped = sword && enemy->GetEquippedObject(false) == sword;
                const bool subjectCombat = subject->IsInCombat();
                const bool enemyCombat = enemy->IsInCombat();
                WriteSample(subject, enemy, "starting");
                const auto decision = mod::test_rules::ObserveStart(phaseTime, equipped, subjectCombat, enemyCombat);
                if (decision == mod::test_rules::StartDecision::Ready) {
                    snapshot.phase = Phase::Running;
                    phaseTime = 0;
                    previousHealth = Value(subject, RE::ActorValue::kHealth);
                    enemyHealth = Value(enemy, RE::ActorValue::kHealth);
                    previousPlayerHealth = Value(RE::PlayerCharacter::GetSingleton(), RE::ActorValue::kHealth);
                    nextInjury = 0;
                    Status("Encounter running. Open the menu to pause; Abort restores temporary changes.");
                } else if (decision == mod::test_rules::StartDecision::Failed) {
                    const auto* weapon = enemy->GetEquippedObject(false);
                    char reason[256];
                    std::snprintf(reason, sizeof(reason),
                                  "Combat startup timed out: sword %s; follower combat %s; opponent combat %s; weapon "
                                  "%08X %.64s.",
                                  equipped ? "yes" : "no", subjectCombat ? "yes" : "no", enemyCombat ? "yes" : "no",
                                  weapon ? weapon->GetFormID() : 0,
                                  weapon && weapon->GetName() ? weapon->GetName() : "None");
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
            beginIssued = beginPending = true;
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{new Begun(ticket)};
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (!vm || !vm->DispatchStaticCall(
                           "YLIWF_CombatTests", "Begin",
                           RE::MakeFunctionArguments(static_cast<RE::Actor*>(subject), static_cast<RE::Actor*>(enemy),
                                                     static_cast<int>(ticket)),
                           callback)) {
                beginPending = false;
                Stop(Phase::Failed, "Combat helper could not be dispatched; install the matching PEX.");
            }
            return;
        }
        snapshot.elapsed += delta;
        if (phaseTime > 3 && (!subject->IsInCombat() || !enemy->IsInCombat())) {
            Stop(Phase::Failed, "Combat did not remain active; no combat-start command was replayed.");
            return;
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        const auto target = enemy->GetActorRuntimeData().currentCombatTarget.get();
        if (player->IsAttacking() || (enemy->IsAttacking() && target.get() != subject)) {
            Stop(Phase::Failed,
                 "Player interference or an opponent attack targeting another actor invalidated the run.");
            return;
        }
        auto* data = RE::TESDataHandler::GetSingleton();
        const auto* sword = data->LookupForm<RE::TESObjectWEAP>(0x12EB7, "Skyrim.esm");
        if (enemy->GetEquippedObject(false) != sword ||
            std::abs(Value(enemy, RE::ActorValue::kWeaponSpeedMult) - 1) > 0.01f) {
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
        if (snapshot.elapsed >= snapshot.duration)
            Stop(Phase::Complete, "Duration reached.");
    }

    std::uint32_t Configure(const Host* value) {
        std::scoped_lock lock(mutex);
        if (!value || value->size < sizeof(Host) || value->version != Version || !value->inspect || !value->setRole ||
            !value->observe || !value->beginDefense || !value->endDefense || !value->defenseState)
            return 0;
        host = *value;
        return 1;
    }

    std::uint32_t Read(Snapshot* output) {
        std::scoped_lock lock(mutex);
        if (!output || output->size < sizeof(*output))
            return 0;
        *output = snapshot;
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

    std::uint32_t StartEncounter(std::uint32_t actor, std::uint32_t encounter, std::uint32_t mode) {
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
        snapshot = {};
        snapshot.subject = requestedActor = actor;
        snapshot.duration = mod::test_rules::Duration;
        snapshot.phase = Phase::Preparing;
        snapshot.mode = mode;
        snapshot.encounter = encounter;
        snapshot.active = 1;
        ticket = ++session;
        initialized = abortRequested = beginPending = beginIssued = false;
        restoreCombatStopped = false;
        defenseBegun = false;
        restoredForReview = false;
        recheckRequested = false;
        previousBlock = previousBash = previousAttack = false;
        behaviorTimes.clear();
        hitCounts = {};
        outcome = Phase::Complete;
        phaseTime = 0;
        reportFile.clear();
        stopReason.clear();
        if (auto directory = SKSE::log::log_directory()) {
            const auto name = "YLIWF-CombatTest-" +
                              std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".csv";
            const auto path = *directory / name;
            csv.open(path);
            if (csv) {
                reportFile = name;
                csv << "elapsed,subject_health,subject_stamina,blocking,bashing,opponent_attacking,opponent_health,"
                       "phase,phase_elapsed,opponent_weapon,subject_attacking,subject_attack_state,opponent_attack_"
                       "state,"
                       "subject_staggered,subject_recoil,shield_equipped,defender_available,defender_requested,style_"
                       "owned,style_yielded,"
                       "stance,combat_style,live_combat_style,threat,distance,heading_degrees,threat_targets_subject,"
                       "incoming_melee,"
                       "guard_committed,"
                       "guard_remaining,recovering,counter_opportunity,stamina_max,decision_age,incoming_hit_events,"
                       "blocked_hit_events,"
                       "power_hit_events,bash_hit_events,outgoing_hit_events,defender_reason,action_hook,control_mode,"
                       "guard_phase,action_fault,"
                       "action_requests,attack_requests,attacks_rejected,engine_rejected,guard_requests,guard_accepted,"
                       "guard_observed,"
                       "release_requests,release_accepted,release_observed,action_timeouts,last_action,action_status,"
                       "guard_rejected,dispatch_outcome,process_result,process_returned,process_flags,validation_"
                       "mask,block_requests,bash_requests,unknown_left_requests,guard_event,guard_suspended,"
                       "block_readable,wants_block,start_recovering,start_stamina_fraction,release_rejected,encounter,"
                       "injury_waves,"
                       "ally_healing_effects,self_healing_effects,sampled_player_health_gain,player_injured_seconds,"
                       "player_health,subject_magicka,player_distance,role_behavior\n";
            }
        }
        lastTick = Clock::now();
        active = true;
        Status("Start requested. Close paused menus to prepare the scenario.");
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
        Status("Recovery recheck requested. Close paused menus to verify without replaying commands.");
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
            Status("Abort requested. Close paused menus to restore temporary changes.");
        }
    }

    bool Current(RE::StaticFunctionTag*, std::int32_t id) {
        std::scoped_lock lock(mutex);
        return mod::test_rules::CurrentTicket(static_cast<std::uint32_t>(id), ticket, ready, active, abortRequested);
    }

    void Reset() {
        ++session;
        active = false;
        plan = {};
        snapshot = {};
        runtimeBase = nullptr;
        initialized = abortRequested = beginPending = beginIssued = unsupported = false;
        originalRecords.clear();
        defenseBegun = false;
        restoredForReview = false;
        recheckRequested = false;
        reportFile.clear();
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
            if (plan.encounter == 0 && host.defenseState)
                plan.guardPhase = host.defenseState(plan.subject, plan.generation);
            if (!serialization->WriteRecord(PlanRecord, 3, &plan, sizeof(plan)))
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
        for (auto* id : {&plan.subject, &plan.enemy, &plan.enemyBase, &plan.origin}) {
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
                    const auto kind = mod::test_rules::ClassifyHit(plan.subject, plan.enemy, target, cause);
                    if (kind == mod::test_rules::HitKind::Incoming) {
                        ++hitCounts.incoming;
                        hitCounts.blocked += flags.any(RE::TESHitEvent::Flag::kHitBlocked);
                        hitCounts.power += flags.any(RE::TESHitEvent::Flag::kPowerAttack);
                        hitCounts.bash += flags.any(RE::TESHitEvent::Flag::kBashAttack);
                    } else if (kind == mod::test_rules::HitKind::Outgoing) {
                        ++hitCounts.outgoing;
                    } else if (kind == mod::test_rules::HitKind::Foreign) {
                        Stop(Phase::Failed, "A third-party hit reached a test actor; run invalidated.");
                    }
                });
            return RE::BSEventNotifyControl::kContinue;
        }
    } hitSink;

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
            return;
        }
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            if (auto* source = RE::ScriptEventSourceHolder::GetSingleton()) {
                source->AddEventSink<RE::TESHitEvent>(&hitSink);
                source->AddEventSink<RE::TESMagicEffectApplyEvent>(&healingSink);
            }
            return;
        }
        if (message->type == SKSE::MessagingInterface::kPreLoadGame) {
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
                                             return ready && !active && !unsupported && !plan.subject &&
                                                    !recheckRequested && travel::Request(returning);
                                         },
                                         StartEncounter};
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
