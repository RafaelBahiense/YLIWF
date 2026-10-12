#pragma once

#include <cstdint>
#include <string>

namespace mod::healer {
    void Configure();
    void Loaded();
    void Observe(std::uint32_t actor);
    void SpellReleased(std::uint32_t actor, std::uint32_t spell);
    void Tick(float elapsed);
    bool NeedsObservation();
    void Invalidate();
    std::string Status(std::uint32_t actor);
}
