#include "Adapters.h"
#include "AdapterRules.h"
#include "ControllerRuntime.h"
#include "ControllerExecutor.h"
#include "Debug.h"
#include "Settings.h"
#include "FollowDistance.h"
#include <Windows.h>
#include <array>
#include <cstring>
#include <map>
#include <memory>
#include <unordered_set>
#include <tuple>

namespace mod::adapters {
    namespace {
        using namespace yliwf::sdk;

        struct Provider {
            Adapter api;
            std::string id, name;
        };

        std::vector<Provider> providers;
        adapter_rules::Requests requests;
        std::map<RequestID, Completion> completions;
        bool loaded = false;
        std::unordered_set<ActorID> knownOwners;
        std::unordered_set<ActorID> knownRecruitable;
        bool knownPresence = false;
        using Observations = std::map<ActorID, std::tuple<AdapterID, State, std::uint32_t, std::string>>;
        Observations observed;
        std::unordered_set<AdapterID> queuedChanges;

        bool MainThread() {
            auto* main = RE::Main::GetSingleton();
            return main && main->threadID == GetCurrentThreadId();
        }

        std::optional<Follower> Inspect(ActorID actor, bool annotatePending = true) {
            knownRecruitable.erase(actor);
            std::optional<Follower> result;
            for (std::size_t i = 0; i < providers.size(); ++i) {
                FollowerState state;
                auto& provider = providers[i];
                if (!provider.api.inspect(provider.api.context, actor, &state))
                    continue;
                state.reason[sizeof(state.reason) - 1] = 0;
                if (state.size < sizeof(state) || state.commands & ~AllCommands ||
                    static_cast<unsigned>(state.state) > 3) {
                    state = {};
                    std::strcpy(state.reason, "Adapter returned an invalid state");
                }
                if (result) {
                    result->state = {};
                    std::strcpy(result->state.reason, "Multiple add-ons claim this actor; commands are disabled");
                    knownOwners.insert(actor);
                    return result;
                }
                result = Follower{actor, static_cast<AdapterID>(i + 1), provider.name, state,
                                  HasFollowDistance(provider.api)};
            }
            if (result) {
                const auto* quest = controller::detail::GetContext().quest;
                if (quest)
                    for (auto* alias : quest->aliases) {
                        const auto* ref = skyrim_cast<RE::BGSRefAlias*>(alias);
                        const auto* occupant = ref ? ref->GetActorReference() : nullptr;
                        if (occupant && occupant->GetFormID() == actor) {
                            result->state.commands = 0;
                            result->state.state = State::Unavailable;
                            std::strcpy(result->state.reason,
                                        "Actor also occupies a YLIWF alias; conflicting ownership");
                        }
                    }
                if (annotatePending &&
                    std::ranges::any_of(requests.items, [actor](const auto& item) { return item.actor == actor; })) {
                    result->state.commands = 0;
                    std::strcpy(result->state.reason,
                                "Controller command is pending or awaiting a late acknowledgement");
                }
            }
            if (result) {
                knownOwners.insert(actor);
                const auto& provider = providers[result->adapter - 1];
                if (result->state.state == State::Inactive && HasRecruitmentEligibility(provider.api) &&
                    !std::ranges::any_of(requests.items, [actor](const auto& item) { return item.actor == actor; }) &&
                    provider.api.canRecruitThroughDialogue(provider.api.context, actor) == 1)
                    knownRecruitable.insert(actor);
            } else {
                knownOwners.erase(actor);
            }
            return result;
        }

        void Finish(AdapterID adapter, RequestID id, bool success, std::string reason, bool remove = true) {
            auto* request = requests.Find(adapter, id);
            if (!request)
                return;
            const auto actor = request->actor;
            auto found = completions.find(id);
            Completion completion;
            if (found != completions.end()) {
                completion = std::move(found->second);
                completions.erase(found);
            }
            if (remove) {
                requests.Remove(adapter, id);
                Inspect(actor, false);  // Dismissal can immediately make dialogue recruitment eligible again.
            }
            if (completion)
                completion(success, std::move(reason));
            debug::NotifyStateChanged();
        }

        AdapterID Register(const Adapter* api) {
            std::scoped_lock lock(settings::Mutex);
            if (!api || !adapter_rules::Valid(*api) || loaded || providers.size() == 32 || std::strlen(api->id) > 128 ||
                std::strlen(api->name) > 128 ||
                std::ranges::any_of(providers, [&](const auto& provider) { return provider.id == api->id; }))
                return 0;
            Adapter supported;
            // Older v1 tables end at start; never read their optional tail.
            std::memcpy(&supported, api, std::min<std::size_t>(api->size, sizeof(Adapter)));
            providers.push_back({supported, api->id, api->name});
            logger::info("Registered follower adapter: {} ({})", api->name, api->id);
            return static_cast<AdapterID>(providers.size());
        }

        void Acknowledge(AdapterID adapter, RequestID id, std::uint32_t success, const char* reason) {
            auto* tasks = SKSE::GetTaskInterface();
            if (!tasks)
                return;
            std::string text;
            if (reason)
                text.assign(reason, strnlen(reason, 191));
            tasks->AddTask([adapter, id, success, text = std::move(text)]() {
                std::scoped_lock lock(settings::Mutex);
                const auto* request = requests.Find(adapter, id);
                if (!request || !request->started)
                    return;
                const auto actor = request->actor;
                const auto command = request->command;
                // Ignore queue/quarantine annotations when verifying the observed state.
                auto state = Inspect(actor, false);
                const bool verified =
                    success == 1 && state && state->adapter == adapter && adapter_rules::Reached(state->state, command);
                logger::info("Follower adapter completed: request={} actor={:08X} verified={}", id, actor, verified);
                Finish(adapter, id, verified,
                       verified       ? "Quest controller completed and state verified."
                       : text.empty() ? "Quest controller did not reach the requested state."
                                      : text);
                controller::executor::RequestCounts();
            });
        }

        bool Queue(const Follower& follower, Command command, Completion completion) {
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(follower.actor);
            if (!loaded || !actor || actor->IsDead() || !adapter_rules::Supports(follower.state, command))
                return false;
            const auto id = requests.Add(follower.adapter, follower.actor, command);
            if (!id)
                return false;
            completions.emplace(*id, std::move(completion));
            logger::info("Adapter command queued: adapter={} request={} actor={:08X} command={}", follower.name, *id,
                         follower.actor, static_cast<unsigned>(command));
            controller::executor::Wake();
            return true;
        }

        Observations Observe() {
            Observations result;
            for (const auto& follower : Followers(false))
                result.emplace(follower.actor, std::tuple{follower.adapter, follower.state.state,
                                                          follower.state.commands, std::string(follower.state.reason)});
            return result;
        }

        void StateChanged(AdapterID adapter) {
            std::scoped_lock lock(settings::Mutex);
            auto* tasks = SKSE::GetTaskInterface();
            if (!tasks || !loaded || !adapter || adapter > providers.size() || !queuedChanges.insert(adapter).second)
                return;
            const auto generation = debug::Generation();
            tasks->AddTask([adapter, generation] {
                std::scoped_lock lock(settings::Mutex);
                if (!loaded || !debug::IsCurrentGame(generation))
                    return;
                queuedChanges.erase(adapter);
                auto current = Observe();
                if (current == observed)
                    return;
                observed = std::move(current);
                ApplyFollowDistance();
                debug::Trace("adapter-state-changed", nullptr, "Controller: {}", providers[adapter - 1].name);
                controller::executor::RequestCounts();
                debug::NotifyStateChanged();  // Refresh stays driven by the visible page.
            });
        }
    }

    const yliwf::sdk::API* Query(std::uint32_t version) {
        static const yliwf::sdk::API api{sizeof(api), yliwf::sdk::InterfaceVersion, Register, Acknowledge,
                                         StateChanged};
        return version == yliwf::sdk::InterfaceVersion ? &api : nullptr;
    }

    std::vector<Follower> Followers(bool annotatePending) {
        std::vector<Follower> result;
        std::vector<ActorID> seen;
        for (const auto& provider : providers) {
            std::array<ActorID, 256> actors{};
            const auto count =
                provider.api.enumerate(provider.api.context, actors.data(), static_cast<std::uint32_t>(actors.size()));
            if (count > actors.size()) {
                logger::warn("Follower adapter {} exceeded enumeration capacity", provider.id);
                continue;
            }
            for (std::uint32_t i = 0; i < count; ++i) {
                if (!actors[i] || std::ranges::find(seen, actors[i]) != seen.end())
                    continue;
                if (!RE::TESForm::LookupByID<RE::Actor>(actors[i]))
                    continue;
                seen.push_back(actors[i]);
                auto follower = Inspect(actors[i], annotatePending);
                if (follower && follower->state.state != State::Inactive)
                    result.push_back(std::move(*follower));
            }
        }
        return result;
    }

    bool Owns(yliwf::sdk::ActorID actor) {
        std::scoped_lock lock(settings::Mutex);
        // Saved engine adapters can validate a step from a VM worker. Never call
        // another DLL's inspector there; use ownership established on the main thread.
        return actor && (MainThread() ? Inspect(actor).has_value() : knownOwners.contains(actor));
    }

    bool HasFollowers() {
        std::scoped_lock lock(settings::Mutex);
        if (!MainThread())
            return knownPresence;
        for (const auto& follower : Followers()) {
            const auto* actor = RE::TESForm::LookupByID<RE::Actor>(follower.actor);
            if (actor && !actor->IsDead() &&
                (follower.state.state == State::Following || follower.state.state == State::Waiting ||
                 actor->IsPlayerTeammate()))
                return knownPresence = true;
        }
        return knownPresence = false;
    }

    bool CanRecruitThroughDialogue(yliwf::sdk::ActorID actor) {
        std::scoped_lock lock(settings::Mutex);
        if (!loaded || !actor)
            return false;
        // Activation/menu events can arrive off-thread; providers are queried
        // only on the main thread, with load/state notifications refreshing this cache.
        if (MainThread())
            Inspect(actor);
        return knownRecruitable.contains(actor);
    }

    void ApplyFollowDistance(bool replaceAll) {
        std::scoped_lock lock(settings::Mutex);
        if (!loaded || !MainThread())
            return;
        for (const auto& follower : Followers(false)) {
            auto& provider = providers[follower.adapter - 1];
            if (!follower.supportsDistance)
                continue;
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(follower.actor);
            if (!actor || actor->IsDead())
                continue;
            if (replaceAll)
                follow_distance::Remember(actor, settings::FollowDistance, false);
            const auto choice = follow_distance::Read(actor);
            follow_distance::Remember(actor, choice.preset, choice.individual);
            if (Supports(follower.state, Command::Follow))
                provider.api.setFollowDistance(provider.api.context, follower.actor,
                                               static_cast<FollowDistance>(choice.preset));
        }
    }

    bool SetFollowDistance(yliwf::sdk::ActorID actorID, std::int32_t preset) {
        const auto follower = Inspect(actorID);
        auto* actor = RE::TESForm::LookupByID<RE::Actor>(actorID);
        if (!loaded || !follow_distance::Available() || !actor || actor->IsDead() ||
            !follow_distance::ValidPreset(preset) || !follower || !follower->supportsDistance ||
            !Supports(follower->state, Command::Follow))
            return false;
        auto& provider = providers[follower->adapter - 1];
        if (!provider.api.setFollowDistance(provider.api.context, actorID, static_cast<FollowDistance>(preset)))
            return false;
        return follow_distance::Remember(actor, preset, true);
    }

    bool Request(yliwf::sdk::ActorID actor, yliwf::sdk::Command command, Completion completion) {
        std::scoped_lock lock(settings::Mutex);
        const auto follower = Inspect(actor);
        return follower && Queue(*follower, command, std::move(completion));
    }

    PartySubmission RequestParty(yliwf::sdk::Command command, Completion completion) {
        std::scoped_lock lock(settings::Mutex);
        const auto followers = Followers();

        struct Group {
            std::size_t remaining = 0, failed = 0;
            Completion completion;
        };

        auto group = std::make_shared<Group>();
        group->completion = std::move(completion);
        for (const auto& follower : followers)
            if (adapter_rules::Supports(follower.state, command))
                ++group->remaining;
        if (!group->remaining)
            return {};
        if (requests.items.size() + group->remaining > adapter_rules::Capacity)
            return {0, "The adapter queue cannot accept all eligible quest-managed followers."};
        const auto count = group->remaining;
        auto done = [group](bool success, std::string) {
            if (!success)
                ++group->failed;
            if (--group->remaining == 0)
                group->completion(group->failed == 0,
                                  fmt::format("Quest-managed followers: {} command(s) failed.", group->failed));
        };
        for (const auto& follower : followers)
            if (adapter_rules::Supports(follower.state, command))
                if (!Queue(follower, command, done))
                    done(false, "Could not queue adapter command");
        return {count, {}};
    }

    void Tick(float elapsed) {
        // Only one external controller call runs at a time. Quarantined calls
        // remain blocked but do not keep a worker alive or stall other actors.
        const auto active =
            std::ranges::find_if(requests.items, [](const auto& item) { return item.started && !item.quarantined; });
        if (active != requests.items.end()) {
            active->elapsed += elapsed;
            if (active->elapsed >= adapter_rules::AcknowledgementSeconds) {
                active->quarantined = true;
                logger::warn("Adapter acknowledgement timed out: adapter={} request={} actor={:08X}", active->adapter,
                             active->id, active->actor);
                Finish(active->adapter, active->id, false,
                       "Quest controller acknowledgement timed out; this actor is blocked until a late acknowledgement "
                       "or reload.",
                       false);
            }
            return;
        }
        const auto next = std::ranges::find_if(requests.items, [](const auto& item) { return !item.started; });
        if (next == requests.items.end())
            return;
        const auto request = *next;
        // Check fresh controller capabilities before dispatch, without our own pending annotation.
        auto& provider = providers[request.adapter - 1];
        const auto follower = Inspect(request.actor, false);
        auto* actor = RE::TESForm::LookupByID<RE::Actor>(request.actor);
        if (!actor || actor->IsDead() || !follower || follower->adapter != request.adapter ||
            !adapter_rules::Supports(follower->state, request.command)) {
            Finish(request.adapter, request.id, false, "Quest state changed before command dispatch.");
            return;
        }
        next->started = true;
        char reason[192]{};
        const auto started = provider.api.start(provider.api.context, request.actor, request.command, request.id,
                                                reason, sizeof(reason));
        logger::info("Adapter dispatch: adapter={} request={} actor={:08X} result={}", provider.name, request.id,
                     request.actor, static_cast<unsigned>(started));
        reason[sizeof(reason) - 1] = 0;
        if (started == StartResult::Completed)
            Acknowledge(request.adapter, request.id, 1, reason);
        else if (started != StartResult::Pending)
            Finish(request.adapter, request.id, false, reason[0] ? reason : "Quest controller rejected dispatch.");
    }

    bool HasWork() {
        return std::ranges::any_of(requests.items, [](const auto& item) { return !item.quarantined; });
    }

    void Invalidate() {
        loaded = false;
        requests.Reset();
        completions.clear();
        knownOwners.clear();
        knownRecruitable.clear();
        knownPresence = false;
        observed.clear();
        queuedChanges.clear();
    }

    void GameLoaded() {
        loaded = true;
        ApplyFollowDistance();
        observed = Observe();
        HasFollowers();
    }
}

extern "C" __declspec(dllexport) const yliwf::sdk::API* YLIWF_GetFollowerAdapterAPI(std::uint32_t version) {
    return mod::adapters::Query(version);
}
