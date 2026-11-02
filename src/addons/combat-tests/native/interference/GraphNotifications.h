#pragma once
#include <cstdint>
#include <string_view>

namespace RE {
    class Actor;
}

namespace mod::combat_tests::interference::graph_notifications {
    using Filter = bool (*)(std::uint32_t actor, std::string_view event, std::uintptr_t caller);
    void Install(Filter filter);
    bool Verified(RE::Actor* actor);
}
