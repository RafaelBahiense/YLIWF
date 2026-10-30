#include <doctest/doctest.h>
#include "TravelRules.h"
#include <array>

TEST_CASE("Arena travel excludes tests, unavailable players and pending movement") {
    using namespace mod::combat_tests::travel;
    CHECK(CanTravel(true, false, false, false));
    CHECK_FALSE(CanTravel(false, false, false, false));
    CHECK_FALSE(CanTravel(true, true, false, false));
    CHECK_FALSE(CanTravel(true, false, true, false));
    CHECK_FALSE(CanTravel(true, false, false, true));
}

TEST_CASE("Arena entry never overwrites a return bookmark") {
    using namespace mod::combat_tests::travel;
    CHECK(CanEnter(true, false, false));
    CHECK_FALSE(CanEnter(true, true, false));
    CHECK_FALSE(CanEnter(true, false, true));
    CHECK_FALSE(CanEnter(false, false, false));
    CHECK(CanReturn(true, true));
    CHECK_FALSE(CanReturn(true, false));
    CHECK_FALSE(CanReturn(false, true));
}

TEST_CASE("Arena bookmarks validate record boundaries, dynamic references and pending move IDs") {
    using namespace mod::combat_tests::travel;
    Bookmark bookmark{0xFF000123, Move::Return};
    std::array<std::uint8_t, sizeof(Bookmark)> bytes;
    std::memcpy(bytes.data(), &bookmark, sizeof(bookmark));
    REQUIRE(Decode(1, bytes));
    CHECK(Decode(1, bytes)->moving == Move::Return);
    CHECK_FALSE(Decode(2, bytes));
    CHECK_FALSE(Decode(1, std::span(bytes).first(4)));
    bookmark.origin = 0x00012345;
    std::memcpy(bytes.data(), &bookmark, sizeof(bookmark));
    CHECK_FALSE(Decode(1, bytes));
    bookmark.origin = 0xFF000123;
    bookmark.moving = static_cast<Move>(3);
    std::memcpy(bytes.data(), &bookmark, sizeof(bookmark));
    CHECK_FALSE(Decode(1, bytes));
}
