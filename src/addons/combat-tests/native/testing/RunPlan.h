#pragma once
#include "Definition.h"
#include "CaseCatalog.h"
#include "Selection.h"
#include <any>
#include <cstdint>
#include <optional>
#include <span>

namespace mod::combat_tests::testing {
    struct ScheduledCase {
        std::uint32_t id = UINT32_MAX, repetition = 0, variant = 0;
        bool parameterized = false;
    };

    class RunPlan {
    public:
        RunPlan(const CaseCatalog& catalog, RunOptions options)
            : RunPlan(catalog.Definition(), catalog.Order(), options.repeats, options.selection,
                      options.expandVariations) {}

        RunPlan(const Suite& suite, std::span<const std::uint32_t> order, std::uint32_t repeats,
                std::uint32_t selection, bool variations = false)
            : suite_(suite), order_(order), repeats_(repeats), selection_(selection), variations_(variations) {}

        bool Valid() const {
            if (!suite_.Valid() || !ValidRepeats(repeats_) || !selection_ || order_.size() != suite_.cases.size())
                return false;
            std::uint32_t seen = 0;
            for (const auto id : order_) {
                if (id >= 32 || !suite_.Find(id) || (seen & (1u << id)))
                    return false;
                seen |= 1u << id;
            }
            return !(selection_ & ~seen);
        }

        std::uint32_t Runs() const {
            if (!Valid())
                return 0;
            std::uint32_t count = 0;
            for (const auto& test : suite_.cases)
                if (selection_ & (1u << test.id))
                    count += Repeats(test) * Variations(test);
            return count;
        }

        std::uint32_t CaseRuns(std::uint32_t id) const {
            if (!Valid() || id >= 32 || !(selection_ & (1u << id)))
                return 0;
            const auto* test = suite_.Find(id);
            return test ? Repeats(*test) * Variations(*test) : 0;
        }

        std::uint32_t CaseRepeats(std::uint32_t id) const {
            const auto* test = Valid() ? suite_.Find(id) : nullptr;
            return test ? Repeats(*test) : 0;
        }

        const TestCase* ForRun(std::uint32_t run) const {
            return suite_.Find(At(run).id);
        }

        const Variant* VariantForRun(std::uint32_t run) const {
            const auto scheduled = At(run);
            const auto* test = suite_.Find(scheduled.id);
            if (scheduled.parameterized && test)
                for (const auto& variant : test->variants)
                    if (variant.id == scheduled.variant)
                        return &variant;
            return nullptr;
        }

        template <class Settings>
        bool Configure(std::uint32_t run, Settings& settings) const {
            const auto scheduled = At(run);
            const auto* test = suite_.Find(scheduled.id);
            if (!test)
                return false;
            std::any configuration = settings;
            if (!test->ConfigureVariant(scheduled.parameterized ? std::optional{scheduled.variant} : std::nullopt,
                                        configuration))
                return false;
            const auto* configured = std::any_cast<Settings>(&configuration);
            if (!configured)
                return false;
            settings = *configured;
            return true;
        }

        float Seconds() const {
            if (!Valid())
                return 0;
            float total = 0;
            for (const auto& test : suite_.cases)
                if (selection_ & (1u << test.id))
                    total += test.Seconds() * static_cast<float>(Repeats(test) * Variations(test));
            return total;
        }

        ScheduledCase At(std::uint32_t run) const {
            if (!run || !Valid())
                return {};
            auto ordinal = run - 1;
            for (const auto id : order_) {
                if (!(selection_ & (1u << id)))
                    continue;
                const auto& test = *suite_.Find(id);
                const auto variations = Variations(test), count = Repeats(test) * variations;
                if (ordinal >= count) {
                    ordinal -= count;
                    continue;
                }
                const auto repetition = ordinal / variations;
                auto variation = ordinal % variations;
                if (test.alternateVariants && repetition % 2)
                    variation = variations - 1 - variation;
                const bool parameterized = variations_ && !test.variants.empty();
                return {id, repetition + 1, parameterized ? test.variants[variation].id : 0, parameterized};
            }
            return {};
        }

    private:
        std::uint32_t Repeats(const TestCase& test) const {
            return test.repeats ? test.repeats : repeats_;
        }

        std::uint32_t Variations(const TestCase& test) const {
            return variations_ && !test.variants.empty() ? static_cast<std::uint32_t>(test.variants.size()) : 1;
        }

        const Suite& suite_;
        std::span<const std::uint32_t> order_;
        std::uint32_t repeats_, selection_;
        bool variations_;
    };
}
