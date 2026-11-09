#pragma once
#include <cstdint>
#include "../testing/Definition.h"

namespace mod::combat_tests::fixtures {
    enum class Archetype : std::uint32_t {
        Sword,
        Heavy,
        Bow,
        Projectile,
        Concentration,
        Area,
        ArcherAlly,
        LeveledMage
    };

    constexpr std::uint32_t Template(Archetype archetype) {
        return archetype >= Archetype::Projectile && archetype != Archetype::ArcherAlly ? 0x44CD7 : 0xC3CA0;
    }

    inline testing::ActorSpawner Spawner(const char* key, Archetype archetype) {
        return testing::ActorSpawner{key}.Archetype(static_cast<std::uint32_t>(archetype));
    }
}
