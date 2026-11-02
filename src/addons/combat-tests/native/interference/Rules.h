#pragma once

#include <cstdint>
#include <string_view>

namespace mod::combat_tests::interference::rules {
    constexpr bool Selected(std::uint32_t selected, std::uint32_t actor) {
        return selected != 0 && selected == actor;
    }
    enum class Event : std::uint32_t { Other, BlockStop, BashStop };

    constexpr Event Classify(std::string_view name) {
        return name == "blockStop" ? Event::BlockStop : name == "bashStop" ? Event::BashStop : Event::Other;
    }

    constexpr bool Observe(std::uint32_t selected, std::uint32_t actor, bool sourceMatches, Event event) {
        return selected && selected == actor && sourceMatches && event != Event::Other;
    }

    constexpr bool Suppress(bool bypass, bool observed) {
        return bypass && observed;
    }
}
