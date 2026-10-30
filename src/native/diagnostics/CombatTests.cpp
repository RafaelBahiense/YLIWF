#include "diagnostics/Debug.h"
#include "CombatTestHost.h"
#include "followers/QuestPolicies.h"
#include "combat/CombatRoles.h"
#include "combat/CombatOrders.h"
#include "combat/ShieldDefender.h"
#include "combat/DefenderActions.h"
#include <cstdio>

namespace mod::debug {
    namespace {
        RE::BGSRefAlias* TestAlias(RE::Actor* actor) {
            auto* data = RE::TESDataHandler::GetSingleton();
            auto* quest = data ? data->LookupForm<RE::TESQuest>(0x750BA, "Skyrim.esm") : nullptr;
            if (!actor || !quest || !quest->IsRunning())
                return nullptr;
            for (auto* entry : quest->aliases) {
                if (!entry || !(entry->aliasID == 0 || (entry->aliasID >= 2 && entry->aliasID <= 8)))
                    continue;
                auto* alias = skyrim_cast<RE::BGSRefAlias*>(entry);
                if (alias && alias->GetActorReference() == actor)
                    return alias;
            }
            return nullptr;
        }

        std::uint32_t InspectTestSubject(std::uint32_t id, yliwf::testing::Subject* output) {
            if (!output || output->size < sizeof(*output))
                return 0;
            *output = {};
            if (!IsCurrentGame(Generation())) {
                std::snprintf(output->reason, sizeof(output->reason), "YLIWF is still loading follower state.");
                return 1;
            }
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(id);
            output->managed = TestAlias(actor) != nullptr;
            if (!output->managed) {
                std::snprintf(output->reason, sizeof(output->reason), "Select a core-managed follower.");
                return 1;
            }
            if (combat_orders::Enabled.load()) {
                std::snprintf(output->reason, sizeof(output->reason),
                              "Turn off Stand down before running combat tests.");
                return 1;
            }
            const auto role = combat_roles::Inspect(actor);
            output->role = static_cast<std::uint32_t>(role.assigned);
            std::snprintf(output->behavior, sizeof(output->behavior), "%s", role.behavior.c_str());
            for (std::size_t i = 0; i < role.options.size(); ++i)
                if (role.options[i].eligible)
                    output->eligibleRoles |= 1u << i;
            const auto policy = quest_policies::Inspect(actor);
            const auto required = yliwf::policy::Bit(yliwf::policy::Action::Follow) |
                                  yliwf::policy::Bit(yliwf::policy::Action::Movement) |
                                  yliwf::policy::Bit(yliwf::policy::Action::Combat);
            output->allowed = role.available && !(policy.blocked & required);
            if (!output->allowed)
                std::snprintf(output->reason, sizeof(output->reason), "%s",
                              !role.available ? role.unavailable.c_str() : policy.reason);
            return 1;
        }

        std::uint32_t SetTestRole(std::uint32_t id, std::uint32_t role) {
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(id);
            auto* alias = TestAlias(actor);
            if (!alias || !combat_roles::Valid(static_cast<std::int32_t>(role)) ||
                !quest_policies::Allows(actor, yliwf::policy::Action::Combat))
                return 0;
            return combat_roles::Apply(id, alias->aliasID, static_cast<combat_roles::Role>(role), Generation());
        }

        std::uint32_t ObserveDefender(std::uint32_t id, yliwf::testing::DefenderObservation* output) {
            if (!output || output->size < sizeof(*output) || !IsCurrentGame(Generation()))
                return 0;
            *output = {};
            const auto state = shield_defender::Inspect(id);
            output->available = state.available;
            output->requested = state.requested;
            output->owned = state.owned;
            output->yielded = state.yielded;
            output->stance = state.stance;
            output->style = state.style;
            output->threat = state.threat;
            output->incoming = state.incoming;
            output->committed = state.committed;
            output->recovering = state.recovering;
            output->counter = state.counter;
            output->targeting = state.targeting;
            output->distance = state.distance;
            output->heading = state.heading;
            output->stamina = state.stamina;
            output->maximumStamina = state.maximumStamina;
            output->guardRemaining = state.guardRemaining;
            output->age = state.age;
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(id);
            const auto* controller = actor ? actor->GetActorRuntimeData().combatController : nullptr;
            output->liveStyle = controller && controller->combatStyle ? controller->combatStyle->GetFormID() : 0;
            const auto actions = defender_actions::Inspect(id);
            output->actionAvailable = actions.available;
            output->actionMode = actions.mode;
            output->guardPhase = actions.phase;
            output->actionFault = actions.fault;
            output->actionRequests = actions.actions;
            output->attackRequests = actions.attacks;
            output->attacksRejected = actions.rejected;
            output->engineRejected = actions.engineRejected;
            output->guardRequests = actions.guardRequests;
            output->guardAccepted = actions.guardAccepted;
            output->guardObserved = actions.guardObserved;
            output->releaseRequests = actions.releaseRequests;
            output->releaseAccepted = actions.releaseAccepted;
            output->releaseObserved = actions.releaseObserved;
            output->actionTimeouts = actions.timeouts;
            output->lastAction = actions.lastAction;
            output->guardRejected = actions.guardRejected;
            output->dispatch = actions.dispatch;
            output->validationMask = actions.validationMask;
            output->processFlags = actions.processFlags;
            output->processResult = actions.processResult;
            output->processReturned = actions.processReturned;
            output->blockRequests = actions.blockRequests;
            output->bashRequests = actions.bashRequests;
            output->unknownLeftRequests = actions.unknownLeftRequests;
            output->guardSuspended = actions.guardSuspended;
            output->blockReadable = actions.blockReadable;
            output->wantsBlock = actions.wantsBlock;
            output->startRecovering = actions.startRecovering;
            output->startStaminaFraction = actions.startStaminaFraction;
            output->releaseRejected = actions.releaseRejected;
            std::snprintf(output->guardEvent, sizeof(output->guardEvent), "%s", actions.guardEvent.c_str());
            std::snprintf(output->actionStatus, sizeof(output->actionStatus), "%s", actions.status.c_str());
            std::snprintf(output->reason, sizeof(output->reason), "%s", state.reason.c_str());
            return 1;
        }
    }

    namespace {
        std::uint32_t BeginDefense(std::uint32_t actor, std::uint32_t mode, std::uint32_t token) {
            if (!IsCurrentGame(Generation()) || !TestAlias(RE::TESForm::LookupByID<RE::Actor>(actor)))
                return 0;
            return defender_actions::Begin(actor, mode, token);
        }

        std::uint32_t EndDefense(std::uint32_t actor, std::uint32_t token, std::uint32_t savedPhase) {
            return IsCurrentGame(Generation()) ? defender_actions::End(actor, token, savedPhase) : 0;
        }
    }

    const yliwf::testing::Host* CombatTestHost() {
        static const yliwf::testing::Host host{
            sizeof(host), yliwf::testing::Version,     InspectTestSubject, SetTestRole, ObserveDefender, BeginDefense,
            EndDefense,   defender_actions::SavedPhase};
        return &host;
    }
}

extern "C" __declspec(dllexport) const yliwf::testing::Host* YLIWF_GetCombatTestHost(std::uint32_t version) {
    return version == yliwf::testing::Version ? mod::debug::CombatTestHost() : nullptr;
}
