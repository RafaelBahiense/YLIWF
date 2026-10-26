#pragma once

#include <cstdint>
#include <optional>
#include <span>

namespace mod::serana_reanimation {
    inline bool Suppress(std::uint32_t attacker, std::uint32_t blocked) {
        return blocked && attacker == blocked;
    }

    inline std::optional<bool> DecodePreference(std::uint32_t version, std::span<const std::uint8_t> bytes) {
        if (version != 1 || bytes.size() != 4 || bytes[0] > 1 || bytes[1] || bytes[2] || bytes[3])
            return {};
        return bytes[0] != 0;
    }
}
