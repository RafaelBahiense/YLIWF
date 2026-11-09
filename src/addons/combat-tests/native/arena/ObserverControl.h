#pragma once
#include <cstdint>
#include <span>

namespace mod::combat_tests::observer_control {
    void Install();
    bool Begin(std::uint32_t subject, std::uint32_t enemy, std::uint32_t auxiliary, bool ally, std::uint32_t token);
    bool Begin(std::span<const std::uint32_t> allies, std::span<const std::uint32_t> opponents, std::uint32_t token);
    void Refresh();
    void End();
    std::uint32_t Rejections();
    std::uint32_t FriendlyRejections();
}
