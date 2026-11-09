#include <doctest/doctest.h>
#include "fixtures/FixtureSpellRules.h"
#include <initializer_list>
#include <array>

using namespace mod::combat_tests::fixture_spells;

TEST_CASE("Fixture spell isolation preserves passives and the selected spell while excluding castable alternatives") {
    CHECK_FALSE(Remove(1, Kind::Spell, false, 1));
    CHECK_FALSE(Remove(0, Kind::Spell, false, 1));
    for (const auto kind : {Kind::Spell, Kind::Power, Kind::LesserPower, Kind::VoicePower, Kind::Scroll, Kind::Staff}) {
        CHECK(Remove(2, kind, false, 1));
        CHECK(Remove(2, kind, false, 0));
        CHECK_FALSE(Remove(2, kind, true, 1));
    }
    CHECK_FALSE(Remove(2, Kind::Passive, false, 1));
    CHECK_FALSE(Remove(2, Kind::Other, false, 1));
}

TEST_CASE("Fixture spell mutation requires a generated actor with its matching owned base") {
    CHECK(Owned(true, true, true));
    CHECK_FALSE(Owned(false, true, true));
    CHECK_FALSE(Owned(true, false, true));
    CHECK_FALSE(Owned(true, true, false));
}

TEST_CASE("Vision utility accepts the installed scripted Night Eye and rejects mixed combat effects") {
    constexpr Effect night{EffectKind::NightVision, false};
    constexpr Effect scripted{EffectKind::RegisteredScriptedVision, false};
    constexpr Effect unknownScript{EffectKind::Other, false};
    constexpr Effect damage{EffectKind::Other, true};
    constexpr Effect hostileVision{EffectKind::NightVision, true};
    CHECK(VisionOnly(true, std::array{night}));
    CHECK(VisionOnly(true, std::array{scripted}));
    CHECK(VisionOnly(true, std::array{night, scripted}));
    CHECK_FALSE(VisionOnly(false, std::array{scripted}));
    CHECK_FALSE(VisionOnly(true, {}));
    CHECK_FALSE(VisionOnly(true, std::array{unknownScript}));
    CHECK_FALSE(VisionOnly(true, std::array{night, damage}));
    CHECK_FALSE(VisionOnly(true, std::array{scripted, unknownScript}));
    CHECK_FALSE(VisionOnly(true, std::array{hostileVision}));
}
