#pragma once
#include <cstdint>
#include <string>
#include <utility>

namespace mod::combat_tests::testing {
    inline constexpr std::uint32_t DefaultRepeats = 1, MaxRepeats = 100;

    constexpr bool ValidRepeats(std::uint32_t repeats) {
        return repeats >= 1 && repeats <= MaxRepeats;
    }

    constexpr std::uint32_t CaseMask(std::uint32_t id) {
        return id < 32 ? 1u << id : 0;
    }

    struct Tag {
        std::string key;

        explicit Tag(std::string value) : key(std::move(value)) {}

        bool operator==(const Tag&) const = default;
    };

    struct RunOptions {
        std::uint32_t repeats = DefaultRepeats, selection = 0;
        bool expandVariations = false;
    };

    struct NamedSelection {
        std::string name;
        Tag tag;
        bool expandVariations = false;
    };
}
