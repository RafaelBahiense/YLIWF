#pragma once

#include "ControllerRules.h"
#include "ExecutorRules.h"
#include "Strings.h"

#include <optional>
#include <string>
#include <vector>

namespace mod::controller::storage {
    using Operation = controller_rules::Operation;
    using Effect = controller_rules::Effect;
    using Phase = executor_rules::Phase;
    using Handle = std::uint64_t;

    struct Command {
        std::int32_t id = 0;
        Operation operation = Operation::Sync;
        Handle actor = 0;
        std::int32_t selected = -1, message = 0, sayLine = 1, debugTicket = 0;
        bool operator==(const Command&) const = default;
    };

    struct Step {
        Effect effect;
        std::int32_t alias = -1;
        Handle actor = 0;
        std::int32_t argument = 0;
        Handle otherActor = 0;
        bool operator==(const Step&) const = default;
    };

    struct Receipt {
        std::int32_t request = 0;
        bool success = false;
        std::int32_t ticket = 0, cursor = 0;
        Operation operation = Operation::Sync;
        std::int32_t debugTicket = 0;
        bool operator==(const Receipt&) const = default;
    };

    struct ActiveOperation {
        std::int32_t ticket = 0, request = 0;
        Operation operation = Operation::Sync;
        std::int32_t debugTicket = 0;
        std::vector<Step> steps;
        std::size_t next = 0;
        Phase phase = Phase::Ready;
        bool succeeded = false;
        float delay = 0;

        // The adapter protocol and current co-save format number step cursors as 1, 3, 5, ...
        std::int32_t Cursor() const {
            return 1 + static_cast<std::int32_t>(next) * 2;
        }

        bool Done() const {
            return next == steps.size();
        }

        bool operator==(const ActiveOperation&) const = default;
    };

    struct Caller {
        std::int32_t request = 0;
        std::uint32_t stack = 0;
        Handle self = 0;
        std::string type, function;
        std::int32_t result = -1;
        bool operator==(const Caller&) const = default;
    };

    struct State {
        std::int32_t followerCount = 0;
        Handle speaker = 0;
        std::optional<ActiveOperation> active;
        std::vector<Command> queue;
        std::vector<Receipt> receipts;
        std::vector<float> deadlines = std::vector<float>(9);
        std::vector<Caller> callers;
        std::int32_t ticket = 0, outcome = 0, sequence = 0;
        std::string failure;
        bool operator==(const State&) const = default;
    };

    inline bool SameCaller(const Caller& expected, const Caller& actual) {
        return expected.request == actual.request && expected.stack == actual.stack && expected.self == actual.self &&
               !expected.type.empty() && !expected.function.empty() &&
               strings::EqualsIgnoreCase(expected.type, actual.type) &&
               strings::EqualsIgnoreCase(expected.function, actual.function);
    }

    template <class Resolve>
    bool RemapState(State& state, Resolve resolve) {
        State replacement = state;
        auto handle = [&](Handle& value) { return !value || resolve(value); };
        if (!handle(replacement.speaker))
            replacement.speaker = 0;
        if (replacement.active)
            for (auto& step : replacement.active->steps)
                if (!handle(step.actor) || !handle(step.otherActor))
                    return false;
        for (auto& command : replacement.queue)
            if (!handle(command.actor))
                return false;
        for (auto& caller : replacement.callers)
            if (!handle(caller.self))
                return false;
        state = std::move(replacement);
        return true;
    }

    inline void ClearUITickets(State& state) {
        if (state.active)
            state.active->debugTicket = 0;
        for (auto& command : state.queue)
            command.debugTicket = 0;
        for (auto& receipt : state.receipts)
            receipt.debugTicket = 0;
    }
}
