#pragma once
#include "CaseSession.h"
#include "Definition.h"
#include "Resources.h"
#include <any>
#include <cstdint>
#include <optional>
#include <utility>

namespace mod::combat_tests::testing {
    class SuiteSession {
    public:
        Context context;
        CaseSession current;

        template <class Settings>
        bool Begin(const Suite& suite, const Settings& settings) {
            if (suite_ || !context.resources.Empty() || !suite.Valid())
                return false;
            suite_ = &suite;
            context.configuration = settings;
            baseline_ = settings;
            return true;
        }

        Result Prepare() {
            if (!suite_)
                return Result::Failed("No suite is active.");
            if (!prepared_) {
                context.scope = Scope::Suite;
                if (suite_->prepare) {
                    const auto result = suite_->prepare(context);
                    if (result.state != State::Ready)
                        return result;
                }
                prepared_ = true;
            }
            return {};
        }

        bool BeginCase(std::uint32_t id, std::uint32_t repetition, std::uint32_t generation,
                       std::optional<std::uint32_t> variant = std::nullopt) {
            if (!suite_ || !generation || !repetition)
                return false;
            const auto* test = suite_->Find(id);
            if (!test || !caseClosed_)
                return false;
            auto configuration = baseline_;
            if (!test->ConfigureVariant(variant, configuration))
                return false;
            context.configuration = std::move(configuration);
            context.observation.reset();
            context.completion.reset();
            context.advancePhase = false;
            current.ResetForLoad();
            context.elapsed = context.phaseElapsed = 0;
            context.repetition = repetition;
            context.generation = generation;
            caseClosed_ = false;
            return current.Begin(*test, context);
        }

        Result RestoreCase() {
            if (caseClosed_)
                return {};
            const auto result = current.Restore();
            if (result.state == State::Ready)
                caseClosed_ = true;
            return result;
        }

        Result Close() {
            if (!suite_)
                return {};
            if (!caseClosed_)
                return Result::Failed("Case restoration must finish before closing the suite.");
            context.scope = Scope::Suite;
            if (!cleanupCalled_) {
                if (suite_->cleanup) {
                    const auto result = suite_->cleanup(context);
                    if (result.state != State::Ready)
                        return result;
                }
                cleanupCalled_ = true;
            }
            const auto result = context.resources.Close(Scope::Suite);
            if (result.state == State::Ready) {
                suite_ = nullptr;
                prepared_ = cleanupCalled_ = false;
            }
            return result;
        }

        void ResetForLoad() {
            current.ResetForLoad();
            context.resources.ResetForLoad();
            context.configuration.reset();
            context.observation.reset();
            context.completion.reset();
            context.advancePhase = false;
            baseline_.reset();
            suite_ = nullptr;
            prepared_ = cleanupCalled_ = false;
            caseClosed_ = true;
        }

    private:
        const Suite* suite_ = nullptr;
        std::any baseline_;
        bool prepared_ = false, cleanupCalled_ = false, caseClosed_ = true;
    };
}
