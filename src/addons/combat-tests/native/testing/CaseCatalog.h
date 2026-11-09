#pragma once
#include "Definition.h"
#include "Selection.h"
#include <cstdint>
#include <vector>

namespace mod::combat_tests::testing {
    // Borrows immutable declarations; IDs need not be dense or declaration indices.
    class CaseCatalog {
    public:
        explicit CaseCatalog(const Suite& suite, std::uint32_t selectionBits = 32) : suite_(suite) {
            if (!suite.Valid() || !selectionBits || selectionBits > 32)
                return;
            for (const auto& test : suite.cases) {
                if (test.id >= selectionBits) {
                    order_.clear();
                    mask_ = 0;
                    return;
                }
                order_.push_back(test.id);
                mask_ |= 1u << test.id;
            }
            valid_ = true;
        }

        bool Valid() const {
            return valid_;
        }

        const Suite& Definition() const {
            return suite_;
        }

        std::uint32_t Tagged(const Tag& tag) const {
            std::uint32_t selected = 0;
            if (valid_)
                for (const auto& test : suite_.cases)
                    if (test.HasTag(tag))
                        selected |= CaseMask(test.id);
            return selected;
        }

        RunOptions Select(const NamedSelection& preset, std::uint32_t repeats = DefaultRepeats) const {
            return {repeats, Tagged(preset.tag), preset.expandVariations};
        }

        bool HasVariations(std::uint32_t selection) const {
            if (!valid_ || (selection & ~mask_))
                return false;
            for (const auto& test : suite_.cases)
                if ((selection & CaseMask(test.id)) && !test.variants.empty())
                    return true;
            return false;
        }

        std::uint32_t Mask() const {
            return mask_;
        }

        const std::vector<std::uint32_t>& Order() const {
            return order_;
        }

        const TestCase* Find(std::uint32_t id) const {
            return valid_ ? suite_.Find(id) : nullptr;
        }

    private:
        const Suite& suite_;
        std::vector<std::uint32_t> order_;
        std::uint32_t mask_ = 0;
        bool valid_ = false;
    };
}
