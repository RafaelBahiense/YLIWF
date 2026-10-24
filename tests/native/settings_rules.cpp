#include <doctest/doctest.h>
#include "settings/GameplaySettings.h"

namespace {
    using namespace mod::settings;

    TEST_CASE("Gameplay save round trip retains choices and plugin-local perk specifications") {
        GameplaySettings value{
            2, 1, 15, 2, 3, true, false, true, false, true, "Example.esp|00000801,Other.esl|00000802"};
        const auto bytes = EncodeGameplay(value);
        const auto loaded = DecodeGameplay(bytes);
        REQUIRE(loaded);
        CHECK(*loaded == value);
        auto other = value;
        other.limitMode = 2;
        CHECK(*DecodeGameplay(EncodeGameplay(other)) == other);
        CHECK(*DecodeGameplay(bytes) == value);
    }

    TEST_CASE("Gameplay settings reject truncation trailing bytes and invalid booleans") {
        const auto bytes = EncodeGameplay(GameplaySettings{});
        for (std::size_t length = 0; length < bytes.size(); ++length)
            CHECK_FALSE(DecodeGameplay(std::span(bytes).first(length)));
        auto invalid = bytes;
        invalid.push_back(0);
        CHECK_FALSE(DecodeGameplay(invalid));
        invalid = bytes;
        invalid[20] = 2;
        CHECK_FALSE(DecodeGameplay(invalid));
        invalid = bytes;
        invalid[0] = 8;
        CHECK_FALSE(DecodeGameplay(invalid));
    }

    TEST_CASE("Gameplay settings bound all enum and text fields before saving") {
        GameplaySettings value;
        for (auto* field : {&value.extraFollowers, &value.limitMode, &value.distance, &value.formation}) {
            const auto original = *field;
            *field = -1;
            CHECK(EncodeGameplay(value).empty());
            *field = 99;
            CHECK(EncodeGameplay(value).empty());
            *field = original;
        }
        value.speechLevels = 0;
        CHECK(EncodeGameplay(value).empty());
        value.speechLevels = 1;
        value.perks.assign(2048, 'a');
        CHECK(EncodeGameplay(value).empty());
        value.perks.assign(2047, 'a');
        CHECK(DecodeGameplay(EncodeGameplay(value)) == value);
        value.perks = std::string("a\0b", 3);
        CHECK(EncodeGameplay(value).empty());
        CHECK_FALSE(DecodeGameplay(std::vector<std::uint8_t>(MaxGameplayBytes + 1)));
    }
}
