#pragma once

#include <cstdint>

namespace RE {
    class Actor;
    class NiPoint3;
}

namespace mod::healing_geometry {
    struct Sight {
        bool available = false, clear = false;
        std::uint32_t blocker = 0;
    };

    Sight Check(RE::Actor* caster, RE::Actor* target);
    Sight From(RE::Actor* caster, const RE::NiPoint3& origin, RE::Actor* target);
}
