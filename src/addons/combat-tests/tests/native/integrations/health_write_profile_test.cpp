#include <doctest/doctest.h>
#include "integrations/HealthWriteProfile.h"
using namespace mod::combat_tests::health_write_profile;

TEST_CASE("Captured combat health write calls the backend directly rather than the owner virtual") {
    constexpr std::array<std::uint8_t, 5> combat{0xE8, 0xCF, 0xA4, 0xFF, 0xFF};
    constexpr std::array<std::uint8_t, 5> owner{0xE8, 0x17, 0x00, 0x00, 0x00};
    CHECK(CallTarget(combat, 0x6B82CC) == 0x6B27A0);
    CHECK(CallTarget(owner, 0x6B2784) == 0x6B27A0);
    CHECK(CallTarget(std::array<std::uint8_t, 5>{0xE9, 0, 0, 0, 0}, 100) == 0);
    CHECK(CallTarget(std::span<const std::uint8_t>(combat.data(), 4), 100) == 0);
    CHECK(CallTarget(combat, 0) == 0);
}

TEST_CASE("Health write relays must fit every call's signed rel32 reach including allocation edges") {
    constexpr std::uintptr_t source = 0x100000000;
    CHECK(Reachable(source, source + 5 + 0x7FFFFFFF));
    CHECK_FALSE(Reachable(source, source + 5 + 0x80000000));
    CHECK(Reachable(source, source + 5 - 0x80000000));
    CHECK_FALSE(Reachable(source, source + 5 - 0x80000001));
    CHECK_FALSE(Reachable(std::numeric_limits<std::uintptr_t>::max(), source));
}
