#pragma once
#include "scenarios/PrimitiveSuite.h"
#include "testing/SuiteSession.h"
#include <vector>

namespace primitive_test {
    namespace harness = mod::combat_tests::testing;
    using mod::combat_tests::primitive::Kind;

    struct Driver {
        harness::SuiteSession session;
        std::vector<harness::Action> requests;
        unsigned disposed = 0;
        bool reject = false;

        bool Begin(Kind kind) {
            session.context.execute = [this](const harness::Command& command) {
                if (command.action == harness::Action::Spawn) {
                    session.context.resources.Own(command.scope, command.actor.key, 10, [this] {
                        ++disposed;
                        return harness::Result{};
                    });
                } else if (command.action >= harness::Action::Guard) {
                    requests.push_back(command.action);
                    if (reject)
                        session.context.Finish(harness::Verdict::Inconclusive, "Rejected before dispatch.");
                }
                return harness::Result{};
            };
            return session.Begin(mod::combat_tests::primitive::Definition, 0) &&
                   session.BeginCase(static_cast<unsigned>(kind), 1, 1) &&
                   session.current.Prepare().state == harness::State::Ready &&
                   session.current.Enter().state == harness::State::Ready;
        }

        void Tick(float delta, const yliwf::testing::DefenderObservation& evidence) {
            session.context.observation = evidence;
            session.current.Tick(delta);
        }
    };
}
