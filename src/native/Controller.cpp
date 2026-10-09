#include "RecordNames.h"
#include "PapyrusNames.h"
#include "Controller.h"
#include "ControllerExecutor.h"
#include "ExecutorRules.h"

#include "ControllerRules.h"
#include "CommandRules.h"
#include "Debug.h"
#include "PartyRules.h"
#include "Strings.h"
#include "Settings.h"
#include "Adapters.h"

#include <mutex>
#include <limits>
#include <optional>
#include <type_traits>

namespace mod::controller {
    namespace properties = mod::papyrus_names::properties;

    namespace {
        using namespace mod::controller_rules;
        using Script = RE::BSTSmartPointer<RE::BSScript::Object>;
        Context context;

        mod::command_rules::ActiveClock progressClock;
        bool ready = false;

        struct PartyCommand {
            std::int32_t ticket;
            std::string label, failure;
            mod::command_rules::GroupProgress progress;
            std::function<void(bool, std::string)> completion;
        };

        std::vector<PartyCommand> partyCommands;

        bool Paused() {
            auto* ui = RE::UI::GetSingleton();
            return ui && ui->GameIsPaused();
        }

        void Progress() {
            progressClock.Progress(mod::command_rules::ActiveClock::Clock::now(), Paused());
        }

        Script FindBound(RE::TESQuest* quest) {
            Script object;
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
            if (quest && quest == context.quest && policy)
                vm->FindBoundObject(policy->GetHandleForObject(RE::FormType::Quest, quest),
                                    mod::papyrus_names::DialogueFollower, object);
            return object && object->IsInitialized() ? object : Script{};
        }

        using storage::Read;
        using storage::Write;

        Script Bound(RE::TESQuest* quest) {
            return storage::Available() ? FindBound(quest) : Script{};
        }

        bool Valid(RE::BGSRefAlias* alias) {
            if (!alias || alias->owningQuest != context.quest)
                return false;
            if (alias->aliasID == 1)
                return mod::strings::EqualsIgnoreCase(alias->aliasName.c_str(), mod::record_names::aliases::Animal);
            return mod::party_rules::ValidBinding(true, alias->aliasID, alias->aliasName.c_str(), alias->aliasID == 0);
        }

        RE::BGSRefAlias* Alias(std::int32_t id) {
            if (!context.quest || id < 0)
                return nullptr;
            for (auto* alias : context.quest->aliases)
                if (alias && alias->aliasID == static_cast<std::uint32_t>(id)) {
                    auto* ref = skyrim_cast<RE::BGSRefAlias*>(alias);
                    return Valid(ref) ? ref : nullptr;
                }
            return nullptr;
        }

        RE::BGSRefAlias* HomeAlias(std::int32_t id) {
            if (!context.homeQuest || id < 0 || id >= 16)
                return nullptr;
            for (auto* alias : context.homeQuest->aliases)
                if (alias && alias->aliasID == static_cast<std::uint32_t>(id)) {
                    auto* ref = skyrim_cast<RE::BGSRefAlias*>(alias);
                    const auto name = mod::record_names::aliases::Home(id);
                    return ref && ref->owningQuest == context.homeQuest &&
                                   mod::strings::EqualsIgnoreCase(ref->aliasName.c_str(), name)
                               ? ref
                               : nullptr;
                }
            return nullptr;
        }

        Slot Snapshot(RE::BGSRefAlias* alias, std::int32_t id) {
            Slot slot{id};
            slot.valid = Valid(alias) && alias->aliasID == static_cast<std::uint32_t>(id);
            if (!slot.valid)
                return slot;
            auto* ref = alias->GetReference();
            auto* actor = ref ? ref->As<RE::Actor>() : nullptr;
            slot.occupied = ref != nullptr;
            if (actor) {
                slot.actor = actor->GetFormID();
                slot.dead = actor->IsDead();
                slot.waiting = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kWaitingForPlayer);
            }
            return slot;
        }

        std::vector<Slot> Slots(const Script& object) {
            std::vector<Slot> result{Snapshot(Read<RE::BGSRefAlias*>(object, properties::FollowerAlias), 0)};
            const auto extras = storage::ExtraAliases();
            for (std::size_t i = 0; i < extras.size(); ++i)
                result.push_back(Snapshot(extras[i], static_cast<std::int32_t>(i + 2)));
            return result;
        }

        std::int32_t Cap(const Script&, std::int32_t capacity = 0) {
            if (!capacity)
                capacity = static_cast<std::int32_t>(storage::ExtraAliases().size()) + 1;
            return std::clamp(context.getCap ? context.getCap() : 1, 1, capacity);
        }

        mod::party_rules::Globals Counts(Script& object) {
            const auto slots = Slots(object);
            const auto values =
                mod::party_rules::CountGlobals(Count(slots), Cap(object, static_cast<std::int32_t>(slots.size())));
            storage::Get().followerCount = static_cast<std::int32_t>(values.partyCount);
            mod::debug::NotifyStateChanged();
            return values;
        }

        bool Owns(const Script& object, std::int32_t ticket) {
            return object && ticket > 0 && storage::Get().active.has_value() && storage::Get().ticket == ticket;
        }

        Request CaptureRequest(const Script& object, std::int32_t operation, RE::Actor* actor, std::int32_t selected,
                               std::int32_t message, std::int32_t sayLine, std::size_t slotCount) {
            const bool valid = actor && actor != RE::PlayerCharacter::GetSingleton();
            return {.operation = static_cast<Operation>(operation),
                    .actor = actor ? actor->GetFormID() : 0,
                    .selected = selected,
                    .message = message,
                    .sayLine = sayLine,
                    .cap = Cap(object, static_cast<std::int32_t>(slotCount)),
                    .actorValid = valid,
                    .actorDead = actor && actor->IsDead(),
                    .recruitable = actor && context.potentialFaction && actor->IsInFaction(context.potentialFaction),
                    .orphan = actor && (actor->IsPlayerTeammate() ||
                                        (context.currentFaction && actor->IsInFaction(context.currentFaction)))};
        }

        Plan BuildHomePlan(const Request& request) {
            std::array<Slot, 8> homes;
            std::array<bool, 8> markers;
            for (std::int32_t i = 0; i < 8; ++i) {
                auto* alias = HomeAlias(i);
                auto* ref = alias ? alias->GetReference() : nullptr;
                auto* resident = ref ? ref->As<RE::Actor>() : nullptr;
                homes[i] = {i, resident ? resident->GetFormID() : 0, alias != nullptr, ref != nullptr};
                auto* marker = HomeAlias(i + 8);
                markers[i] = marker && marker->GetReference();
            }
            return Home(homes, markers, request);
        }

        bool Start(const storage::Command& command) {
            auto object = Bound(context.quest);
            if (!object || storage::Get().active || !SupportedOperation(command.operation) ||
                storage::Get().ticket == std::numeric_limits<std::int32_t>::max())
                return false;
            auto* actor = storage::Actor(command.actor);
            const auto slots = Slots(object);
            const auto animal = Snapshot(Read<RE::BGSRefAlias*>(object, properties::AnimalAlias), 1);
            const auto request = CaptureRequest(object, static_cast<int>(command.operation), actor, command.selected,
                                                command.message, command.sayLine, slots.size());
            if ((command.operation == Operation::HomeAssign || command.operation == Operation::HomeRemove) &&
                !context.homeFaction)
                return false;
            auto plan = command.operation == Operation::HomeAssign || command.operation == Operation::HomeRemove
                            ? BuildHomePlan(request)
                            : Build(slots, animal, request);
            if (!plan.accepted || plan.steps.size() > 63)
                return false;
            if (std::ranges::any_of(plan.steps, [](const auto& step) {
                    return (step.actor && adapters::Owns(step.actor)) ||
                           (step.otherActor && adapters::Owns(step.otherActor));
                })) {
                logger::warn("Native plan rejected: an add-on owns a target actor");
                return false;
            }
            storage::ActiveOperation active;
            active.ticket = storage::Get().ticket + 1;
            active.request = command.id;
            active.operation = command.operation;
            active.debugTicket = command.debugTicket;
            for (const auto& step : plan.steps) {
                auto* target = step.actor ? RE::TESForm::LookupByID<RE::Actor>(step.actor) : nullptr;
                const auto handle = storage::ActorHandle(target);
                auto* other = step.otherActor ? RE::TESForm::LookupByID<RE::Actor>(step.otherActor) : nullptr;
                const auto otherHandle = storage::ActorHandle(other);
                if ((step.actor && !handle) || (step.otherActor && !otherHandle))
                    return false;
                active.steps.push_back({step.effect, step.alias, handle, step.argument, otherHandle});
            }
            storage::Get().ticket = active.ticket;
            storage::SetActive(std::move(active));
            storage::Get().outcome = static_cast<int>(mod::command_rules::State::Started);
            storage::Get().failure.clear();
            Progress();
            mod::debug::Trace("Native.Controller.begin", actor, "request={} operation={} steps={}", command.id,
                              static_cast<int>(command.operation), plan.steps.size());
            return true;
        }

        void PartyReceipt(std::int32_t request, bool success, const std::string& detail) {
            for (auto it = partyCommands.begin(); it != partyCommands.end(); ++it) {
                if (!it->progress.Record(request, success))
                    continue;
                if (!success && it->failure.empty())
                    it->failure = detail;
                const auto& progress = it->progress;
                auto message = fmt::format("{}: {} / {} followers verified", it->label,
                                           progress.completed - progress.failed, progress.total);
                if (progress.failed)
                    message += fmt::format("; {} failed. {}", progress.failed, it->failure);
                else
                    message += progress.Done() ? "." : "; remaining commands pending.";
                if (progress.Done()) {
                    const auto verified = progress.failed == 0;
                    const auto ticket = it->ticket;
                    auto completion = std::move(it->completion);
                    partyCommands.erase(it);
                    if (completion)
                        completion(verified, std::move(message));
                    else
                        mod::debug::CompleteCommand(ticket, verified, std::move(message));
                } else
                    mod::debug::UpdateCommandProgress(it->ticket, std::move(message));
                return;
            }
        }

        void RecordReceipt(const storage::Command& command, bool success, std::string detail, std::int32_t ticket = 0,
                           std::int32_t cursor = 0) {
            auto& state = storage::Get();
            if (state.receipts.size() == mod::command_rules::Capacity)
                state.receipts.erase(state.receipts.begin());
            state.receipts.push_back({command.id, success, ticket, cursor, command.operation, command.debugTicket});
            for (auto& caller : state.callers)
                if (caller.request == command.id && caller.result < 0)
                    caller.result = success ? 1 : 0;
            state.failure = detail;
            if (!success)
                logger::warn("Controller command request={} failed: {}", command.id, detail);
            if (command.debugTicket)
                mod::debug::CompleteCommand(command.debugTicket, success, std::move(detail));
            else
                PartyReceipt(command.id, success, detail);
            mod::debug::NotifyStateChanged();
        }

        std::int32_t Enqueue(Operation operation, RE::Actor* actor, std::int32_t selected, std::int32_t message,
                             std::int32_t sayLine, std::int32_t debugTicket,
                             std::optional<storage::Caller> caller = std::nullopt) {
            std::scoped_lock lock(mod::settings::Mutex);
            auto& state = storage::Get();
            if (!ready || !Bound(context.quest) || !SupportedOperation(operation) ||
                (debugTicket && !mod::debug::IsCommandCurrent(debugTicket)))
                return 0;
            if (debugTicket) {
                if (state.active && state.active->debugTicket == debugTicket)
                    return state.active->request;
                for (const auto& command : state.queue)
                    if (command.debugTicket == debugTicket)
                        return command.id;
                for (const auto& receipt : state.receipts)
                    if (receipt.debugTicket == debugTicket) {
                        mod::debug::CompleteCommand(debugTicket, receipt.success, "Existing completion receipt");
                        return receipt.request;
                    }
            }
            if (auto duplicate = mod::command_rules::Coalesce(state.queue, operation, debugTicket)) {
                if (caller) {
                    caller->request = duplicate;
                    state.callers.push_back(std::move(*caller));
                }
                executor::Kick();
                return duplicate;
            }
            if (!mod::command_rules::CanAppend(state.queue.size(), 1, state.sequence))
                return 0;
            const auto handle = storage::ActorHandle(actor);
            if (actor && !handle)
                return 0;
            const auto request = state.sequence + 1;
            storage::Command command{request, operation, handle, selected, message, sayLine, debugTicket};
            // Publish the continuation before any inline execution can complete.
            if (caller) {
                caller->request = request;
                state.callers.push_back(std::move(*caller));
            }
            state.sequence = request;
            if (executor::CanStartNow()) {
                if (!Start(command))
                    RecordReceipt(command, false, "Request rejected against current actor/alias state");
            } else {
                auto queue = state.queue;
                queue.push_back(command);
                storage::SetQueue(std::move(queue));
            }
            executor::Kick();
            return request;
        }

        bool StartNext() {
            std::scoped_lock lock(mod::settings::Mutex);
            auto& state = storage::Get();
            if (!Bound(context.quest) || state.active)
                return false;
            while (!state.queue.empty()) {
                const auto command = state.queue.front();
                const bool accepted = Start(command);
                auto remaining = state.queue;
                remaining.erase(remaining.begin());
                storage::SetQueue(std::move(remaining));
                if (accepted)
                    return true;
                RecordReceipt(command, false, "Request rejected against current actor/alias state");
            }
            return false;
        }

        bool Acknowledge(std::int32_t ticket, std::int32_t cursor, bool succeeded) {
            std::scoped_lock lock(mod::settings::Mutex);
            if (!Owns(Bound(context.quest), ticket))
                return false;
            auto& active = storage::Active();
            if (!succeeded || !mod::command_rules::Acknowledgement(active.next, cursor, active.steps.size())) {
                if (storage::Get().failure.empty())
                    storage::Get().failure = fmt::format("Step at cursor {} failed or returned out of order", cursor);
                return false;
            }
            ++active.next;
            storage::Get().outcome = static_cast<int>(mod::command_rules::State::Executing);
            Progress();
            return true;
        }

        bool Validate(const storage::Step& step) {
            if (!storage::Get().active)
                return false;
            const auto ticket = storage::Active().ticket;
            const auto effect = static_cast<int>(step.effect), aliasID = step.alias, argument = step.argument;
            auto* actor = storage::Actor(step.actor);
            std::scoped_lock settingsGuard(mod::settings::Mutex);
            auto object = Bound(context.quest);
            auto reject = [&](std::string_view reason) {
                mod::debug::Trace("Native.Controller.stale", actor, "ticket={} effect={} alias={} {}", ticket, effect,
                                  aliasID, reason);
                if (Owns(object, ticket))
                    storage::Get().failure = fmt::format("Effect {} / alias {}: {}", effect, aliasID, reason);
                return false;
            };
            if (!Owns(object, ticket) || effect < 1 || effect > static_cast<std::int32_t>(Effect::SwapPrimary))
                return reject("invalid ownership/effect");
            auto* otherActor = storage::Actor(step.otherActor);
            if ((actor && adapters::Owns(actor->GetFormID())) ||
                (otherActor && adapters::Owns(otherActor->GetFormID())))
                return reject("an add-on owns this actor; native effects cannot resume against its controller");
            const auto command = static_cast<Effect>(effect);
            const bool home = IsHomeEffect(command);
            auto* alias = aliasID >= 0 ? (home ? HomeAlias(aliasID) : Alias(aliasID)) : nullptr;
            if (aliasID >= 0 && !alias)
                return reject("invalid alias binding");
            if (command == Effect::SwapPrimary) {
                auto* other = storage::Actor(step.otherActor);
                if (!actor || other == RE::PlayerCharacter::GetSingleton() || (step.otherActor && !other) ||
                    !CanSwapPrimary(Snapshot(Alias(0), 0), Snapshot(alias, aliasID), actor->GetFormID(),
                                    other ? other->GetFormID() : 0))
                    return reject("primary or selected follower changed");
            }
            if (alias) {
                auto* actual = alias->GetActorReference();
                if (!ExpectedOccupant(command, alias->GetReference() != nullptr, actual ? actual->GetFormID() : 0,
                                      actor ? actor->GetFormID() : 0, actor && actor->IsDead(), argument)) {
                    mod::debug::Trace("Native.Controller.occupant", actor,
                                      "ticket={} alias={} actual={:08X} argument={}", ticket, aliasID,
                                      actual ? actual->GetFormID() : 0, argument);
                    return reject("alias occupant or actor state changed");
                }
                if (command == Effect::Assign || command == Effect::HomeAssign) {
                    if (argument == 1 && Count(Slots(object)) >= Cap(object))
                        return reject("party limit reached");
                } else if (command == Effect::HomeMove) {
                    auto* marker = HomeAlias(aliasID + 8);
                    if (alias->GetReference() || !marker || !marker->GetReference())
                        return reject("home destination changed");
                }
            }
            if (command != Effect::ReservedUpdate && command != Effect::EndDismissLine &&
                (!actor || actor == RE::PlayerCharacter::GetSingleton()))
                return false;
            if (command == Effect::Release) {
                auto slots = Slots(object);
                const auto animal = Snapshot(Read<RE::BGSRefAlias*>(object, properties::AnimalAlias), 1);
                if (Find(slots, actor->GetFormID()) || animal.actor == actor->GetFormID())
                    return reject("actor is registered again");
            }
            if (command == Effect::Prepare && actor->IsDead())
                return reject("actor is dead");
            if (command == Effect::Waiting) {
                auto slots = Slots(object);
                const auto animal = Snapshot(Read<RE::BGSRefAlias*>(object, properties::AnimalAlias), 1);
                if (!Find(slots, actor->GetFormID()) && animal.actor != actor->GetFormID())
                    return reject("actor is unregistered");
            }
            mod::debug::Trace("Native.Controller.step", actor, "ticket={} effect={} alias={} argument={}", ticket,
                              effect, aliasID, argument);
            return true;
        }

        std::string VerifyAliasExpectation(mod::command_rules::Expected check) {
            using Check = mod::command_rules::Check;
            auto* alias = check.kind == Check::FollowerAlias ? Alias(check.alias) : HomeAlias(check.alias);
            auto* expected = storage::Actor(check.actor);
            if (!alias || (check.actor && !expected) || alias->GetReference() != expected)
                return fmt::format("Alias {} has an unexpected occupant", check.alias);
            return {};
        }

        std::string VerifyActorExpectation(const Script& object, mod::command_rules::Expected check) {
            using Check = mod::command_rules::Check;
            auto* actor = storage::Actor(check.actor);
            if (!actor)
                return "An operation actor no longer resolves";
            if (check.kind == Check::CurrentFaction && check.value == 2 &&
                (!context.potentialFaction || !actor->IsInFaction(context.potentialFaction)))
                return {};
            bool actual = false;
            RE::TESFaction* faction = nullptr;
            switch (check.kind) {
                case Check::Teammate:
                    actual = actor->IsPlayerTeammate();
                    break;
                case Check::Waiting:
                    if (actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kWaitingForPlayer) !=
                        static_cast<float>(check.value))
                        return fmt::format("Actor {:08X} waiting state differs from the command", actor->GetFormID());
                    return {};
                case Check::CurrentFaction:
                    faction = context.currentFaction;
                    break;
                case Check::DismissedFaction:
                    faction = Read<RE::TESFaction*>(object, properties::DismissedFollower);
                    break;
                case Check::HirelingFaction:
                    faction = Read<RE::TESFaction*>(object, properties::CurrentHireling);
                    break;
                case Check::HomeFaction:
                    faction = context.homeFaction;
                    break;
                default:
                    break;
            }
            if (check.kind != Check::Teammate) {
                if (!faction) {
                    // Optional removals were skipped by the executor too; additions need a binding.
                    if (check.kind == Check::HirelingFaction || (check.kind == Check::DismissedFaction && !check.value))
                        return {};
                    return "Required service faction binding is missing";
                }
                actual = actor->IsInFaction(faction);
            }
            if (actual != (check.value != 0))
                return fmt::format("Actor {:08X} service flag {} differs from the command", actor->GetFormID(),
                                   static_cast<int>(check.kind));
            return {};
        }

        std::string VerifyExpectation(const Script& object, mod::command_rules::Expected check) {
            using Check = mod::command_rules::Check;
            if (check.kind == Check::FollowerAlias || check.kind == Check::HomeAlias)
                return VerifyAliasExpectation(check);
            if (check.kind == Check::AnimalCount) {
                auto* global = Read<RE::TESGlobal*>(object, properties::PlayerAnimalCount);
                if (!global || global->value != check.value)
                    return "Animal count was not published";
                return {};
            }
            return VerifyActorExpectation(object, check);
        }

        std::string VerifyFollowerCounts(const Script& object) {
            const auto values = mod::party_rules::CountGlobals(Count(Slots(object)), Cap(object));
            if (storage::Get().followerCount != static_cast<std::int32_t>(values.partyCount))
                return "Native follower count differs from live aliases";
            for (const auto& [name, expected] : std::array<std::pair<const char*, float>, 2>{
                     {{"CurrentFollowerCount", values.partyCount}, {"CanRecruitMore", values.recruitGate}}}) {
                auto* global = mod::strings::EqualsIgnoreCase(name, "CurrentFollowerCount") ? storage::PartyCount()
                                                                                            : storage::RecruitGate();
                if (!global || global->value != expected)
                    return fmt::format("{} differs from live aliases", name);
            }
            // Activation may change the vanilla dialogue gate after publication;
            // it must remain binary, not equal the party count.
            auto* vanillaGate = Read<RE::TESGlobal*>(object, properties::PlayerFollowerCount);
            if (!vanillaGate || (vanillaGate->value != 0 && vanillaGate->value != 1))
                return "Vanilla follower dialogue gate is missing or invalid";
            return {};
        }

        std::string Verify(const Script& object) {
            const auto plan = storage::Active().steps;
            if (!storage::Active().Done())
                return "Executor did not acknowledge every step";
            for (auto check : mod::command_rules::Expectations(plan)) {
                if (auto failure = VerifyExpectation(object, check); !failure.empty())
                    return failure;
            }
            if (auto failure = VerifyFollowerCounts(object); !failure.empty())
                return failure;
            if (Read<std::int32_t>(object, properties::FollowerDismiss))
                return "Dismissal dialogue flag is still set";
            return {};
        }

        std::string CompletionFailure(const Script& object, bool succeeded) {
            const auto recorded = storage::Get().failure;
            if (succeeded)
                return Verify(object);
            if (recorded.empty())
                return "Executor rejected a step; partial effects were not replayed";
            return recorded.c_str();
        }

        bool Complete(bool succeeded) {
            std::scoped_lock lock(mod::settings::Mutex);
            auto object = Bound(context.quest);
            if (!object || !storage::Get().active)
                return false;
            const auto active = storage::Active();
            const auto failure = CompletionFailure(object, succeeded);
            const bool verified = succeeded && failure.empty();
            Counts(object);
            const bool countRefresh =
                active.operation == Operation::Sync && active.steps.empty() && !active.debugTicket &&
                !std::ranges::any_of(storage::Get().callers,
                                     [&](const auto& caller) { return caller.request == active.request; });
            // Routine count publication must not erase the last gameplay command's diagnostic result.
            if (!countRefresh || !verified) {
                storage::Get().outcome = static_cast<int>(verified ? mod::command_rules::State::Verified
                                                                   : mod::command_rules::State::Failed);
                RecordReceipt({active.request, active.operation, 0, -1, 0, 1, active.debugTicket}, verified, failure,
                              active.ticket, active.Cursor());
            }
            storage::SetActive(std::nullopt);
            Progress();
            return verified;
        }

        RE::Actor* BladeCandidate(RE::StaticFunctionTag*, RE::TESQuest* quest, std::int32_t id) {
            const auto object = Bound(quest);
            if (!object)
                return nullptr;
            for (const auto& slot : Slots(object))
                if (slot.id == id && slot.valid && slot.actor && !slot.dead)
                    return RE::TESForm::LookupByID<RE::Actor>(slot.actor);
            return nullptr;
        }
    }

    namespace detail {
        const Context& GetContext() {
            return context;
        }

        storage::Script Object() {
            return Bound(context.quest);
        }

        std::int32_t Enqueue(Operation op, RE::Actor* actor, std::int32_t selected, std::int32_t message,
                             std::int32_t sayLine, std::int32_t debugTicket, std::optional<storage::Caller> caller) {
            return controller::Enqueue(op, actor, selected, message, sayLine, debugTicket, std::move(caller));
        }

        bool StartNext() {
            return controller::StartNext();
        }

        bool StartCounts() {
            std::scoped_lock lock(mod::settings::Mutex);
            auto& state = storage::Get();
            if (!Object() || state.active || !state.queue.empty() ||
                state.sequence == std::numeric_limits<std::int32_t>::max() ||
                state.ticket == std::numeric_limits<std::int32_t>::max())
                return false;
            storage::ActiveOperation active;
            active.ticket = ++state.ticket;
            active.request = ++state.sequence;
            storage::SetActive(std::move(active));
            return true;
        }

        bool Validate(const storage::Step& step) {
            return controller::Validate(step);
        }

        bool Acknowledge(std::int32_t ticket, std::int32_t cursor, bool succeeded) {
            return controller::Acknowledge(ticket, cursor, succeeded);
        }

        bool Complete(bool succeeded) {
            return controller::Complete(succeeded);
        }

        std::int32_t Cap() {
            auto object = Object();
            return object ? controller::Cap(object) : 1;
        }

        std::int32_t Count() {
            auto object = Object();
            return object ? controller_rules::Count(Slots(object)) : 0;
        }
    }

    void Configure(Context value) {
        context = value;
        storage::Configure(context.quest);
    }

    bool SubmitDebug(std::int32_t action, RE::Actor* actor, std::int32_t alias, std::int32_t ticket) {
        return executor::SubmitDebug(action, actor, alias, ticket);
    }

    namespace {
        template <class F>
        void ForActorAlias(RE::Actor* actor, F function) {
            if (!actor)
                return;
            std::scoped_lock lock(mod::settings::Mutex);
            const auto object = Bound(context.quest);
            if (!object)
                return;
            for (const auto& slot : Slots(object))
                if (slot.valid && slot.actor == actor->GetFormID())
                    function(slot.id);
        }
    }

    void Activated(RE::Actor* actor) {
        ForActorAlias(actor, [actor](std::int32_t) {
            storage::SetActor(storage::Get().speaker, actor);
            executor::RequestCounts();
        });
    }

    void Died(RE::Actor* actor) {
        ForActorAlias(actor, [actor](std::int32_t id) { Enqueue(Operation::Death, actor, id, 0, 1, 0); });
    }

    void CombatChanged(RE::Actor* actor, RE::Actor* target) {
        if (target != RE::PlayerCharacter::GetSingleton())
            return;
        ForActorAlias(actor, [actor](std::int32_t id) { Enqueue(Operation::Dismiss, actor, id, 0, 0, 0); });
    }

    void Unloaded(RE::Actor* actor) {
        ForActorAlias(actor, [actor](std::int32_t id) { Enqueue(Operation::Unload, actor, id, 0, 1, 0); });
    }

    PartySubmission EnqueueParty(std::int32_t operation, std::int32_t uiTicket,
                                 std::function<void(bool, std::string)> completion) {
        std::scoped_lock settingsGuard(mod::settings::Mutex);
        auto object = Bound(context.quest);
        const auto action = static_cast<Operation>(operation);
        if (!ready || !object || !mod::debug::IsCommandCurrent(uiTicket))
            return {0, "The follower controller is unavailable or the request is obsolete."};
        const auto slots = Slots(object);
        if (slots.size() != 8 ||
            std::ranges::any_of(slots, [](const auto& slot) { return !slot.valid || (slot.occupied && !slot.actor); }))
            return {0, "Follower bindings changed; refresh and check installation diagnostics."};
        auto targets = PartyTargets(slots, action);
        std::erase_if(targets, [](auto actor) { return adapters::Owns(actor); });
        if (targets.empty())
            return {0, "No eligible registered followers."};
        auto queue = storage::Get().queue;
        auto sequence = std::max(storage::Get().sequence, 0);
        if (!mod::command_rules::CanAppend(queue.size(), targets.size(), sequence))
            return {0, "The command queue cannot accept the whole party. No followers were queued."};
        std::vector<std::int32_t> requests;
        for (auto id : targets) {
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(id);
            if (!actor)
                return {0, "A follower is unavailable. No followers were queued; refresh and retry."};
            requests.push_back(++sequence);
            // Use actor identity, not a slot: dismissing the primary promotes another
            // follower. Accepted gameplay requests survive reloading their save.
            const auto handle = storage::ActorHandle(actor);
            if (!handle)
                return {0, "Could not retain the whole party. No requests accepted."};
            queue.push_back({sequence, action, handle, PartySelection, -1, 0, 0});
        }
        const auto label = mod::debug::PartyActionName(static_cast<mod::debug::Action>(operation));
        partyCommands.push_back(
            {uiTicket, label, {}, mod::command_rules::GroupProgress{std::move(requests)}, std::move(completion)});
        storage::SetQueue(std::move(queue));
        storage::Get().sequence = sequence;
        logger::info("{} queued for {} unique followers", label, targets.size());
        executor::Kick();
        return {targets.size(), {}};
    }

    void Invalidate() {
        std::scoped_lock settingsGuard(mod::settings::Mutex);
        executor::Invalidate();
        ready = false;
        Progress();
        adapters::Invalidate();
        storage::Reset();
        partyCommands.clear();  // UI feedback is transient; accepted requests remain saved.
    }

    void GameLoaded() {
        std::scoped_lock settingsGuard(mod::settings::Mutex);
        adapters::GameLoaded();
        storage::Loaded();
        ready = storage::Available();
        executor::Loaded();
        Progress();
    }

    void ObservePause() {
        std::scoped_lock settingsGuard(mod::settings::Mutex);
        executor::ObservePause();
        progressClock.Observe(mod::command_rules::ActiveClock::Clock::now(), Paused());
    }

    std::string Status() {
        std::scoped_lock settingsGuard(mod::settings::Mutex);
        progressClock.Observe(mod::command_rules::ActiveClock::Clock::now(), Paused());
        const auto object = FindBound(context.quest);
        if (!storage::LoadFailure().empty())
            return storage::LoadFailure();
        if (!object)
            return {};
        const auto queue = storage::Get().queue;
        if (storage::Get().active.has_value()) {
            const auto phase = storage::Active().phase;
            if (!executor_rules::KnownPhase(phase) ||
                (phase == executor_rules::Phase::Delay && !executor_rules::ValidDelay(storage::Active().delay)))
                return "Saved native execution phase/delay is invalid; operation retained without replay.";
            const auto size = storage::Active().steps.size();
            auto result =
                fmt::format("Executing request {} / operation {}: {}/{} steps; {} queued.", storage::Active().request,
                            static_cast<int>(storage::Active().operation), storage::Active().next, size, queue.size());
            if (progressClock.elapsed >= std::chrono::seconds(30))
                result += " No progress for 30 active seconds; outcome unknown. Command retained, no automatic replay.";
            return result;
        }
        if (!queue.empty())
            return fmt::format("{} queued; waiting for the native executor.", queue.size());
        if (!storage::Get().callers.empty())
            return fmt::format("{} native completion(s) waiting for their original Papyrus stacks; no calls replayed.",
                               storage::Get().callers.size());
        const auto failure = storage::Get().failure;
        if (!failure.empty())
            return std::string("Last command failed: ") + failure.c_str();
        return storage::Get().outcome == static_cast<std::int32_t>(mod::command_rules::State::Verified)
                   ? "Last operation verified."
                   : "Controller idle.";
    }

    void RegisterPapyrus(RE::BSScript::IVirtualMachine* vm) {
        executor::RegisterPapyrus(vm);
        // Set prefixes and normal VM dispatch avoid Papyrus Tweaks worker tasklets.
        vm->RegisterFunction("GetControllerBladeCandidate", mod::info::NativeScript, BladeCandidate);
    }
}
