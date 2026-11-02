#include <doctest/doctest.h>
#include "interference/Rules.h"
#include "interference/UNDProfile.h"

TEST_CASE("Interruption comparison isolates the selected actor and source") {
    using namespace mod::combat_tests::interference::rules;
    CHECK(Classify("blockStop") == Event::BlockStop);
    CHECK(Classify("bashStop") == Event::BashStop);
    CHECK(Classify("blockStart") == Event::Other);
    CHECK(Observe(1, 1, true, Event::BlockStop));
    CHECK(Observe(1, 1, true, Event::BashStop));
    CHECK_FALSE(Observe(0, 0, true, Event::BlockStop));
    CHECK_FALSE(Observe(1, 2, true, Event::BlockStop));
    CHECK_FALSE(Observe(1, 1, false, Event::BlockStop));
    CHECK_FALSE(Observe(1, 1, true, Event::Other));
    CHECK(Suppress(true, true));
    CHECK_FALSE(Suppress(false, true));
    CHECK_FALSE(Suppress(true, false));
}

TEST_CASE("UND profile validates relocated forward and backward calls and rejects changed sites") {
    using mod::combat_tests::interference::und::profile::DirectCall;
    constexpr std::uintptr_t image = 0x7FFD00000000;
    constexpr std::array<std::uint8_t, 5> forward{0xE8, 0xD7, 0x50, 0, 0};
    constexpr std::array<std::uint8_t, 5> backward{0xE8, 0x5A, 0x4B, 0xFE, 0xFF};
    CHECK(DirectCall(forward, image + 0x2DC14, image + 0x32CF0));
    CHECK(DirectCall(backward, image + 0x4E191, image + 0x32CF0));
    CHECK_FALSE(DirectCall(backward, image + 0x4E191, image + 0x4CC40));
    auto modified = forward;
    modified[0] = 0xE9;
    CHECK_FALSE(DirectCall(modified, image + 0x2DC14, image + 0x32CF0));
    modified = forward;
    modified[1] ^= 1;
    CHECK_FALSE(DirectCall(modified, image + 0x2DC14, image + 0x32CF0));
}

TEST_CASE("UND trampoline must be reachable from every call rather than only the module base") {
    using mod::combat_tests::interference::und::profile::Reachable;
    constexpr std::uintptr_t image = 0x7FFD00000000;
    constexpr auto oldAllocation = image - 0x80000000;
    CHECK_FALSE(Reachable(image + 0x2DC14, oldAllocation));
    CHECK_FALSE(Reachable(image + 0x52FF7, oldAllocation));
    constexpr auto lower = image + 0x52FF7 + 5 - 0x80000000;
    CHECK(Reachable(image + 0x2DC14, lower));
    CHECK(Reachable(image + 0x52FF7, lower));
    CHECK(Reachable(image + 0x2DC14, lower + 511));
    CHECK(Reachable(image + 0x52FF7, lower + 511));
    CHECK_FALSE(Reachable(image + 0x52FF7, lower - 1));
    constexpr auto upper = image + 0x2DC14 + 5 + 0x7FFFFFFF;
    CHECK(Reachable(image + 0x2DC14, upper));
    CHECK_FALSE(Reachable(image + 0x2DC14, upper + 1));
}
