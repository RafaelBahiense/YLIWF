#pragma once
#include <cstdint>
#include <string_view>

namespace mod::combat_tests::telemetry::rules {
    constexpr bool Active(std::uint32_t readable, std::uint32_t flags) {
        // bUND_IsDodgeRoll selects a dodge style; it is not an active-dodge flag.
        return (readable & flags & 1) != 0;
    }

    constexpr char Lower(char ch) {
        return ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch + ('a' - 'A')) : ch;
    }

    constexpr bool Contains(std::string_view text, std::string_view pattern) {
        if (pattern.size() > text.size())
            return false;
        for (std::size_t i = 0; i + pattern.size() <= text.size(); ++i) {
            bool same = true;
            for (std::size_t j = 0; j < pattern.size(); ++j)
                if (Lower(text[i + j]) != pattern[j]) {
                    same = false;
                    break;
                }
            if (same)
                return true;
        }
        return false;
    }

    constexpr bool DodgeEvent(std::string_view tag) {
        return Contains(tag, "dodge") || Contains(tag, "tkdr") || Contains(tag, "dmco");
    }

    constexpr bool GuardEvent(std::string_view tag) {
        return Contains(tag, "block") || Contains(tag, "bash");
    }

    constexpr bool Relevant(std::string_view tag) {
        return DodgeEvent(tag) || GuardEvent(tag) || Contains(tag, "mco") || Contains(tag, "attack") ||
               Contains(tag, "swing") || Contains(tag, "hitframe") || Contains(tag, "stagger") ||
               Contains(tag, "recoil");
    }
}
