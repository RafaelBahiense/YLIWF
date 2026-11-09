#include <doctest/doctest.h>
#include "configuration/TuningProfile.h"
#include <cstring>

using namespace yliwf::testing;
using namespace mod::combat_tests::tuning;

TEST_CASE("Tuning profiles preserve all parameter values through a strict file round trip") {
    DefenderTuning value;
    std::strcpy(value.name, "Caster spacing");
    value.lateralStep = 400;
    value.movementRest = .1f;
    value.recoveryEnter = .25f;
    value.recoveryExit = .55f;
    value.challengeEnabled = 0;
    const auto text = Serialize(value);
    const auto parsed = Parse(text);
    REQUIRE(parsed);
    CHECK(std::string_view(parsed->name) == value.name);
    CHECK(parsed->challengeEnabled == 0);
    for (const auto& field : TuningFields)
        CHECK(parsed.value().*field.member == value.*field.member);
    CHECK_FALSE(TuningError(*parsed));
}

TEST_CASE("Profile ranges, structure size, names and coupled thresholds are validated") {
    DefenderTuning value;
    CHECK_FALSE(TuningError(value));
    for (const auto& field : TuningFields) {
        auto invalid = value;
        invalid.*field.member = std::numeric_limits<float>::quiet_NaN();
        CHECK(TuningError(invalid));
        invalid.*field.member = field.maximum + 1;
        CHECK(TuningError(invalid));
        invalid.*field.member = field.minimum - 1;
        CHECK(TuningError(invalid));
    }
    value.recoveryEnter = .6f;
    value.recoveryExit = .5f;
    CHECK(TuningError(value));
    value.recoveryEnter = .4f;
    value.recoveryExit = .45f;
    CHECK_FALSE(TuningError(value));
    value = {};
    value.movementBurst = .25f;
    CHECK(TuningError(value));
    value.movementRefresh = .1f;
    value.guardSettle = .4f;
    CHECK(TuningError(value));
    value = {};
    --value.size;
    CHECK(TuningError(value));
    value = {};
    value.challengeEnabled = 2;
    CHECK(TuningError(value));
    value = {};
    std::memset(value.name, 'x', sizeof(value.name));
    CHECK(TuningError(value));
    value = {};
    value.name[0] = ',';
    CHECK(TuningError(value));
}

TEST_CASE("Profile files reject omissions, duplicates, unknown fields and partial numeric input") {
    const auto valid = Serialize({});
    CHECK_FALSE(Parse(valid + "lateral_step=400\n"));
    CHECK_FALSE(Parse(valid + "unknown=1\n"));
    CHECK_FALSE(Parse(valid.substr(valid.find('\n') + 1)));
    CHECK_FALSE(Parse(valid + "name=Another\n"));
    auto invalid = valid;
    const auto position = invalid.find("lateral_step=256");
    REQUIRE(position != std::string::npos);
    invalid.replace(position, std::strlen("lateral_step=256"), "lateral_step=256garbage");
    CHECK_FALSE(Parse(invalid));
    CHECK_FALSE(Parse(std::string(8193, 'x')));
    CHECK_FALSE(Parse("schema=2\nname=Candidate\n"));
    CHECK(Parse("# candidate\n" + valid));
    CHECK(Parse(std::string("\xEF\xBB\xBF") + valid));
}

TEST_CASE("Challenge profiles accept existing files and reject ambiguous switches") {
    const auto valid = Serialize({});
    auto previous = valid;
    const auto position = previous.find("challenge_enabled=1\n");
    REQUIRE(position != std::string::npos);
    previous.erase(position, std::strlen("challenge_enabled=1\n"));
    const auto parsed = Parse(previous);
    REQUIRE(parsed);
    CHECK(parsed->challengeEnabled == 1);
    CHECK_FALSE(Parse(valid + "challenge_enabled=0\n"));
    CHECK_FALSE(Parse(previous + "challenge_enabled=2\n"));
    CHECK_FALSE(Parse(previous + "challenge_enabled=true\n"));
}
