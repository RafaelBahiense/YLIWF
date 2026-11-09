#include <doctest/doctest.h>
#include "telemetry/DefenseMetrics.h"
#include "interference/DodgeBudgetRules.h"
#include <string_view>

TEST_CASE("Dodge budget retains reserve and spaces decisions without admitting stale snapshots") {
    using namespace mod::combat_tests::interference::dodge_budget_rules;
    CHECK(Check(100, 200, 0, 130, 390, 50, .2f) == Denial::None);
    CHECK(Check(100, 200, 0, 127, 390, 50, .2f) == Denial::Reserve);
    CHECK(Check(100, 200, 150, 200, 390, 50, .2f) == Denial::Cooldown);
    CHECK(Check(201, 200, 0, 200, 390, 50, .2f) == Denial::Snapshot);
    CHECK(Valid(.2f, 2));
    CHECK_FALSE(Valid(-.1f, 2));
    CHECK_FALSE(Valid(.2f, 11));
}

TEST_CASE("Recovery declines discretionary dodges without waiving emergency resource checks") {
    using namespace mod::combat_tests::interference::dodge_budget_rules;
    CHECK(Check(100, 200, 0, 200, 390, 50, .2f, true, false) == Denial::Recovery);
    CHECK(Check(100, 200, 0, 200, 390, 50, .2f, true, true) == Denial::None);
    CHECK(Check(100, 200, 0, 127, 390, 50, .2f, true, true) == Denial::Reserve);
    CHECK(Check(100, 200, 150, 200, 390, 50, .2f, true, true) == Denial::Cooldown);
    CHECK(Check(201, 200, 0, 200, 390, 50, .2f, true, true) == Denial::Snapshot);
    CHECK(Check(201, 200, 0, 200, 390, 50, .2f, true, false) == Denial::Snapshot);
}

TEST_CASE("Dodge loss excludes contaminated episodes from the clean subset") {
    using namespace mod::combat_tests::metrics;
    Tracker tracker;
    tracker.Observe({0, 200, 390, 630});
    tracker.Observe({.1f, 170, 390, 630, true});
    tracker.Observe({.5f, 172, 390, 630});
    tracker.Observe({.6f, 140, 390, 620, true, false, false, 1});
    tracker.Observe({1, 141, 390, 620, false, false, false, 1});
    const auto result = tracker.Read();
    CHECK(result.dodgeEpisodes == 2);
    CHECK(result.cleanDodgeEpisodes == 1);
    CHECK(result.cleanDodgeNetLoss == 30);
    CHECK(result.dodgeNetLoss == 62);
    CHECK(result.damage == 10);
}

TEST_CASE("Recovery success requires a live exit rather than run teardown") {
    using namespace mod::combat_tests::metrics;
    Tracker tracker;
    tracker.Observe({0, 10, 390, 100, false, false, true});
    tracker.Observe({2, 260, 390, 90, false, false, false});
    tracker.Observe({3, 5, 390, 80, false, false, true});
    tracker.Finish();
    const auto result = tracker.Read();
    CHECK(result.recoveryEntries == 2);
    CHECK(result.recoveryExits == 1);
    CHECK(result.completedRecoverySeconds == 2);
    CHECK(result.lowStaminaSeconds == 1);
    CHECK(result.damage == 20);
    CHECK(std::string_view(Unavailable(true, false, true, false, false, true, true, true)).find("downed") !=
          std::string_view::npos);
    CHECK(Unavailable(true, false, false, false, false, true, true, true) == nullptr);
}

TEST_CASE("Recovery evidence excludes refilled startup state and unreadable or subthreshold exits") {
    using namespace mod::combat_tests::metrics;
    Tracker tracker;
    tracker.Observe({0, 390, 390, 630, false, false, true});
    tracker.Observe({.1f, 390, 390, 630});
    CHECK(tracker.Read().recoveryEntries == 0);
    CHECK(tracker.Read().recoveryExits == 0);
    tracker.Observe({1, 58.5f, 390, 630, false, false, true});
    CHECK(tracker.Read().recoveryEntries == 1);
    Sample unknown{2, 300, 390, 630};
    unknown.recoveryReadable = false;
    tracker.Observe(unknown);
    CHECK(tracker.Read().recoveryExits == 0);
    tracker.Observe({3, 150, 390, 630});
    CHECK(tracker.Read().recoveryExits == 0);
    tracker.Observe({4, 260, 390, 630});
    CHECK(tracker.Read().recoveryExits == 1);
    tracker.Observe({5, 300, 390, 630});
    CHECK(tracker.Read().recoveryExits == 1);
}

TEST_CASE("Recovery evidence honors the applied thresholds instead of fixed default values") {
    using namespace mod::combat_tests::metrics;
    Tracker tracker;
    Sample entry{0, 20, 100, 100, false, false, true};
    entry.recoveryEntryFraction = .25f;
    entry.recoveryExitFraction = .5f;
    tracker.Observe(entry);
    entry.elapsed = 1;
    entry.stamina = 50;
    entry.recovering = false;
    tracker.Observe(entry);
    CHECK(tracker.Read().recoveryExits == 1);
}

TEST_CASE("Unavailable graph samples and truncated dodges cannot enter the clean cost subset") {
    using namespace mod::combat_tests::metrics;
    Tracker tracker;
    tracker.Observe({0, 200, 390, 630});
    tracker.Observe({.1f, 170, 390, 630, true});
    tracker.Observe({.2f, 169, 390, 630, false, false, false, 0, 0, 0, false});
    tracker.Observe({.3f, 168, 390, 630, true});
    tracker.Finish();
    tracker.Finish();
    const auto result = tracker.Read();
    CHECK(result.dodgeEpisodes == 1);
    CHECK(result.cleanDodgeEpisodes == 0);
    CHECK(result.partialDodgeEpisodes == 1);
    CHECK(result.dodgeUnavailableSamples == 1);
    CHECK(result.dodgeNetLoss == 32);
}
