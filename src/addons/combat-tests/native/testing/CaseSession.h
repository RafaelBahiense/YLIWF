#pragma once
#include "Definition.h"
#include "Resources.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace mod::combat_tests::testing {
    struct PhaseResult {
        const char* name = "";
        float elapsed = 0;
        bool started = false, entered = false, completed = false;
    };

    // One case repetition. Engine preparation/receipt polling remains in the
    // shared backend. This owns phase boundaries and scoped hook execution.
    class CaseSession {
    public:
        bool Begin(const TestCase& test, Context& context) {
            if (test_ || test.phases.empty())
                return false;
            test_ = &test;
            context_ = &context;
            results_.assign(test.phases.size(), {});
            for (std::size_t i = 0; i < results_.size(); ++i)
                results_[i].name = test.phases[i].name.c_str();
            return true;
        }

        Result Prepare() {
            if (fault_.state == State::Failed)
                return fault_;
            if (!test_ || !context_)
                return Result::Failed("No case is active.");
            if (!prepared_) {
                context_->scope = Scope::Case;
                if (!prepareCalled_ && test_->prepare) {
                    const auto result = test_->prepare(*context_);
                    if (result.state != State::Ready)
                        return Remember(result);
                }
                prepareCalled_ = true;
                while (actor_ < test_->actors.size()) {
                    const auto result = test_->actors[actor_].Spawn(*context_);
                    if (result.state != State::Ready)
                        return Remember(result);
                    ++actor_;
                }
                prepared_ = true;
            }
            return {};
        }

        Result Enter() {
            if (fault_.state == State::Failed)
                return fault_;
            if (Done())
                return {};
            auto& result = results_[index_];
            if (result.entered)
                return {};
            result.started = true;
            context_->advancePhase = false;
            context_->phaseElapsed = 0;
            context_->scope = Scope::Phase;
            const auto& commands = test_->phases[index_].enterActions;
            while (entryCommand_ < commands.size()) {
                const auto step = context_->Apply(commands[entryCommand_]);
                if (step.state != State::Ready)
                    return Remember(step);
                ++entryCommand_;
            }
            if (const auto& hook = test_->phases[index_].enter; hook) {
                const auto step = hook(*context_);
                if (step.state != State::Ready)
                    return Remember(step);
            }
            result.entered = true;
            return {};
        }

        Result Tick(float delta) {
            if (fault_.state == State::Failed)
                return fault_;
            if (!prepared_ || !std::isfinite(delta) || delta < 0 || delta > 1)
                return Remember(Result::Failed("Invalid case clock or preparation state."));
            if (Done())
                return {};
            const auto entry = Enter();
            if (entry.state != State::Ready)
                return entry;
            auto& result = results_[index_];
            const auto& phase = test_->phases[index_];
            if (!leaving_) {
                result.elapsed += delta;
                context_->phaseElapsed = result.elapsed;
                if (phase.tick && !context_->completion) {
                    const auto step = phase.tick(*context_);
                    if (step.state != State::Ready)
                        return Remember(step);
                }
                leaving_ =
                    result.elapsed >= phase.seconds || context_->advancePhase || context_->completion.has_value();
            }
            if (!leaving_)
                return Result::Pending();
            const auto close = ClosePhase();
            if (close.state != State::Ready)
                return close;
            result.completed = true;
            ended_ = context_->completion.has_value();
            ++index_;
            entryCommand_ = 0;
            leaving_ = exitCalled_ = false;
            if (context_->completion || Done())
                return {};
            const auto next = Enter();
            return next.state == State::Failed ? next : Result::Pending();
        }

        Result Restore() {
            if (!test_ || !context_)
                return {};
            if (!Done() && results_[index_].started) {
                const auto close = ClosePhase();
                if (close.state != State::Ready)
                    return close;
            }
            if (!caseExitCalled_) {
                context_->scope = Scope::Case;
                if (test_->cleanup) {
                    const auto result = test_->cleanup(*context_);
                    if (result.state != State::Ready)
                        return result;
                }
                caseExitCalled_ = true;
            }
            return context_->resources.Close(Scope::Case);
        }

        bool Done() const {
            return test_ && (ended_ || index_ >= test_->phases.size());
        }

        bool Prepared() const {
            return prepared_;
        }

        std::uint32_t Index() const {
            return static_cast<std::uint32_t>(index_);
        }

        const Phase* Current() const {
            return test_ && !Done() ? &test_->phases[index_] : nullptr;
        }

        const std::vector<PhaseResult>& Results() const {
            return results_;
        }

        void ResetForLoad() {
            *this = {};
        }

    private:
        Result Remember(Result result) {
            if (result.state == State::Failed)
                fault_ = result;
            return result;
        }

        Result ClosePhase() {
            if (!exitCalled_) {
                if (const auto& hook = test_->phases[index_].leave; hook) {
                    const auto result = hook(*context_);
                    if (result.state != State::Ready)
                        return result;
                }
                exitCalled_ = true;
            }
            return context_->resources.Close(Scope::Phase);
        }

        const TestCase* test_ = nullptr;
        Context* context_ = nullptr;
        std::vector<PhaseResult> results_;
        Result fault_;
        std::size_t index_ = 0;
        std::size_t actor_ = 0;
        std::size_t entryCommand_ = 0;
        bool prepared_ = false, prepareCalled_ = false, leaving_ = false, exitCalled_ = false, caseExitCalled_ = false,
             ended_ = false;
    };
}
