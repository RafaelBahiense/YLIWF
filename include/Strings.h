#pragma once

#include <algorithm>
#include <string_view>

namespace mod::strings {
    // Papyrus identifiers and BSFixedString names ignore ASCII case.
    inline bool EqualsIgnoreCase(std::string_view left, std::string_view right) {
        auto lower = [](unsigned char character) {
            return character >= 'A' && character <= 'Z' ? character + ('a' - 'A') : character;
        };
        return std::ranges::equal(left, right, {}, lower, lower);
    }
}
