#include <doctest/doctest.h>

#include "ControllerRules.h"
#include "CommandRules.h"
#include "ExecutorRules.h"
#include "StateCodec.h"

#include <array>
#include <unordered_map>

using namespace mod::controller_rules;

namespace {
    struct Actor {
        bool teammate = false, waiting = false, timer = false, protectedActor = false;
    };

    struct Game {
        std::vector<Slot> slots{{0, 0, true}, {2, 0, true}, {3, 0, true}, {4, 0, true}};
        std::unordered_map<std::uint32_t, Actor> actors;
        int dismissLine = 0;

        Slot* Alias(int id) {
            for (auto& slot : slots)
                if (slot.id == id)
                    return &slot;
            return nullptr;
        }

        // Engine stand-in: execute the policy's edits and reject an unexpected occupant.
        bool Step(const Plan& plan, std::size_t offset) {
            const auto& step = plan.steps[offset];
            auto effect = step.effect;
            auto* slot = Alias(step.alias);
            auto id = step.actor;
            auto argument = step.argument;
            auto& actor = actors[id];
            auto* expected = Find(slots, id);
            if (slot &&
                !ExpectedOccupant(effect, slot->occupied, slot->actor, id, expected && expected->dead, argument))
                return false;
            if (effect == Effect::SwapPrimary) {
                auto* primary = Alias(0);
                if (!primary || !slot || !CanSwapPrimary(*primary, *slot, id, step.otherActor))
                    return false;
                const auto oldWaiting = primary->waiting;
                primary->actor = id;
                primary->occupied = true;
                primary->dead = false;
                primary->waiting = slot->waiting;
                slot->actor = step.otherActor;
                slot->occupied = step.otherActor != 0;
                slot->dead = false;
                slot->waiting = slot->occupied ? oldWaiting : 0;
            } else if (effect == Effect::Assign) {
                if (!slot || slot->occupied)
                    return false;
                slot->actor = id;
                slot->occupied = true;
                slot->dead = false;
            } else if (effect == Effect::Clear) {
                if (!slot)
                    return false;
                slot->actor = 0;
                slot->occupied = false;
                slot->dead = false;
            } else if (effect == Effect::CancelTimer)
                actor.timer = false;
            else if (effect == Effect::StartTimer)
                actor.timer = true;
            else if (effect == Effect::Teammate)
                actor.teammate = argument == 1;
            else if (effect == Effect::Waiting) {
                actor.waiting = argument != 0;
                for (auto& own : slots)
                    if (own.actor == id)
                        own.waiting = actor.waiting ? 1.0f : 0.0f;
            } else if (effect == Effect::Prepare) {
                actor.protectedActor = true;
                if (!argument)
                    actor.waiting = false;
            } else if (effect == Effect::Release) {
                actor.protectedActor = actor.waiting = false;
            } else if (effect == Effect::Protection)
                actor.protectedActor = argument != 0;
            else if (effect == Effect::DismissLine)
                dismissLine = 1;
            else if (effect == Effect::EndDismissLine)
                dismissLine = 0;
            return true;
        }

        bool Execute(const Plan& plan, std::size_t start = 0) {
            if (!plan.accepted)
                return false;
            for (std::size_t i = start; i < plan.steps.size(); ++i)
                if (!Step(plan, i))
                    return false;
            return true;
        }

        Request For(Operation operation, std::uint32_t actor = 0) {
            Request request;
            request.operation = operation;
            request.actor = actor;
            request.actorValid = actor != 0;
            request.cap = 4;
            auto* own = Find(slots, actor);
            request.actorDead = own && own->dead;
            return request;
        }

        bool Run(Operation operation, std::uint32_t actor = 0) {
            return Execute(Build(slots, {}, For(operation, actor)));
        }
    };

    static_assert(mod::command_rules::Capacity == 16);
    static_assert(static_cast<int>(mod::command_rules::State::Verified) == 4 &&
                  static_cast<int>(mod::command_rules::State::Failed) == 5);

    TEST_CASE("Clear Rejects Missing Alias") {
        Game game;
        Plan invalid;
        invalid.Add(Effect::Clear, 999, 200);
        REQUIRE((!game.Execute(invalid)));
    }

    TEST_CASE("Version One Save Layout") {
        using namespace mod::controller::storage;
        // Fixed fixture for the current YLIWF co-save format (version one), independent of the encoder.
        // This checks save-format stability; it is not an SFF/YLIF migration fixture.
        // Both steps reference actor-pool index zero; the saved cursor resumes at the second step.
        const std::uint32_t versionOneWords[]{0,  0, 0,          1, 0, 1,  1, 0, 9,   0, 0, 0,     0, 0,     0, 0,
                                              0,  0, 7,          3, 4, 10, 0, 0, 11,  5, 7, 20102, 0, 20009, 0, 6,
                                              11, 2, 0xFFFFFFFF, 0, 1, 0,  0, 1, 200, 0, 1, 400,   0, 0,     0};
        std::vector<std::uint8_t> versionOneBytes;
        for (auto word : versionOneWords)
            for (unsigned shift = 0; shift < 32; shift += 8)
                versionOneBytes.push_back(static_cast<std::uint8_t>(word >> shift));
        State expected;
        expected.ticket = 7;
        expected.sequence = 11;
        ActiveOperation active;
        active.ticket = 7;
        active.request = 10;
        active.operation = Operation::Dismiss;
        active.phase = Phase::Engine;
        active.next = 1;
        active.steps = {{Effect::Clear, 0, 200, 0}, {Effect::Evaluate, -1, 200, 0}};
        expected.active = active;
        expected.queue = {{11, Operation::Follow, 400}};
        State decoded;
        REQUIRE((DecodeState(versionOneBytes, decoded) && decoded == expected));
        REQUIRE((EncodeState(expected) == versionOneBytes));
        for (auto operation : {Operation::ReservedPrepare, Operation::ReservedCleanup, Operation::ReservedPromote,
                               Operation::ReservedPop, Operation::ReservedAddExtra, Operation::ReservedClearDead})
            REQUIRE((!SupportedOperation(operation) && !Build({}, {}, {.operation = operation}).accepted));
    }

    TEST_CASE("Empty Save Round Trip") {
        using namespace mod::controller::storage;
        State state;
        REQUIRE((ValidState(state)));
        State decoded;
        REQUIRE((DecodeState(EncodeState(state), decoded) && decoded == state));
    }

    mod::controller::storage::State SavedEngineState() {
        using namespace mod::controller::storage;
        State state;
        // Save at an accepted engine call: loading must preserve its phase and offset.
        state.ticket = 7;
        state.sequence = 10;
        state.speaker = 100;
        ActiveOperation active;
        active.ticket = 7;
        active.request = 10;
        active.operation = Operation::Dismiss;
        active.phase = Phase::Engine;
        active.steps = {{Effect::Clear, 0, 200, 0}};
        state.active = active;
        state.deadlines[0] = 123.5f;
        state.callers = {{10, 42, 300, "DialogueFollowerScript", "DismissFollower"}};
        state.sequence = 11;
        state.queue = {{11, Operation::Follow, 400, -1, 0, 1, 0}};
        return state;
    }

    TEST_CASE("Pending Engine Call Round Trip") {
        using namespace mod::controller::storage;
        auto state = SavedEngineState();
        State decoded;
        const auto bytes = EncodeState(state);
        REQUIRE((!bytes.empty() && DecodeState(bytes, decoded) && decoded == state));
    }

    TEST_CASE("Save Rejects Malformed Data") {
        using namespace mod::controller::storage;
        auto state = SavedEngineState();
        State decoded;
        const auto bytes = EncodeState(state);
        // Every truncation and trailing data is rejected without replacing current state.
        for (std::size_t size = 0; size < bytes.size(); ++size) {
            State sentinel = state;
            REQUIRE((!DecodeState(std::span(bytes).first(size), sentinel) && sentinel == state));
        }
        auto trailing = bytes;
        trailing.push_back(0);
        REQUIRE((!DecodeState(trailing, decoded)));
        auto invalid = state;
        invalid.active->delay = std::numeric_limits<float>::quiet_NaN();
        REQUIRE((!ValidState(invalid) && EncodeState(invalid).empty()));
        invalid = state;
        invalid.active->next = 2;
        REQUIRE((!ValidState(invalid)));
        invalid = state;
        invalid.active->steps[0].alias = 100;
        REQUIRE((!ValidState(invalid)));
        invalid = state;
        invalid.queue.push_back(invalid.queue.front());
        REQUIRE((!ValidState(invalid)));
        invalid = state;
        invalid.callers.push_back(invalid.callers.front());
        REQUIRE((!ValidState(invalid)));
        auto oversized = bytes;
        // First vector count begins after count, speaker, flags, phase, and delay.
        for (std::size_t i = 32; i < 36; ++i)
            oversized[i] = 0xFF;
        REQUIRE((!DecodeState(oversized, decoded)));
    }

    TEST_CASE("Save Handle Remapping Is Transactional") {
        using namespace mod::controller::storage;
        auto state = SavedEngineState();
        State decoded;
        auto remapped = state;
        REQUIRE((RemapState(remapped, [](std::uint64_t& handle) {
            handle += 1000;
            return true;
        })));
        REQUIRE((remapped.speaker == 1100 && remapped.active->steps[0].actor == 1200 &&
                 remapped.queue[0].actor == 1400 && remapped.callers[0].self == 1300 &&
                 remapped.callers[0].stack == 42));
        auto unresolved = state;
        REQUIRE((!RemapState(unresolved, [](std::uint64_t& handle) {
            if (handle == 200)
                return false;
            handle += 1;
            return true;
        })));
        REQUIRE((unresolved == state));  // Resolution is transactional.
        auto optionalSpeaker = state;
        REQUIRE((RemapState(optionalSpeaker, [](std::uint64_t& handle) { return handle != 100; }) &&
                 !optionalSpeaker.speaker));
    }

    TEST_CASE("Save Clears UI Tickets") {
        using namespace mod::controller::storage;
        auto state = SavedEngineState();
        auto checkpoint = state;
        checkpoint.active->debugTicket = checkpoint.queue[0].debugTicket = 73;
        ClearUITickets(checkpoint);
        REQUIRE((!checkpoint.active->debugTicket && !checkpoint.queue[0].debugTicket &&
                 checkpoint.queue[0] == state.queue[0]));
    }

    TEST_CASE("Latent Caller Identity") {
        using namespace mod::controller::storage;
        auto state = SavedEngineState();
        State decoded;
        state.callers[0].result = 1;
        REQUIRE((DecodeState(EncodeState(state), decoded) && decoded.callers[0].result == 1));
        auto caller = state.callers[0];
        auto same = caller;
        same.type = "dialoguefollowerscript";
        same.function = "dismissfollower";
        REQUIRE((SameCaller(caller, same)));
        same.stack++;
        REQUIRE((!SameCaller(caller, same)));
        same = caller;
        same.self++;
        REQUIRE((!SameCaller(caller, same)));
    }

    TEST_CASE("Saved Delays And Reserved Effects") {
        using namespace mod::controller::storage;
        auto state = SavedEngineState();
        State decoded;
        state.callers[0].result = 1;
        // Pause/delay remains a native save field rather than a restarted two seconds.
        state.active->phase = Phase::Delay;
        state.active->delay = 1.25f;
        REQUIRE((DecodeState(EncodeState(state), decoded) && decoded.active->delay == 1.25f &&
                 decoded.active->phase == Phase::Delay));
        // Saved effect ID 10 remains readable even though current plans never emit it.
        static_assert(static_cast<int>(Effect::ReservedUpdate) == 10);
        state.active->phase = Phase::Ready;
        state.active->delay = 0;
        state.active->steps = {{Effect::ReservedUpdate, -1, 0, 0}};
        REQUIRE((DecodeState(EncodeState(state), decoded) && decoded == state));
        REQUIRE((Build({}, {}, {}).steps.empty()));  // An idle Sync needs only completion/count publication.
    }

    TEST_CASE("Saved Primary Swap And Handle Remapping") {
        using namespace mod::controller::storage;
        auto state = SavedEngineState();
        State decoded;
        state.callers[0].result = 1;
        // Both captured swap actors survive save/load and SKSE handle remapping.
        state.active->operation = Operation::PromotePrimary;
        state.active->phase = Phase::Engine;
        state.active->steps = {{Effect::SwapPrimary, 2, 200, 0, 500}};
        REQUIRE((DecodeState(EncodeState(state), decoded) && decoded == state));
        REQUIRE((RemapState(decoded, [](auto& handle) {
            handle += 1000;
            return true;
        })));
        REQUIRE((decoded.active->steps[0].actor == 1200 && decoded.active->steps[0].otherActor == 1500));
        auto unresolvedPrevious = state;
        REQUIRE((!RemapState(unresolvedPrevious, [](auto& handle) { return handle != 500; }) &&
                 unresolvedPrevious == state));
        auto invalidSwap = state;
        invalidSwap.active->steps[0].otherActor = 200;
        REQUIRE((!ValidState(invalidSwap)));
        auto checkpointSwap = Checkpoint(state);
        // The new effect's wire argument must resolve within the actor pool.
        checkpointSwap.plan[1] = wire_v1::EncodeEffect(Effect::SwapPrimary, 2, 31);
        REQUIRE((!ValidCheckpoint(checkpointSwap)));
        state.active->steps[0].otherActor = 0;
        REQUIRE((DecodeState(EncodeState(state), decoded) && decoded == state));
        const auto expectations = mod::command_rules::Expectations(state.active->steps);
        REQUIRE((expectations.size() == 2 && expectations[0].alias == 0 && expectations[0].actor == 200 &&
                 expectations[1].alias == 2 && expectations[1].actor == 0 && expectations[1].value == 0));
        REQUIRE((IsHomeEffect(Effect::HomeEvaluate) && !IsHomeEffect(Effect::SwapPrimary)));
    }

    TEST_CASE("Waiting Deadlines And Primary Swap") {
        using namespace mod::executor_rules;
        std::array<float, 9> deadlines{};
        REQUIRE((SetDeadline(deadlines, 0, 100.0f) && deadlines[0] == 172.0f));
        REQUIRE((SetDeadline(deadlines, 2, 200.0f) && deadlines[2] == 272.0f));
        REQUIRE((SetDeadline(deadlines, 0, std::nullopt) && deadlines[0] == 0 && deadlines[2] == 272.0f));
        const auto unchanged = deadlines;
        REQUIRE((!SetDeadline(deadlines, -1, 100.0f) && !SetDeadline(deadlines, 9, 100.0f)));
        REQUIRE(
            (!SetDeadline(deadlines, 2, -1.0f) && !SetDeadline(deadlines, 2, std::numeric_limits<float>::quiet_NaN())));
        REQUIRE((!SetDeadline(deadlines, 2, std::numeric_limits<float>::infinity()) && deadlines == unchanged));
        REQUIRE((!SetDeadline(std::span(deadlines).first(8), 0, std::nullopt)));
        deadlines[0] = 172.0f;
        deadlines[2] = 272.0f;
        REQUIRE((SwapPrimaryDeadlines(deadlines, 2, true) && deadlines[0] == 272.0f && deadlines[2] == 172.0f));
        REQUIRE((SwapPrimaryDeadlines(deadlines, 2, false) && deadlines[0] == 172.0f && deadlines[2] == 0));
        const auto swapped = deadlines;
        REQUIRE((!SwapPrimaryDeadlines(deadlines, 1, true) && !SwapPrimaryDeadlines(deadlines, 9, true) &&
                 deadlines == swapped));
    }

    TEST_CASE("Paused And Reloaded Dismissal Delay") {
        using namespace mod::executor_rules;
        // A saved delay resumes its remaining duration; pause/load time cannot consume it.
        const auto saved = Remaining(2, 0.75f, false, false);
        REQUIRE((saved == 1.25f && Remaining(saved, 100, true, false) == saved));
        REQUIRE((Remaining(saved, 100, false, true) == saved && Remaining(saved, 100, false, false) == 0));
        REQUIRE((ValidDelay(0) && ValidDelay(2) && !ValidDelay(-1) && !ValidDelay(3)));
    }

    TEST_CASE("Engine Acknowledgement Identity") {
        using namespace mod::executor_rules;
        // Saved engine calls acknowledge once at the same ticket/offset; never replay.
        REQUIRE((CanAcknowledge(true, 7, 3, Phase::Engine, 7, 3)));
        REQUIRE((CanAcknowledge(true, 7, 3, Phase::Counts, 7, 3)));
        REQUIRE((!CanAcknowledge(true, 7, 5, Phase::Engine, 7, 3)));
        REQUIRE((!CanAcknowledge(true, 8, 3, Phase::Engine, 7, 3)));
        REQUIRE((!CanAcknowledge(false, 7, 3, Phase::Engine, 7, 3)));
        REQUIRE((!CanAcknowledge(true, 7, 3, Phase::Delay, 7, 3)));
        REQUIRE((!KnownPhase(static_cast<Phase>(4))));
    }

    TEST_CASE("Party Target Selection") {
        const std::array<Slot, 6> slots{{{0, 101, true, true},
                                         {2, 102, true, true},
                                         {3, 101, true, true},
                                         {4, 103, true, true, true},
                                         {1, 104, true, true},
                                         {5, 105, false, true}}};
        REQUIRE((PartyTargets(slots, Operation::Follow) == (std::vector<std::uint32_t>{101, 102})));
        REQUIRE((PartyTargets(slots, Operation::Wait) == (std::vector<std::uint32_t>{101, 102})));
        REQUIRE((PartyTargets(slots, Operation::Dismiss) == (std::vector<std::uint32_t>{101, 102, 103})));
        REQUIRE((PartyTargets(slots, Operation::Recruit).empty()));
        REQUIRE((PartyTargets({}, Operation::Dismiss).empty()));
    }

    TEST_CASE("Party Capacity And Progress") {
        using namespace mod::command_rules;
        REQUIRE((CanAppend(8, 8, 100) && CanAppend(0, 16, 100)));
        REQUIRE((!CanAppend(9, 8, 100) && !CanAppend(0, 0, 100)));
        REQUIRE((!CanAppend(0, 2, std::numeric_limits<std::int32_t>::max() - 1)));
        GroupProgress group({11, 12, 13});
        REQUIRE((group.Record(12, false) && !group.Done() && group.failed == 1));
        REQUIRE((!group.Record(12, true) && !group.Record(99, true)));
        REQUIRE((group.Record(11, true) && group.Record(13, true) && group.Done()));
        REQUIRE((group.completed == 3 && group.failed == 1));
        GroupProgress success({1});
        REQUIRE((success.Record(1, true) && success.Done() && success.failed == 0));
    }

    TEST_CASE("Party Commands Resolve Promoted Slots") {
        // Commands resolve actors again after every promotion, never stale slot IDs.
        for (auto operation : {Operation::Follow, Operation::Wait, Operation::Dismiss}) {
            Game game;
            game.slots.clear();
            for (int i = 0; i < 8; ++i) {
                const auto id = static_cast<std::uint32_t>(101 + i);
                game.slots.push_back({i == 0 ? 0 : i + 1, id, true, true, false, 1});
                game.actors[id] = {true, true, true, true};
            }
            const auto targets = PartyTargets(game.slots, operation);
            REQUIRE((targets.size() == 8));
            for (auto id : targets) {
                auto request = game.For(operation, id);
                request.selected = PartySelection;
                request.message = -1;
                request.sayLine = 0;
                const auto plan = Build(game.slots, {}, request);
                // Each individual payload fits the saved Papyrus array limit.
                REQUIRE((plan.steps.size() * 2 + 1 <= 128));
                REQUIRE((game.Execute(plan)));
                if (operation == Operation::Dismiss)
                    REQUIRE((!Find(game.slots, id) && !game.actors[id].teammate));
                else
                    REQUIRE((game.actors[id].teammate && game.actors[id].waiting == (operation == Operation::Wait)));
            }
            REQUIRE((Count(game.slots) == (operation == Operation::Dismiss ? 0 : 8)));
        }
    }

    TEST_CASE("Party Dismissal Clears Duplicate And Dead Slots") {
        Game game;
        game.slots = {
            {0, 101, true, true}, {2, 101, true, true}, {3, 102, true, true, true}, {4, 103, true, true, true}};
        for (auto id : {101u, 102u, 103u})
            game.actors[id] = {true, true, true, true};
        const auto targets = PartyTargets(game.slots, Operation::Dismiss);
        REQUIRE((targets.size() == 3));
        for (auto id : targets) {
            auto request = game.For(Operation::Dismiss, id);
            request.selected = PartySelection;
            request.message = -1;
            request.sayLine = 0;
            request.actorDead = id != 101;  // Engine death state survives alias reconciliation.
            REQUIRE((game.Execute(Build(game.slots, {}, request))));
            REQUIRE((!Find(game.slots, id) && !game.actors[id].teammate && !game.actors[id].protectedActor));
        }
        REQUIRE((Count(game.slots) == 0));
        auto orphan = game.For(Operation::Dismiss, 104);
        orphan.selected = PartySelection;
        REQUIRE((!Build(game.slots, {}, orphan).accepted));  // A living actor that left is not released.
        orphan.actorDead = true;
        REQUIRE((!Build(game.slots, {1, 104, true, true, true}, orphan).accepted));  // Never release the animal.
        orphan.selected = -1;
        REQUIRE((!Build(game.slots, {}, orphan).accepted));  // Ordinary requests retain their ownership check.
    }

    TEST_CASE("Queue Admission And Maintenance Coalescing") {
        using namespace mod::command_rules;
        using mod::controller::storage::Command;
        std::vector<Command> queue{{1, Operation::Sync}};
        REQUIRE((Coalesce(queue, Operation::Sync, 0) == 1));
        REQUIRE((!Coalesce(queue, Operation::Sync, 73)));
        queue.push_back({2, Operation::Wait, 100});
        REQUIRE((!Coalesce(queue, Operation::Sync, 0)));
        queue.push_back({3, Operation::Sync});
        REQUIRE((Coalesce(queue, Operation::Sync, 0) == 3));
        REQUIRE((CanStartImmediately(true, true, false, false, false, 0)));
        REQUIRE((!CanStartImmediately(false, true, false, false, false, 0)));
        REQUIRE((!CanStartImmediately(true, false, false, false, false, 0)));
        REQUIRE((!CanStartImmediately(true, true, true, false, false, 0)));
        REQUIRE((!CanStartImmediately(true, true, false, true, false, 0)));
        REQUIRE((!CanStartImmediately(true, true, false, false, true, 0)));
        REQUIRE((!CanStartImmediately(true, true, false, false, false, 1)));
    }

    TEST_CASE("Acknowledgements Use Final Plan State") {
        using namespace mod::command_rules;
        const std::vector<mod::controller::storage::Step> plan{{Effect::Assign, 0, 100, 1},
                                                               {Effect::Waiting, -1, 100, 1},
                                                               {Effect::Clear, 0, 100, 0},
                                                               {Effect::Assign, 2, 100, 0},
                                                               {Effect::Waiting, -1, 100, 0}};
        REQUIRE((Acknowledgement(0, 1, plan.size())));
        REQUIRE((!Acknowledgement(1, 1, plan.size())));
        REQUIRE((!Acknowledgement(0, 3, plan.size())));
        REQUIRE((!Acknowledgement(plan.size(), 11, plan.size())));
        const auto expected = Expectations(plan);
        auto check = [&](Check kind, int key) {
            auto entry = std::ranges::find_if(expected, [&](auto item) {
                return item.kind == kind && (kind == Check::Waiting ? item.actor == 100 : item.alias == key);
            });
            REQUIRE((entry != expected.end()));
            return entry->value;
        };
        REQUIRE((check(Check::FollowerAlias, 0) == 0));  // The final clear supersedes initial assignment.
        REQUIRE((check(Check::FollowerAlias, 2) == 1));  // Same saved actor payload, different slot.
        REQUIRE((check(Check::Waiting, 0) == 0));        // A later Follow supersedes Wait.
        std::size_t resumedOffset = 2;                   // Saved executor already acknowledged two steps.
        REQUIRE((!Acknowledgement(resumedOffset, 1, plan.size())));
        REQUIRE((Acknowledgement(resumedOffset, 5, plan.size())));
    }

    TEST_CASE("Command Clock Excludes Paused Time") {
        using namespace mod::command_rules;
        ActiveClock clock;
        const auto start = clock.last;
        clock.Observe(start + std::chrono::seconds(10), true);
        clock.Observe(start + std::chrono::seconds(100), false);
        REQUIRE((clock.elapsed == std::chrono::seconds(10)));  // Ninety paused seconds are excluded.
        clock.Observe(start + std::chrono::seconds(121), false);
        REQUIRE((clock.elapsed >= std::chrono::seconds(30)));
        clock.Progress(start + std::chrono::seconds(121), false);
        REQUIRE((clock.elapsed.count() == 0));  // Late progress clears the warning, without replay.
    }

    TEST_CASE("Primary Promotion Preserves Party State") {
        Game party;
        for (std::uint32_t id = 100; id < 104; ++id)
            REQUIRE((party.Run(Operation::Recruit, id)));
        REQUIRE((party.Run(Operation::Wait, 100) && party.Run(Operation::Wait, 102)));
        REQUIRE((party.Run(Operation::PromotePrimary, 102)));
        REQUIRE((party.slots[0].actor == 102 && party.slots[2].actor == 100 && Count(party.slots) == 4));
        REQUIRE((party.actors[100].waiting && party.actors[102].waiting && party.actors[100].timer &&
                 party.actors[102].timer));
        REQUIRE((party.actors[100].teammate && party.actors[102].teammate && party.dismissLine == 0));
        REQUIRE((party.Run(Operation::PromotePrimary, 102) && party.slots[0].actor == 102));
        REQUIRE((party.Run(Operation::Follow, 100)));
        REQUIRE((party.Run(Operation::PromotePrimary, 100) && party.slots[0].actor == 100));
        REQUIRE((!party.actors[100].waiting && !party.actors[100].timer && party.actors[102].waiting &&
                 party.actors[102].timer));
        auto lowerCap = party.For(Operation::PromotePrimary, 103);
        lowerCap.cap = 1;
        REQUIRE((party.Execute(Build(party.slots, {}, lowerCap)) && Count(party.slots) == 4));
        REQUIRE((!party.Run(Operation::PromotePrimary, 999)));
        const auto pendingSwap = Build(party.slots, {}, party.For(Operation::PromotePrimary, 101));
        auto changed = party;
        changed.slots[0].actor = 999;
        REQUIRE((!changed.Execute(pendingSwap) && changed.slots[0].actor == 999));
        auto dead = party;
        Find(dead.slots, 101)->dead = true;
        REQUIRE((!dead.Run(Operation::PromotePrimary, 101)));
        auto deadPrimary = party;
        deadPrimary.slots[0].dead = true;
        REQUIRE((!deadPrimary.Run(Operation::PromotePrimary, 101)));
        auto duplicates = party;
        duplicates.slots[3] = {4, 101, true, true};
        REQUIRE((!duplicates.Run(Operation::PromotePrimary, 101)));
        Game emptyPrimary;
        emptyPrimary.slots[1] = {2, 100, true, true, false, 1};
        emptyPrimary.actors[100] = {true, true, true, true};
        REQUIRE((emptyPrimary.Run(Operation::PromotePrimary, 100)));
        REQUIRE(
            (emptyPrimary.slots[0].actor == 100 && !emptyPrimary.slots[1].occupied && Count(emptyPrimary.slots) == 1));
        REQUIRE((emptyPrimary.actors[100].waiting && emptyPrimary.actors[100].timer));
    }

    TEST_CASE("Recruitment Waiting And Dismissal Continuation") {
        Game game;
        for (std::uint32_t id = 100; id < 104; ++id)
            REQUIRE((game.Run(Operation::Recruit, id)));
        REQUIRE((Count(game.slots) == 4 && game.slots[0].actor == 100));
        REQUIRE((!game.Run(Operation::Recruit, 104)));
        REQUIRE((game.Run(Operation::Recruit, 102)));  // Repeated recruitment does not change the primary or count.
        REQUIRE((Count(game.slots) == 4 && game.slots[0].actor == 100));
        auto reduced = game.For(Operation::Sync);
        reduced.cap = 1;
        REQUIRE((game.Execute(Build(game.slots, {}, reduced)) && Count(game.slots) == 4));
        REQUIRE((game.Run(Operation::Wait, 102)));
        REQUIRE((game.actors[102].waiting && game.actors[102].timer));
        REQUIRE((game.Run(Operation::Follow, 102)));
        REQUIRE((!game.actors[102].waiting && !game.actors[102].timer));
        REQUIRE((game.Run(Operation::Timeout, 102)));  // A queued timeout after Follow cannot dismiss the actor.
        REQUIRE((Find(game.slots, 102)));
        REQUIRE((game.Run(Operation::Wait, 100)));
        REQUIRE((game.Run(Operation::Unload, 100) && game.actors[100].timer));

        // Resume the same dismissal plan after its latent line, without rebuilding from a second roster.
        const auto dismissal = Build(game.slots, {}, game.For(Operation::Dismiss, 100));
        std::size_t resume = 0;
        for (; resume < dismissal.steps.size(); ++resume) {
            REQUIRE((game.Step(dismissal, resume)));
            if (dismissal.steps[resume].effect == Effect::DismissLine) {
                ++resume;
                break;
            }
        }
        REQUIRE((game.dismissLine == 1 && game.slots[0].actor == 100));
        auto loaded = game;
        const auto savedPlan = dismissal;
        REQUIRE((loaded.Execute(savedPlan, resume)));
        REQUIRE((loaded.slots[0].actor == 101 && Count(loaded.slots) == 3));
        REQUIRE((!loaded.actors[100].teammate && !loaded.actors[100].protectedActor && loaded.dismissLine == 0));
        REQUIRE((!loaded.Run(Operation::Dismiss, 100)));  // Repeated events cannot evict the promoted primary.
        REQUIRE((loaded.Run(Operation::Recruit, 104) && loaded.slots[0].actor == 101));
        REQUIRE((loaded.Run(Operation::Dismiss, 103) && loaded.slots[0].actor == 101));

        // An external alias edit during the dismissal wait is detected before clearing its new occupant.
        auto external = game;
        external.slots[0].actor = 999;
        REQUIRE((!external.Execute(savedPlan, resume)));
        REQUIRE((external.slots[0].actor == 999));
    }

    TEST_CASE("Reconciliation Clears Duplicates And Dead Slots") {
        Game duplicates;
        duplicates.slots[0] = {0, 100, true, true};
        duplicates.slots[1] = {2, 100, true, true};
        duplicates.slots[2] = {3, 200, true, true, true};
        REQUIRE((duplicates.Run(Operation::Repair, 100)));
        REQUIRE((duplicates.slots[0].actor == 100 && duplicates.slots[1].actor == 0));
        REQUIRE((duplicates.Run(Operation::Sync)));
        REQUIRE((duplicates.slots[2].actor == 0 && Count(duplicates.slots) == 1));
        duplicates.slots[0].dead = true;
        REQUIRE((duplicates.Run(Operation::Death, 100) && Count(duplicates.slots) == 0));
    }

    TEST_CASE("Reconciliation Preserves Resurrected Follower") {
        Game resurrected;
        resurrected.slots[0] = {0, 100, true, true, true};
        const auto cleanup = Build(resurrected.slots, {}, resurrected.For(Operation::Sync));
        resurrected.slots[0].dead = false;
        REQUIRE((!resurrected.Execute(cleanup) && resurrected.slots[0].actor == 100));
    }

    TEST_CASE("Dialogue Commands Promote Living Follower") {
        Game dialogue;
        dialogue.slots[0] = {0, 100, true, true, true};
        dialogue.slots[1] = {2, 200, true, true};
        REQUIRE((dialogue.Run(Operation::DialogueWait, 100)));
        REQUIRE((dialogue.slots[0].actor == 200 && dialogue.actors[200].waiting && dialogue.actors[200].timer));
        REQUIRE((dialogue.Run(Operation::DialogueDismiss, 200) && Count(dialogue.slots) == 0));
    }

    TEST_CASE("Unavailable Slots And Animal Ownership") {
        Game unavailable;
        unavailable.slots[0].valid = false;
        REQUIRE((!unavailable.Run(Operation::Recruit, 100)));
        unavailable.slots[0].valid = true;
        unavailable.slots[0].occupied = true;  // Non-actor references stay occupied.
        REQUIRE((unavailable.Run(Operation::Recruit, 100) && unavailable.slots[0].actor == 0));
        auto adopt = unavailable.For(Operation::Adopt, 200);
        REQUIRE((!Build(unavailable.slots, {}, adopt).accepted));
        adopt.recruitable = true;
        REQUIRE((unavailable.Execute(Build(unavailable.slots, {}, adopt))));
        auto release = unavailable.For(Operation::Release, 100);
        release.orphan = true;
        REQUIRE((!Build(unavailable.slots, {}, release).accepted));  // Cannot release a managed actor's flags.
        release.actor = 300;
        REQUIRE((unavailable.Execute(Build(unavailable.slots, {}, release))));

        Slot animal{1, 400, true, true};
        auto recruit = unavailable.For(Operation::Recruit, 400);
        REQUIRE((!Build(unavailable.slots, animal, recruit).accepted));
        REQUIRE((!Build(unavailable.slots, animal, unavailable.For(Operation::AnimalRecruit, 500)).accepted));
        REQUIRE((Build(unavailable.slots, animal, unavailable.For(Operation::AnimalWait, 400)).accepted));
    }

    TEST_CASE("Home Capacity And Marker Validation") {
        Game game;
        std::array<Slot, 8> homes;
        std::array<bool, 8> markers;
        markers.fill(true);
        for (int i = 0; i < 8; ++i)
            homes[i] = {i, static_cast<std::uint32_t>(i + 1), true, true};
        auto home = game.For(Operation::HomeAssign, 100);
        REQUIRE((!Home(homes, markers, home).accepted));  // Full home roster leaves all previous residents untouched.
        homes[4].actor = 100;
        REQUIRE((Home(homes, markers, home).accepted));  // Reassign a full roster's existing resident.
        markers[4] = false;
        REQUIRE((!Home(homes, markers, home).accepted));  // Validate the marker before clearing the resident.
        home.operation = Operation::HomeRemove;
        REQUIRE((Home(homes, markers, home).accepted));
    }

    TEST_CASE("Largest Recruitment Plan Fits Wire Limit") {
        std::vector<Slot> allDead{{0, 100, true, true, true}};
        for (int i = 2; i <= 8; ++i)
            allDead.push_back({i, static_cast<std::uint32_t>(100 + i), true, true, true});
        Request worst{Operation::Recruit, 999, -1, 0, 1, 8, true};
        const auto biggest = Build(allDead, {}, worst);
        REQUIRE((biggest.accepted && biggest.steps.size() * 2 + 1 <= 128));
    }

}
