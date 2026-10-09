#pragma once

#include "ControllerState.h"
#include <chrono>
#include <limits>

// Saved queue metadata contains no FormIDs. Matching native actor entries hold persistent engine handles.
namespace mod::command_rules {
    constexpr std::size_t Capacity = 16;
    enum class State : std::int32_t { Queued = 1, Started = 2, Executing = 3, Verified = 4, Failed = 5 };

    inline bool CanStartImmediately(bool mainThread, bool ready, bool active, bool paused, bool pumping,
                                    std::size_t pending) {
        return mainThread && ready && !active && !paused && !pumping && pending == 0;
    }

    inline bool Acknowledgement(std::size_t next, std::int32_t cursor, std::size_t steps) {
        return next < steps && cursor == 1 + static_cast<std::int32_t>(next) * 2;
    }

    inline bool CanAppend(std::size_t queued, std::size_t count, std::int32_t sequence) {
        return count && queued <= Capacity && count <= Capacity - queued &&
               count <= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max() - std::max(sequence, 0));
    }

    struct GroupProgress {
        std::vector<std::int32_t> pending;
        std::size_t total = 0, completed = 0, failed = 0;

        explicit GroupProgress(std::vector<std::int32_t> requests)
            : pending(std::move(requests)), total(pending.size()) {}

        bool Record(std::int32_t request, bool success) {
            const auto found = std::ranges::find(pending, request);
            if (found == pending.end())
                return false;
            pending.erase(found);
            ++completed;
            if (!success)
                ++failed;
            return true;
        }

        bool Done() const {
            return total && pending.empty();
        }
    };

    // Only adjacent maintenance can coalesce; a gameplay command is an ordering barrier.
    inline std::int32_t Coalesce(std::span<const mod::controller::storage::Command> queue,
                                 controller_rules::Operation operation, std::int32_t debugTicket) {
        if (operation != controller_rules::Operation::Sync || debugTicket || queue.empty())
            return 0;
        const auto& tail = queue.back();
        return tail.operation == controller_rules::Operation::Sync && !tail.debugTicket ? tail.id : 0;
    }
    enum class Check {
        FollowerAlias,
        HomeAlias,
        Teammate,
        Waiting,
        CurrentFaction,
        DismissedFaction,
        HirelingFaction,
        HomeFaction,
        AnimalCount
    };

    struct Expected {
        Check kind;
        std::int32_t alias;
        std::uint64_t actor;
        std::int32_t value;
    };

    inline std::vector<Expected> Expectations(std::span<const mod::controller::storage::Step> steps) {
        using namespace controller_rules;
        std::vector<Expected> result;
        auto set = [&](Check kind, std::int32_t alias, std::uint64_t actor, std::int32_t value) {
            const bool slot = kind == Check::FollowerAlias || kind == Check::HomeAlias;
            auto found = std::ranges::find_if(result, [&](auto entry) {
                return entry.kind == kind && (slot ? entry.alias == alias : entry.actor == actor);
            });
            if (found == result.end())
                result.push_back({kind, alias, actor, value});
            else
                *found = {kind, alias, actor, value};
        };
        for (const auto& step : steps) {
            const auto actor = step.actor;
            const auto argument = step.argument;
            switch (step.effect) {
                case Effect::Clear:
                    set(Check::FollowerAlias, step.alias, 0, 0);
                    break;
                case Effect::Assign:
                    set(Check::FollowerAlias, step.alias, actor, 1);
                    break;
                case Effect::SwapPrimary:
                    set(Check::FollowerAlias, 0, actor, 1);
                    set(Check::FollowerAlias, step.alias, step.otherActor, step.otherActor != 0);
                    break;
                case Effect::HomeClear:
                    set(Check::HomeAlias, step.alias, 0, 0);
                    break;
                case Effect::HomeAssign:
                    set(Check::HomeAlias, step.alias, actor, 1);
                    break;
                case Effect::Teammate:
                    set(Check::Teammate, -1, actor, argument == 1);
                    break;
                case Effect::AnimalPrepare:
                    set(Check::Teammate, -1, actor, 1);
                    break;
                case Effect::Waiting:
                    set(Check::Waiting, -1, actor, argument != 0);
                    break;
                case Effect::Prepare:
                    set(Check::DismissedFaction, -1, actor, 0);
                    set(Check::CurrentFaction, -1, actor, argument == 1 ? 1 : 2);
                    if (argument == 0)
                        set(Check::Waiting, -1, actor, 0);
                    break;
                case Effect::Release:
                    set(Check::CurrentFaction, -1, actor, 0);
                    set(Check::Waiting, -1, actor, 0);
                    break;
                case Effect::Cleanup:
                    set(Check::DismissedFaction, -1, actor, 1);
                    set(Check::HirelingFaction, -1, actor, 0);
                    set(Check::Waiting, -1, actor, 0);
                    break;
                case Effect::HomeFaction:
                    set(Check::HomeFaction, -1, actor, argument != 0);
                    break;
                case Effect::AnimalCount:
                    set(Check::AnimalCount, -1, 0, argument);
                    break;
                default:
                    break;
            }
        }
        return result;
    }

    // Diagnostic time only. Loading rebases it; menus exclude paused wall time.
    struct ActiveClock {
        using Clock = std::chrono::steady_clock;
        Clock::time_point last = Clock::now();
        std::chrono::milliseconds elapsed{};
        bool paused = false;

        void Observe(Clock::time_point now, bool nextPaused) {
            if (!paused)
                elapsed += std::chrono::duration_cast<std::chrono::milliseconds>(now - last);
            last = now;
            paused = nextPaused;
        }

        void Progress(Clock::time_point now, bool nextPaused) {
            Observe(now, nextPaused);
            elapsed = {};
        }
    };
}
