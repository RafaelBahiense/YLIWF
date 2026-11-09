#pragma once
#include <cstdint>

namespace mod::target_preference {
    void Install();
    bool Available();
    void Publish(std::uint32_t enemy, std::uint32_t defender);
    void Clear();
    std::uint32_t Rejections();
}
