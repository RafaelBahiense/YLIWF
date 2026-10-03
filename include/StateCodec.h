#pragma once

#include "ControllerState.h"
#include "CommandRules.h"
#include "ControllerRules.h"
#include "ExecutorRules.h"

#include <bit>
#include <limits>
#include <span>
#include <type_traits>

namespace mod::controller::storage {
    constexpr std::uint32_t SaveVersion = 1;
    constexpr std::size_t MaxSaveBytes = 65536;
    // Current YLIWF co-save format: version one. Positional arrays exist only in
    // serialization; gameplay uses named structures. This is not a migration codec.
    namespace wire_v1 {
        constexpr std::size_t Width = 6;
        struct Checkpoint {
            std::int32_t followerCount = 0;
            std::uint64_t speaker = 0;
            bool owned = false, succeeded = false, busy = false;
            std::int32_t phase = 0;
            float delay = 0;
            std::vector<float> deadlines;
            std::vector<Caller> callers;
            std::int32_t ticket = 0, progress = 0, operation = 0, request = 0, debugTicket = 0, outcome = 0, sequence = 0;
            std::vector<std::int32_t> plan, queue, receipts;
            std::vector<std::uint64_t> actors, queueActors;
            std::string failure;
        };
        inline std::int32_t EncodeEffect(Effect effect, std::int32_t alias, std::int32_t argument) {
            return static_cast<std::int32_t>(effect) + (alias + 1) * 100 + (argument + 2) * 10000;
        }
        inline Effect DecodeEffect(std::int32_t code) { return static_cast<Effect>(code % 100); }
        inline std::int32_t DecodeAlias(std::int32_t code) { return code / 100 % 100 - 1; }
        inline std::int32_t DecodeArgument(std::int32_t code) { return code / 10000 - 2; }
    }
    inline bool ValidCheckpoint(const wire_v1::Checkpoint& s) {
        using namespace wire_v1;
        using namespace mod::command_rules;
        constexpr auto maximumOperation = static_cast<std::int32_t>(Operation::PromotePrimary);
        if (s.followerCount < 0 || s.followerCount > 8 || s.ticket < 0 || s.sequence < 0 || s.request < 0 ||
            s.debugTicket < 0 || s.operation < 0 || s.operation > maximumOperation || s.outcome < 0 || s.outcome > 5 ||
            !executor_rules::KnownPhase(static_cast<executor_rules::Phase>(s.phase)) || !executor_rules::ValidDelay(s.delay) ||
            s.deadlines.size() != 9 || s.callers.size() > 32 || s.actors.size() > 32 || s.plan.size() > 128 ||
            s.receipts.size() > Capacity * Width || s.receipts.size() % Width || s.failure.size() > 4096 ||
            (s.queue.size() % Width || s.queue.size() / Width != s.queueActors.size() || s.queueActors.size() > Capacity)) return false;
        for (auto time : s.deadlines) if (!std::isfinite(time) || time < 0) return false;
        for (const auto& caller : s.callers)
            if (caller.request <= 0 || caller.request > s.sequence || caller.result < -1 || caller.result > 1 || caller.type.empty() || caller.function.empty() ||
                caller.type.size() > 256 || caller.function.size() > 256) return false;
        for (std::size_t i = 0; i < s.callers.size(); ++i)
            for (std::size_t j = i + 1; j < s.callers.size(); ++j) if (s.callers[i].stack == s.callers[j].stack) return false;
        std::int32_t previous = 0;
        for (std::size_t i = 0; i < s.queue.size(); i += Width) {
            if (s.queue[i] <= previous || s.queue[i] > s.sequence || s.queue[i + 1] < 0 || s.queue[i + 1] > maximumOperation ||
                s.queue[i + 2] < -2 || s.queue[i + 2] > 15 || s.queue[i + 5] < 0) return false;
            previous = s.queue[i];
        }
        for (std::size_t i = 0; i < s.receipts.size(); i += Width)
            if (s.receipts[i] <= 0 || s.receipts[i] > s.sequence || s.receipts[i + 1] < 0 || s.receipts[i + 1] > 1 ||
                s.receipts[i + 2] < 0 || s.receipts[i + 3] < 0 || s.receipts[i + 4] < 0 || s.receipts[i + 4] > maximumOperation || s.receipts[i + 5] < 0) return false;
        if (s.busy) {
            if (!s.owned || !s.ticket || !s.request || s.plan.empty() || s.plan[0] != s.ticket || s.plan.size() % 2 != 1 ||
                s.progress < 1 || s.progress % 2 != 1 || static_cast<std::size_t>(s.progress) > s.plan.size()) return false;
            for (std::size_t i = 1; i + 1 < s.plan.size(); i += 2) {
                const auto effect = wire_v1::DecodeEffect(s.plan[i]);
                const auto index = s.plan[i + 1];
                if (effect < controller_rules::Effect::CancelTimer || effect > controller_rules::Effect::SwapPrimary ||
                    index < -1 || (index >= 0 && static_cast<std::size_t>(index) >= s.actors.size())) return false;
                if (effect == Effect::SwapPrimary) {
                    const auto other = wire_v1::DecodeArgument(s.plan[i]);
                    if (index < 0 || other < -1 || (other >= 0 && static_cast<std::size_t>(other) >= s.actors.size())) return false;
                }
            }
        } else if (s.owned || s.phase != 0) return false;
        return true;
    }
    // Explicit little-endian wire format, bounded before allocating any data.
    class StateWriter {
    public:
        std::vector<std::uint8_t> bytes;
        template <class T> void Number(T value) {
            using U = std::conditional_t<sizeof(T) == 8, std::uint64_t, std::uint32_t>;
            U bits;
            if constexpr (std::is_same_v<T, float>) bits = std::bit_cast<std::uint32_t>(value);
            else bits = static_cast<U>(value);
            for (std::size_t i = 0; i < sizeof(T); ++i) bytes.push_back(static_cast<std::uint8_t>(bits >> (i * 8)));
        }
        void Text(const std::string& value) { Number(static_cast<std::uint32_t>(value.size())); bytes.insert(bytes.end(), value.begin(), value.end()); }
        template <class T> void Array(const std::vector<T>& values) {
            Number(static_cast<std::uint32_t>(values.size())); for (auto value : values) Number(value);
        }
    };
    inline std::vector<std::uint8_t> EncodeCheckpoint(const wire_v1::Checkpoint& s) {
        if (!ValidCheckpoint(s)) return {};
        StateWriter w;
        w.Number(s.followerCount); w.Number(s.speaker);
        w.Number<std::uint32_t>(s.owned); w.Number<std::uint32_t>(s.succeeded); w.Number<std::uint32_t>(s.busy);
        w.Number(s.phase); w.Number(s.delay); w.Array(s.deadlines);
        for (auto value : {s.ticket, s.progress, s.operation, s.request, s.debugTicket, s.outcome, s.sequence}) w.Number(value);
        w.Array(s.plan); w.Array(s.queue); w.Array(s.receipts); w.Array(s.actors); w.Array(s.queueActors); w.Text(s.failure);
        w.Number(static_cast<std::uint32_t>(s.callers.size()));
        for (const auto& caller : s.callers) {
            w.Number(caller.request); w.Number(caller.stack); w.Number(caller.self); w.Text(caller.type); w.Text(caller.function); w.Number(caller.result);
        }
        return std::move(w.bytes);
    }
    class StateReader {
        std::span<const std::uint8_t> bytes;
        std::size_t position = 0;
    public:
        explicit StateReader(std::span<const std::uint8_t> data) : bytes(data) {}
        template <class T> bool Number(T& value) {
            if (bytes.size() - position < sizeof(T)) return false;
            using U = std::conditional_t<sizeof(T) == 8, std::uint64_t, std::uint32_t>;
            U bits = 0;
            for (std::size_t i = 0; i < sizeof(T); ++i) bits |= static_cast<U>(bytes[position++]) << (i * 8);
            if constexpr (std::is_same_v<T, float>) value = std::bit_cast<float>(static_cast<std::uint32_t>(bits));
            else if constexpr (std::is_signed_v<T>) value = std::bit_cast<T>(bits);
            else value = static_cast<T>(bits);
            return true;
        }
        bool Boolean(bool& value) { std::uint32_t bits; if (!Number(bits) || bits > 1) return false; value = bits != 0; return true; }
        bool Text(std::string& value, std::size_t maximum) {
            std::uint32_t length; if (!Number(length) || length > maximum || length > bytes.size() - position) return false;
            value.assign(reinterpret_cast<const char*>(bytes.data() + position), length); position += length; return true;
        }
        template <class T> bool Array(std::vector<T>& values, std::size_t maximum) {
            std::uint32_t count; if (!Number(count) || count > maximum || count > (bytes.size() - position) / sizeof(T)) return false;
            values.resize(count); for (auto& value : values) if (!Number(value)) return false; return true;
        }
        bool Done() const { return position == bytes.size(); }
    };
    inline bool DecodeCheckpoint(std::span<const std::uint8_t> data, wire_v1::Checkpoint& result) {
        if (data.size() > MaxSaveBytes) return false;
        StateReader r(data); wire_v1::Checkpoint s;
        if (!r.Number(s.followerCount) || !r.Number(s.speaker) || !r.Boolean(s.owned) || !r.Boolean(s.succeeded) ||
            !r.Boolean(s.busy) || !r.Number(s.phase) || !r.Number(s.delay) || !r.Array(s.deadlines, 9)) return false;
        for (auto* value : {&s.ticket, &s.progress, &s.operation, &s.request, &s.debugTicket, &s.outcome, &s.sequence}) if (!r.Number(*value)) return false;
        if (!r.Array(s.plan, 128) || !r.Array(s.queue, 96) || !r.Array(s.receipts, 96) || !r.Array(s.actors, 32) ||
            !r.Array(s.queueActors, 16) || !r.Text(s.failure, 4096)) return false;
        std::uint32_t count; if (!r.Number(count) || count > 32) return false;
        s.callers.resize(count);
        for (auto& caller : s.callers)
            if (!r.Number(caller.request) || !r.Number(caller.stack) || !r.Number(caller.self) ||
                !r.Text(caller.type, 256) || !r.Text(caller.function, 256) || !r.Number(caller.result)) return false;
        if (!r.Done() || !ValidCheckpoint(s)) return false;
        result = std::move(s); return true;
    }
    inline wire_v1::Checkpoint Checkpoint(const State& state) {
        wire_v1::Checkpoint wire;
        wire.followerCount = state.followerCount; wire.speaker = state.speaker; wire.ticket = state.ticket;
        wire.outcome = state.outcome; wire.sequence = state.sequence; wire.deadlines = state.deadlines;
        wire.callers = state.callers; wire.failure = state.failure;
        if (state.active) {
            const auto& active = *state.active;
            wire.busy = wire.owned = true; wire.succeeded = active.succeeded; wire.phase = static_cast<int>(active.phase); wire.delay = active.delay;
            wire.request = active.request; wire.operation = static_cast<int>(active.operation); wire.debugTicket = active.debugTicket;
            wire.progress = active.Cursor(); wire.plan.push_back(active.ticket);
            auto actorIndex = [&](Handle handle) -> std::int32_t {
                if (!handle) return -1;
                const auto found = std::ranges::find(wire.actors, handle);
                if (found != wire.actors.end()) return static_cast<std::int32_t>(found - wire.actors.begin());
                wire.actors.push_back(handle); return static_cast<std::int32_t>(wire.actors.size() - 1);
            };
            for (const auto& step : active.steps) {
                const auto actor = actorIndex(step.actor);
                // SwapPrimary uses its wire argument for the previous primary's actor-pool index.
                const auto argument = step.effect == Effect::SwapPrimary ? actorIndex(step.otherActor) : step.argument;
                wire.plan.push_back(wire_v1::EncodeEffect(step.effect, step.alias, argument)); wire.plan.push_back(actor);
            }
        }
        for (const auto& command : state.queue) {
            wire.queue.insert(wire.queue.end(), {command.id, static_cast<int>(command.operation), command.selected, command.message, command.sayLine, command.debugTicket});
            wire.queueActors.push_back(command.actor);
        }
        for (const auto& receipt : state.receipts)
            wire.receipts.insert(wire.receipts.end(), {receipt.request, receipt.success ? 1 : 0, receipt.ticket, receipt.cursor, static_cast<int>(receipt.operation), receipt.debugTicket});
        return wire;
    }
    inline bool ValidState(const State& state) {
        if (state.queue.size() > command_rules::Capacity || state.receipts.size() > command_rules::Capacity ||
            state.callers.size() > 32 || state.deadlines.size() != 9 || state.failure.size() > 4096) return false;
        if (state.active && (state.active->steps.size() > 63 || state.active->ticket != state.ticket ||
            state.active->next > state.active->steps.size())) return false;
        if (state.active) for (const auto& step : state.active->steps) {
            if (step.alias < -1 || step.alias > 15 || step.argument < -2 || step.argument > 97) return false;
            if (step.effect == Effect::SwapPrimary) {
                if (!step.actor || step.actor == step.otherActor || step.alias < 2 || step.alias > 8 || step.argument != 0) return false;
            } else if (step.otherActor) return false;
        }
        return ValidCheckpoint(Checkpoint(state));
    }
    inline std::vector<std::uint8_t> EncodeState(const State& state) {
        return ValidState(state) ? EncodeCheckpoint(Checkpoint(state)) : std::vector<std::uint8_t>{};
    }
    inline bool DecodeState(std::span<const std::uint8_t> data, State& result) {
        wire_v1::Checkpoint wire;
        if (!DecodeCheckpoint(data, wire)) return false;
        State state;
        state.followerCount = wire.followerCount; state.speaker = wire.speaker; state.ticket = wire.ticket;
        state.outcome = wire.outcome; state.sequence = wire.sequence; state.deadlines = std::move(wire.deadlines);
        state.callers = std::move(wire.callers); state.failure = std::move(wire.failure);
        if (wire.busy) {
            ActiveOperation active;
            active.ticket = wire.ticket; active.request = wire.request; active.operation = static_cast<Operation>(wire.operation); active.debugTicket = wire.debugTicket;
            active.next = static_cast<std::size_t>((wire.progress - 1) / 2); active.phase = static_cast<Phase>(wire.phase);
            active.succeeded = wire.succeeded; active.delay = wire.delay;
            for (std::size_t i = 1; i + 1 < wire.plan.size(); i += 2) {
                const auto index = wire.plan[i + 1];
                const auto effect = wire_v1::DecodeEffect(wire.plan[i]);
                const auto argument = wire_v1::DecodeArgument(wire.plan[i]);
                const auto other = effect == Effect::SwapPrimary && argument >= 0 ? wire.actors[argument] : 0;
                active.steps.push_back({effect, wire_v1::DecodeAlias(wire.plan[i]),
                    index >= 0 ? wire.actors[index] : 0, effect == Effect::SwapPrimary ? 0 : argument, other});
            }
            state.active = std::move(active);
        }
        for (std::size_t i = 0; i < wire.queue.size(); i += wire_v1::Width)
            state.queue.push_back({wire.queue[i], static_cast<Operation>(wire.queue[i + 1]), wire.queueActors[i / wire_v1::Width],
                wire.queue[i + 2], wire.queue[i + 3], wire.queue[i + 4], wire.queue[i + 5]});
        for (std::size_t i = 0; i < wire.receipts.size(); i += wire_v1::Width)
            state.receipts.push_back({wire.receipts[i], wire.receipts[i + 1] != 0, wire.receipts[i + 2], wire.receipts[i + 3], static_cast<Operation>(wire.receipts[i + 4]), wire.receipts[i + 5]});
        if (!ValidState(state)) return false;
        result = std::move(state); return true;
    }
}
