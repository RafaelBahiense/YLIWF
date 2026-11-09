#include "SpellDiagnostic.h"
#include "SpellDiagnosticRules.h"
#include "../ui/Menu.h"
#include "ModInfo.h"
#include "CombatTestHost.h"
#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace mod::combat_tests::spell_diagnostic {
    namespace {
        constexpr const char* Plugin = "YouLeadIWillFollow.CombatArena.esp";
        Snapshot snapshot;
        std::atomic_bool busy{false};
        std::uint32_t casterID = 0, targetID = 0, originID = 0;
        RE::TESNPC* base = nullptr;
        RE::SpellItem* challenge = nullptr;
        RE::SpellItem* control = nullptr;
        RE::ActorHandle casterHandle;
        bool originalInvulnerable = false, abortRequested = false, cast = false, clearing = false;
        bool casterAICaptured = false, casterAIEnabled = false;
        bool arrived = false, returning = false, returnIssued = false, locationRestored = false;
        std::string pendingResult;
        float clock = 0;
        std::array<spell_diagnostic_rules::Evidence, 4> evidence{};
        std::ostringstream summary, samples;

        RE::Actor* Actor(std::uint32_t id) {
            return id ? RE::TESForm::LookupByID<RE::Actor>(id) : nullptr;
        }

        const char* AvailabilityReason(RE::Actor* caster, RE::PlayerCharacter* player, RE::TESObjectREFR* marker) {
            if (!caster)
                return "Selected caster could not be resolved.";
            if (!player)
                return "Player unavailable.";
            if (!marker)
                return "Arena marker missing.";
            if (caster->IsDead())
                return "Selected caster is dead.";
            if (caster->IsDisabled())
                return "Selected caster is disabled.";
            if (caster->IsOnMount() || caster->AsActorState()->IsUnconscious())
                return "Selected caster is mounted or unconscious.";
            if (!base && caster->IsInCombat())
                return "Selected caster is still in combat.";
            if (!base && caster->IsAttacking())
                return "Selected caster has an active attack.";
            if (caster->GetCurrentScene())
                return "Selected caster is in a scene.";
            if (caster->AsActorState()->IsBleedingOut())
                return "Selected caster is bleeding out.";
            if (player->GetParentCell() != marker->GetParentCell())
                return "Player left the arena.";
            return nullptr;
        }

        void SetInvulnerable(bool enabled) {
            if (!base)
                return;
            if (enabled)
                base->actorData.actorBaseFlags.set(RE::ACTOR_BASE_DATA::Flag::kInvulnerable);
            else
                base->actorData.actorBaseFlags.reset(RE::ACTOR_BASE_DATA::Flag::kInvulnerable);
        }

        bool EffectsCleared(RE::Actor* target) {
            if (!target)
                return true;
            auto* magic = target->AsMagicTarget();
            auto* list = magic->GetActiveEffectList();
            if (list)
                for (auto* effect : *list)
                    if (effect && (effect->spell == challenge || effect->spell == control) &&
                        !effect->flags.any(RE::ActiveEffect::Flag::kInactive, RE::ActiveEffect::Flag::kDispelled))
                        return false;
            return true;
        }

        bool ClearEffects(RE::Actor* target) {
            if (target)
                for (auto* spell : {challenge, control})
                    if (spell)
                        target->AsMagicTarget()->DispelEffect(spell, casterHandle);
            return EffectsCleared(target);
        }

        RE::TESObjectREFR* Origin() {
            auto* origin = originID ? RE::TESForm::LookupByID<RE::TESObjectREFR>(originID) : nullptr;
            return origin && (originID & 0xFF000000) == 0xFF000000 && origin->GetBaseObject() &&
                           origin->GetBaseObject()->GetFormID() == 0x3B
                       ? origin
                       : nullptr;
        }

        void DeleteOrigin() {
            if (auto* origin = Origin()) {
                origin->Disable();
                origin->SetDelete(true);
            }
            originID = 0;
        }

        void RestoreAI() {
            if (casterAICaptured)
                if (auto* caster = Actor(casterID))
                    caster->EnableAI(casterAIEnabled);
            casterAICaptured = false;
        }

        void CleanupTarget() {
            auto* target = Actor(targetID);
            auto* caster = Actor(casterID);
            if (casterAICaptured && caster) {
                const auto combatTarget = caster->GetActorRuntimeData().currentCombatTarget.get();
                if (combatTarget && combatTarget->GetFormID() == targetID)
                    caster->StopCombat();
            }
            if (target && target->GetActorBase() == base) {
                ClearEffects(target);
                target->Disable();
                target->SetDelete(true);
            }
            SetInvulnerable(originalInvulnerable);
            targetID = 0;
            base = nullptr;
        }

        void CompleteReport(const char* reason) {
            summary << "\nResult: " << reason << "\n"
                    << spell_diagnostic_rules::Finding(evidence) << "\n"
                    << "Cleanup: dedicated target disabled/deleted if created; original base protection and caster AI "
                       "restored.\n"
                    << "Caster returned to captured position: " << locationRestored << '\n'
                    << "This diagnostic does not establish combat redirection or taunt effectiveness.\n";
            const auto text = summary.str();
            std::snprintf(snapshot.report, sizeof(snapshot.report), "%s", text.c_str());
            std::snprintf(snapshot.status, sizeof(snapshot.status), "%s", reason);
            if (auto directory = SKSE::log::log_directory()) {
                const auto folder = *directory / "YLIWF" / "CombatTests";
                std::error_code error;
                std::filesystem::create_directories(folder, error);
                const auto name = "YLIWF-SpellDiagnostic-" +
                                  std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".txt";
                std::ofstream file(folder / name);
                if (file) {
                    file << text << "\nSAMPLES\n" << samples.str();
                    file.flush();
                    if (file)
                        std::snprintf(snapshot.path, sizeof(snapshot.path), "%s", (folder / name).string().c_str());
                }
            }
            if (!snapshot.path[0])
                std::snprintf(snapshot.status, sizeof(snapshot.status),
                              "Report file write failed; summary remains below.");
            snapshot.active = 0;
            busy = false;
            base = nullptr;
            targetID = 0;
            RequestMenu(true);
        }

        void Finish(const char* reason) {
            CleanupTarget();
            if (originID) {
                pendingResult = reason;
                returning = true;
                returnIssued = false;
                clock = 0;
                std::snprintf(snapshot.status, sizeof(snapshot.status), "Returning caster to the previous location.");
                return;
            }
            RestoreAI();
            CompleteReport(reason);
        }

        void ReturnCaster(const yliwf::testing::Host& host) {
            auto* caster = Actor(casterID);
            auto* origin = Origin();
            yliwf::testing::Subject permission;
            if (!caster || caster->IsDead() || !origin || caster->GetCurrentScene() || !host.inspect ||
                !host.inspect(casterID, &permission) || !permission.allowed) {
                std::snprintf(snapshot.status, sizeof(snapshot.status),
                              "Return waiting for caster, origin or quest permission; no movement replayed.");
                return;
            }
            const bool atOrigin = caster->GetParentCell() == origin->GetParentCell() &&
                                  caster->GetPosition().GetDistance(origin->GetPosition()) <= 32;
            if (atOrigin) {
                locationRestored = true;
                RestoreAI();
                DeleteOrigin();
                returning = false;
                summary << "Caster origin return verified.\n";
                CompleteReport(pendingResult.c_str());
            } else if (!returnIssued) {
                caster->MoveTo(origin);
                returnIssued = true;
            } else if (clock >= 15)
                std::snprintf(snapshot.status, sizeof(snapshot.status),
                              "Caster return is still unverified; waiting without replaying the move.");
        }

        void Observe(RE::Actor* target, RE::SpellItem* spell) {
            auto& result = evidence[snapshot.completed];
            auto* magic = target->AsMagicTarget();
            auto* expected = spell->effects.empty() ? nullptr : spell->effects[0]->baseEffect;
            const bool has = expected && magic->HasMagicEffect(expected);
            result.hasEffect |= has;
            samples << snapshot.completed + 1 << ',' << clock << ',' << has;
            auto* list = magic->GetActiveEffectList();
            if (list)
                for (auto* effect : *list) {
                    if (!effect)
                        continue;
                    const bool spellMatch = effect->spell == spell;
                    const bool baseMatch = effect->GetBaseObject() == expected;
                    if (!spellMatch && !baseMatch && clock != 0)
                        continue;
                    const auto caster = effect->caster.get();
                    const bool inactive = effect->flags.any(RE::ActiveEffect::Flag::kInactive);
                    const bool dispelled = effect->flags.any(RE::ActiveEffect::Flag::kDispelled);
                    const bool current = !inactive && !dispelled;
                    result.spellEntry |= spellMatch && current;
                    result.baseEntry |= baseMatch && current;
                    result.strictEntry |= spellMatch && caster && caster->GetFormID() == casterID && !inactive;
                    samples << ",spell=" << std::hex << (effect->spell ? effect->spell->GetFormID() : 0)
                            << ",effect=" << (effect->GetBaseObject() ? effect->GetBaseObject()->GetFormID() : 0)
                            << ",caster=" << (caster ? caster->GetFormID() : 0) << std::dec << ",inactive=" << inactive
                            << ",dispelled=" << dispelled << ",duration=" << effect->duration
                            << ",elapsed=" << effect->elapsedSeconds;
                }
            samples << '\n';
        }

        void Advance() {
            SetInvulnerable(originalInvulnerable);
            ++snapshot.completed;
            cast = clearing = false;
            clock = 0;
            if (snapshot.completed == spell_diagnostic_rules::Cases.size())
                Finish("All four application cases completed.");
        }
    }

    bool Busy() {
        return busy.load();
    }

    Snapshot Read() {
        return snapshot;
    }

    bool Request(std::uint32_t caster) {
        if (!caster || busy)
            return false;
        snapshot = {};
        snapshot.active = 1;
        casterID = caster;
        targetID = 0;
        originID = 0;
        base = nullptr;
        abortRequested = cast = clearing = false;
        casterAICaptured = false;
        arrived = returning = returnIssued = locationRestored = false;
        pendingResult.clear();
        clock = 0;
        evidence = {};
        summary = {};
        samples = {};
        summary << "CHALLENGE APPLICATION DIAGNOSTIC\n"
                << "One native immediate cast per case; no automatic retry.\n"
                << "Control: targeted water breathing, two seconds. No combat or health damage.\n"
                << "CheckCast is caster eligibility, not proof of target acceptance.\n";
        samples << "case,active_seconds,HasMagicEffect,matching_active_effect_fields\n";
        std::snprintf(snapshot.status, sizeof(snapshot.status), "Waiting for the game thread.");
        busy = true;
        return true;
    }

    void Abort() {
        abortRequested = true;
    }

    void Tick(float elapsed, const yliwf::testing::Host& host) {
        if (!busy)
            return;
        if (returning) {
            clock += elapsed;
            ReturnCaster(host);
            return;
        }
        if (abortRequested) {
            Finish("Aborted.");
            return;
        }
        auto* caster = Actor(casterID);
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* data = RE::TESDataHandler::GetSingleton();
        auto* marker = data ? data->LookupForm<RE::TESObjectREFR>(0x811, Plugin) : nullptr;
        if (const auto* unavailable = AvailabilityReason(caster, player, marker)) {
            summary << "Caster " << std::hex << casterID << "; caster cell "
                    << (caster && caster->GetParentCell() ? caster->GetParentCell()->GetFormID() : 0) << "; arena cell "
                    << (marker && marker->GetParentCell() ? marker->GetParentCell()->GetFormID() : 0) << std::dec
                    << '\n';
            Finish(unavailable);
            return;
        }
        clock += elapsed;
        if (!originID) {
            yliwf::testing::Subject permission;
            if (!host.inspect || !host.inspect(casterID, &permission) || !permission.allowed) {
                Finish(permission.reason[0] ? permission.reason
                                            : "Caster movement is restricted by the follower controller.");
                return;
            }
            auto* markerBase = data->LookupForm<RE::TESObjectSTAT>(0x3B, "Skyrim.esm");
            auto origin = markerBase ? caster->PlaceObjectAtMe(markerBase, true) : RE::NiPointer<RE::TESObjectREFR>{};
            if (!origin) {
                Finish("Could not capture the caster's original position.");
                return;
            }
            origin->data.angle = caster->GetAngle();
            originID = origin->GetFormID();
            casterAIEnabled = caster->IsAIEnabled();
            casterAICaptured = true;
            caster->EnableAI(false);
            summary << "Caster " << std::hex << casterID << "; origin marker " << originID << "; origin cell "
                    << (origin->GetParentCell() ? origin->GetParentCell()->GetFormID() : 0) << std::dec
                    << "; original AI " << casterAIEnabled << '\n';
            caster->MoveTo(marker);
            clock = 0;
            std::snprintf(snapshot.status, sizeof(snapshot.status), "Summoning caster; waiting for arena loading.");
            return;
        }
        if (!arrived) {
            const auto arrival = spell_diagnostic_rules::ObserveArrival(
                caster->GetParentCell() == marker->GetParentCell(), caster->Is3DLoaded(), clock);
            if (arrival == spell_diagnostic_rules::Arrival::Failed) {
                Finish("Caster did not finish loading into the arena.");
                return;
            }
            if (arrival != spell_diagnostic_rules::Arrival::Ready)
                return;
            arrived = true;
            clock = 0;
        } else if (!caster->Is3DLoaded() || caster->GetParentCell() != marker->GetParentCell()) {
            Finish("Caster left the arena or became unloaded during the diagnostic.");
            return;
        }
        if (!base) {
            challenge = data->LookupForm<RE::SpellItem>(0xA1D, info::PluginFile);
            control = data->LookupForm<RE::SpellItem>(0x858, Plugin);
            base = data->LookupForm<RE::TESNPC>(0x857, Plugin);
            if (!base || !challenge || !control) {
                base = nullptr;
                Finish("Missing diagnostic records; install matching core and Combat Tests plugins.");
                return;
            }
            originalInvulnerable = base->IsInvulnerable();
            casterHandle = caster->GetHandle();
            auto placed = caster->PlaceObjectAtMe(base, false);
            auto* target = placed ? placed->As<RE::Actor>() : nullptr;
            if (!target) {
                if (placed) {
                    placed->Disable();
                    placed->SetDelete(true);
                }
                Finish("Could not create the dedicated diagnostic target.");
                return;
            }
            targetID = target->GetFormID();
            auto position = caster->GetPosition();
            position.x += 128;
            target->SetPosition(position, true);
            target->EnableAI(false);
            summary << "Caster " << std::hex << casterID << "; target " << targetID << "; Challenge "
                    << challenge->GetFormID() << "; control " << control->GetFormID() << std::dec << '\n';
            clock = 0;
        }
        auto* target = Actor(targetID);
        const auto combatTarget = caster->GetActorRuntimeData().currentCombatTarget.get();
        const auto targetEnemy = target ? target->GetActorRuntimeData().currentCombatTarget.get() : nullptr;
        if (!target || target->GetActorBase() != base || target->IsDead() ||
            (combatTarget && combatTarget->GetFormID() != targetID) ||
            (targetEnemy && targetEnemy->GetFormID() != casterID)) {
            Finish("Target unavailable or an external combat target appeared; comparison invalidated.");
            return;
        }
        if (!target->Is3DLoaded()) {
            if (clock >= spell_diagnostic_rules::LoadTimeout)
                Finish("Diagnostic target did not load.");
            return;
        }
        if (clearing) {
            const auto decision = spell_diagnostic_rules::ObserveCleanup(EffectsCleared(target), clock);
            if (decision == spell_diagnostic_rules::Cleanup::Failed)
                Finish("Diagnostic effect did not clear; comparison invalidated.");
            else if (decision == spell_diagnostic_rules::Cleanup::Complete)
                Advance();
            return;
        }
        const auto test = spell_diagnostic_rules::Cases[snapshot.completed];
        auto* spell = test.control ? control : challenge;
        if (!cast) {
            SetInvulnerable(test.invulnerable);
            if (clock < spell_diagnostic_rules::Settle)
                return;
            if (!ClearEffects(target)) {
                Finish("Previous diagnostic effect remains active; comparison invalidated.");
                return;
            }
            auto* engineCaster = caster->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
            if (!engineCaster) {
                Finish("Instant magic caster unavailable.");
                return;
            }
            float strength = 0;
            auto reason = RE::MagicSystem::CannotCastReason::kOK;
            const bool eligible = engineCaster->CheckCast(spell, false, &strength, &reason, false);
            summary << "\nCase " << snapshot.completed + 1 << ": " << (test.control ? "Control" : "Challenge")
                    << "; invulnerable=" << target->GetActorBase()->IsInvulnerable() << "; CheckCast=" << eligible
                    << "; reason=" << static_cast<int>(reason) << "; strength=" << strength
                    << "; distance=" << target->GetPosition().GetDistance(caster->GetPosition()) << '\n';
            engineCaster->CastSpellImmediate(spell, false, target, 1.0f, false, 0, caster);
            evidence[snapshot.completed].dispatched = true;
            cast = true;
            clock = 0;
            std::snprintf(snapshot.status, sizeof(snapshot.status), "Observing case %u / 4.", snapshot.completed + 1);
        }
        Observe(target, spell);
        if (clock < spell_diagnostic_rules::Observe)
            return;
        const auto& result = evidence[snapshot.completed];
        summary << "Observed HasMagicEffect=" << result.hasEffect << "; active spell=" << result.spellEntry
                << "; active base effect=" << result.baseEntry << "; production detector=" << result.strictEntry
                << '\n';
        if (!ClearEffects(target)) {
            clearing = true;
            clock = 0;
            return;
        }
        Advance();
    }

    void Reset() {
        if (busy) {
            CleanupTarget();
            RestoreAI();
            DeleteOrigin();
            CompleteReport("Interrupted by game load/revert; old world position was not restored.");
        }
        base = nullptr;
        targetID = casterID = 0;
        busy = false;
    }
}
