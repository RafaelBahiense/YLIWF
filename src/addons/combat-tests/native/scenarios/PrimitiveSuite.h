#pragma once
#include "Scenario.h"
#include "../fixtures/Archetypes.h"
#include "../testing/Definition.h"
#include "../testing/Resources.h"
#include "../testing/Assessment.h"
#include "../testing/Preparation.h"
#include "CombatTestHost.h"
#include <array>
#include <cstdint>
#include <utility>

namespace mod::combat_tests::primitive {
    enum class Kind { GuardRelease, Bash, PowerBash, RightAttack };

    constexpr bool UsesBash(Kind kind) {
        return kind == Kind::Bash || kind == Kind::PowerBash;
    }

    inline testing::Result Observe(testing::Context& context, Kind kind, bool acquire) {
        using namespace testing;
        const auto* evidence = context.Observation<yliwf::testing::DefenderObservation>();
        if (!evidence)
            return Result::Failed("Primitive observation is unavailable.");
        if (evidence->actionFault || evidence->actionTimeouts || evidence->nativeBashTimeouts ||
            evidence->rightAttackTimeouts) {
            context.Finish(Verdict::Failed, "Primitive acknowledgement or cleanup unresolved; no retry.");
            return {};
        }
        if (acquire) {
            if (evidence->guardRejected)
                context.Finish(Verdict::Inconclusive, "Guard rejected by native eligibility or idle selection.");
            else if (evidence->guardObserved && evidence->guardPhase == 2)
                context.AdvancePhase();
        } else {
            const bool attack = kind == Kind::RightAttack, release = kind == Kind::GuardRelease;
            const bool rejected = attack    ? evidence->rightAttackRejected
                                  : release ? evidence->releaseRejected
                                            : evidence->nativeBashRejected;
            const bool interrupted =
                attack ? evidence->rightAttackInterrupted : !release && evidence->nativeBashInterrupted;
            const bool completed = attack    ? evidence->rightAttackCompleted
                                   : release ? evidence->releaseObserved && evidence->guardPhase == 0
                                             : evidence->nativeBashCompleted;
            if (rejected || interrupted)
                context.Finish(Verdict::Inconclusive,
                               "Requested action rejected or interrupted; no replacement request.");
            else if (completed)
                context.Finish(Verdict::Passed, "Requested action started and completed; cleanup confirmed.");
        }
        if (!context.completion && !context.advancePhase && context.phaseElapsed >= 8)
            context.Finish(Verdict::Inconclusive, acquire ? "No owned guard observed within the scenario window."
                                                          : "Follow-up did not finish within the scenario window.");
        return {};
    }

    inline testing::Phase Request(const char* name, float seconds, testing::Action action, Kind kind,
                                  bool acquire = false) {
        using namespace testing;
        return Phase{name, seconds}
            .Prepare([action](Context& context) { return context.Apply({action}); })
            .Observe([kind, acquire](Context& context) { return Observe(context, kind, acquire); });
    }

    inline const testing::Suite Definition = [] {
        using namespace testing;
        Suite suite{"Action primitives"};
        suite.Prepare(testing::PrepareArenaWithObserver);
        const std::array names{"Guard and release", "Normal bash", "Power bash", "Right-hand normal attack"};
        const std::array descriptions{
            "Request a guard, hold for one second, then release and verify cleanup. Defender tactics are bypassed.",
            "Acquire an owned guard, request one normal bash, then verify start and completion.",
            "Acquire an owned guard, request one power bash, then verify start and completion.",
            "Request one normal right-hand attack and verify its resolved action, start and cleanup."};
        for (std::uint32_t id = 0; id < names.size(); ++id) {
            const auto kind = static_cast<Kind>(id);
            TestCase test{id, names[id], descriptions[id]};
            test.Expect(kind).Prepare([](Context& context) { return context.PrepareSubject(); });
            test.Actor(fixtures::Spawner("opponent", fixtures::Archetype::Sword).Target("subject").At(0, 512));
            if (kind == Kind::RightAttack)
                test.Phase(Request("Attack", 20, Action::RightAttack, kind));
            else {
                test.Phase(Request("Acquire guard", 8, Action::Guard, kind, true));
                test.Phase(Phase{"Hold guard", 1}.Observe([](Context& context) {
                    const auto* evidence = context.Observation<yliwf::testing::DefenderObservation>();
                    if (!evidence || evidence->guardPhase != 2)
                        context.Finish(Verdict::Inconclusive, "Guard ownership changed before the follow-up request.");
                    return Result{};
                }));
                test.Phase(Request("Follow-up", 11,
                                   kind == Kind::GuardRelease ? Action::Release
                                   : kind == Kind::PowerBash  ? Action::PowerBash
                                                              : Action::Bash,
                                   kind));
            }
            suite.Case(std::move(test));
        }
        return suite;
    }();

    inline constexpr std::array Navigation{
        scenarios::Entry{
            2, 0, "Action primitives", "Guard and release",
            "Request a guard, hold for one second, then release and verify cleanup. Defender tactics are bypassed.",
            "Needs a one-handed weapon and shield.", &Definition, 0, 5, 3, scenarios::Execution::Primitive},
        scenarios::Entry{3, 0, "Action primitives", "Normal bash",
                         "Acquire an owned guard, request one normal bash, then verify start and completion.",
                         "Needs a one-handed weapon and shield.", &Definition, 1, 5, 3,
                         scenarios::Execution::Primitive},
        scenarios::Entry{4, 0, "Action primitives", "Power bash",
                         "Acquire an owned guard, request one power bash, then verify start and completion.",
                         "Needs a one-handed weapon, shield and the power-bash perk.", &Definition, 2, 5, 3,
                         scenarios::Execution::Primitive},
        scenarios::Entry{5, 0, "Action primitives", "Right-hand normal attack",
                         "Request one normal right-hand attack and verify its resolved action, start and cleanup.",
                         "Needs a one-handed melee weapon and shield; start without blocking.", &Definition, 3, 5, 3,
                         scenarios::Execution::Primitive}};
}
