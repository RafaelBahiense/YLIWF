#pragma once

#include <cstdint>
#include <cstring>
#include <optional>
#include <span>

namespace mod::combat_tests::travel {
    enum class Move : std::uint32_t { None, Enter, Return };

    struct Bookmark {
        std::uint32_t origin = 0;
        Move moving = Move::None;
    };

    inline std::optional<Bookmark> Decode(std::uint32_t version, std::span<const std::uint8_t> bytes) {
        if (version != 1 || bytes.size() != sizeof(Bookmark))
            return std::nullopt;
        Bookmark result;
        std::memcpy(&result, bytes.data(), sizeof(result));
        if ((result.origin & 0xFF000000) != 0xFF000000 || result.moving > Move::Return)
            return std::nullopt;
        return result;
    }

    constexpr bool CanTravel(bool ready, bool busy, bool unavailable, bool pending) {
        return ready && !busy && !unavailable && !pending;
    }

    constexpr bool CanEnter(bool allowed, bool atArena, bool hasReturn) {
        return allowed && !atArena && !hasReturn;
    }

    constexpr bool CanReturn(bool allowed, bool hasReturn) {
        return allowed && hasReturn;
    }
}
