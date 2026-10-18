#include <doctest/doctest.h>

#include "combat/HealingRules.h"
#include "combat/HealerMovementRules.h"
#include <array>
#include <limits>

namespace {
    using namespace mod::healing_rules;

    Target InjuredAlly(std::uint32_t id, float health = 50, float distance = 100) {
        return {id, health, 100, distance, true, true, true, true, true, false};
    }

    TEST_CASE("Self healing does not require an ally ray or an unreserved target") {
        auto self = InjuredAlly(1, 40);
        self.visible = false;
        self.friendly = false;
        self.reserved = true;
        CHECK(SelfEligible(self, 1, true));
        CHECK_FALSE(Seekable(self, 1));
        self.health = 80;
        CHECK_FALSE(SelfEligible(self, 1, true));
        CHECK(SelfEligible(self, 1, false));
        self.bleedingOut = true;
        CHECK_FALSE(SelfEligible(self, 1, false));
    }

    TEST_CASE("Self healing respects caster identity, health and spell conditions") {
        auto self = InjuredAlly(1, 40);
        SUBCASE("Another actor") {
            self.id = 2;
        }
        SUBCASE("Dead") {
            self.alive = false;
        }
        SUBCASE("Unloaded") {
            self.loaded = false;
        }
        SUBCASE("Healthy") {
            self.health = 100;
        }
        SUBCASE("Invalid health") {
            self.health = std::numeric_limits<float>::quiet_NaN();
        }
        SUBCASE("Rejected spell") {
            self.spellApplicable = false;
        }
        CHECK_FALSE(SelfEligible(self, 1, true));
    }

    TEST_CASE("A moving healer fights back when pursuers prevent gaining separation") {
        EscapeAttempt escape;
        CHECK_FALSE(escape.Failed(200, 1));
        CHECK_FALSE(escape.Failed(230, PursuitTimeout - 1));
        CHECK(escape.Failed(180, 1));
        escape.Reset();
        CHECK_FALSE(escape.Failed(180, 1));
    }

    TEST_CASE("Meaningful escape progress resets the pursuit deadline") {
        EscapeAttempt escape;
        CHECK_FALSE(escape.Failed(200, 1));
        CHECK_FALSE(escape.Failed(210, PursuitTimeout - 1));
        CHECK_FALSE(escape.Failed(200 + EscapeProgress, 1));
        CHECK_FALSE(escape.Failed(270, PursuitTimeout - 1));
        CHECK(escape.Failed(270, 1));
        CHECK(Remaining(OffensiveInterval, OffensiveInterval) == 0);
    }

    TEST_CASE("A healer selects the most injured reachable ally, excluding itself") {
        std::array targets{InjuredAlly(1, 5), InjuredAlly(2, 40), InjuredAlly(3, 20), InjuredAlly(4, 10, 2000)};
        REQUIRE(Choose(targets, 1, MaximumRange) == 2);
        targets[2].health = 40;
        targets[2].distance = 50;
        REQUIRE(Choose(targets, 1, MaximumRange) == 2);
    }

    TEST_CASE("Healing respects death, hostility, loading, visibility and target reservations") {
        auto ally = InjuredAlly(2);
        REQUIRE(Eligible(ally, 1, MaximumRange));
        SUBCASE("Dead") {
            ally.alive = false;
        }
        SUBCASE("Unloaded") {
            ally.loaded = false;
        }
        SUBCASE("Hostile") {
            ally.friendly = false;
        }
        SUBCASE("Different area") {
            ally.sameArea = false;
        }
        SUBCASE("Behind a wall") {
            ally.visible = false;
        }
        SUBCASE("Another healer reserved it") {
            ally.reserved = true;
        }
        SUBCASE("Spell conditions reject this target") {
            ally.spellApplicable = false;
        }
        CHECK_FALSE(Eligible(ally, 1, MaximumRange));
    }

    TEST_CASE("Touch healing stays close and healthy allies are left alone") {
        auto ally = InjuredAlly(2, 69, TouchRange);
        CHECK(Eligible(ally, 1, TouchRange));
        ally.distance += 1;
        CHECK_FALSE(Eligible(ally, 1, TouchRange));
        ally.distance = 100;
        ally.health = 70;
        CHECK_FALSE(Eligible(ally, 1, MaximumRange));
    }

    TEST_CASE("Outside combat healers top up allies but stop near full health") {
        auto ally = InjuredAlly(2, 90);
        CHECK_FALSE(Seekable(ally, 1, true));
        CHECK(Seekable(ally, 1, false));
        CHECK(Eligible(ally, 1, TouchRange, false));
        ally.health = 100;
        CHECK_FALSE(Seekable(ally, 1, false));
        ally.health = 99.99f;
        CHECK_FALSE(Seekable(ally, 1, false));
        ally.health = 0;
        ally.bleedingOut = true;
        CHECK(Seekable(ally, 1, false));
        ally.alive = false;
        CHECK_FALSE(Seekable(ally, 1, false));
    }

    TEST_CASE("Invalid health and distance do not produce a healing target") {
        auto ally = InjuredAlly(2);
        SUBCASE("Zero maximum") {
            ally.maximumHealth = 0;
        }
        SUBCASE("Dead health") {
            ally.health = 0;
        }
        SUBCASE("Invalid health") {
            ally.health = std::numeric_limits<float>::quiet_NaN();
        }
        SUBCASE("Invalid maximum") {
            ally.maximumHealth = std::numeric_limits<float>::infinity();
        }
        SUBCASE("Invalid distance") {
            ally.distance = std::numeric_limits<float>::quiet_NaN();
        }
        CHECK_FALSE(Eligible(ally, 1, MaximumRange));
    }

    TEST_CASE("Paused time does not advance cooldowns and expiry never becomes negative") {
        CHECK(Remaining(Cooldown, 0) == Cooldown);
        CHECK(Remaining(Cooldown, -1) == Cooldown);
        CHECK(Remaining(Cooldown, 1) == Cooldown - 1);
        CHECK(Remaining(Cooldown, 100) == 0);
    }

    TEST_CASE("A healer can seek an injured ally beyond casting range or behind an obstacle") {
        auto ally = InjuredAlly(2, 20, 2000);
        ally.visible = false;
        CHECK(Seekable(ally, 1));
        CHECK_FALSE(Eligible(ally, 1, SpellRange(30, false)));
        ally.distance = SearchRange + 1;
        CHECK_FALSE(Seekable(ally, 1));
    }

    TEST_CASE("Seeking does not relax ally identity, health or ownership requirements") {
        auto ally = InjuredAlly(2, 20, 2000);
        SUBCASE("Dead") {
            ally.alive = false;
        }
        SUBCASE("Hostile") {
            ally.friendly = false;
        }
        SUBCASE("Unloaded") {
            ally.loaded = false;
        }
        SUBCASE("Another healer reserved it") {
            ally.reserved = true;
        }
        SUBCASE("Healthy") {
            ally.health = 90;
        }
        SUBCASE("Different area") {
            ally.sameArea = false;
        }
        SUBCASE("Spell conditions reject the ally") {
            ally.spellApplicable = false;
        }
        CHECK_FALSE(Seekable(ally, 1));
    }

    TEST_CASE("Spell range converts feet to the same units as actor coordinates") {
        CHECK(SpellRange(30, false) == doctest::Approx(640));
        CHECK(Eligible(InjuredAlly(2, 20, 407), 1, SpellRange(30, false)));
        CHECK_FALSE(Eligible(InjuredAlly(2, 20, 700), 1, SpellRange(30, false)));
        CHECK(SpellRange(1000, false) == MaximumRange);
        CHECK(SpellRange(0, false) == MaximumRange);
        CHECK(SpellRange(30, true) == TouchRange);
    }

    TEST_CASE("Living followers in bleedout remain healing targets at zero health") {
        auto downed = InjuredAlly(2, 0);
        downed.bleedingOut = true;
        CHECK(Eligible(downed, 1, MaximumRange));
        CHECK(HealthFraction(downed) == 0);
        downed.health = -10;
        CHECK(Eligible(downed, 1, MaximumRange));
        downed.alive = false;
        CHECK_FALSE(Eligible(downed, 1, MaximumRange));
    }

    TEST_CASE("Failed spell conditions do not change an ally's friendliness") {
        auto ally = InjuredAlly(2);
        ally.spellApplicable = false;
        CHECK(ally.friendly);
        CHECK_FALSE(Eligible(ally, 1, MaximumRange));
    }

    TEST_CASE("Urgency is based on health fraction rather than total hit points") {
        const auto fragile = InjuredAlly(2, 30);
        auto strong = InjuredAlly(3, 100);
        strong.maximumHealth = 1000;
        CHECK(MoreUrgent(strong, fragile));
        CHECK_FALSE(MoreUrgent(fragile, strong));
    }

    TEST_CASE("Healing positions must stay out of melee and within the selected spell's range") {
        const Point origin{0, 0, 0}, ally{600, 0, 0};
        const std::array threats{Point{600, 100, 0}};
        CHECK_FALSE(MovementScore(origin, ally, {600, 0, 0}, 640, threats));
        CHECK_FALSE(MovementScore(origin, ally, {-100, 0, 0}, 640, threats));
        CHECK(MovementScore(origin, ally, {100, 0, 0}, 640, threats));
        CHECK_FALSE(MovementScore(origin, ally, {100, 0, 0}, TouchRange, threats));
    }

    TEST_CASE("Retreat stays active until the healer reaches safe casting separation") {
        CHECK(Retreating(false, 200));
        CHECK(Retreating(true, 300));
        CHECK(Retreating(false, 400));
        CHECK(Retreating(true, SafeDistance));
        CHECK_FALSE(Retreating(true, RetreatCompleteDistance));
        CHECK_FALSE(SafeToStartCast(300));
        CHECK(SafeToStartCast(SafeDistance));
    }

    TEST_CASE("Charging has a short grace period but an enemy within melee reach interrupts immediately") {
        CHECK_FALSE(InterruptForThreat(200, 1));
        CHECK(InterruptForThreat(200, CastThreatGrace));
        CHECK(InterruptForThreat(100, 0.1f));
        CHECK_FALSE(InterruptForThreat(400, CastTimeout));
    }

    TEST_CASE("Retreat positions require more clearance than ordinary target seeking") {
        const Point origin{0, 0, 0}, ally{500, 0, 0}, candidate{0, 0, 0};
        const std::array threats{Point{0, 300, 0}};
        CHECK(MovementScore(origin, ally, candidate, 640, threats));
        CHECK_FALSE(MovementScore(origin, ally, candidate, 640, threats, SafeDistance));
    }

    TEST_CASE("An escape endpoint beyond an enemy is rejected when the first step moves closer") {
        const Point origin{0, 0, 0};
        const std::array threats{Point{200, 0, 0}};
        CHECK_FALSE(EscapeDirection(origin, {800, 0, 0}, threats));
        CHECK(EscapeDirection(origin, {-640, 0, 0}, threats));
        CHECK_FALSE(EscapeDirection(origin, origin, threats));
        CHECK(EscapeDirection(origin, {640, 0, 0}, {}));
    }

    TEST_CASE("Healers favor safer ground rather than the closest point beside an enemy") {
        const Point origin{0, 0, 0}, ally{500, 0, 0};
        const std::array threats{Point{0, 300, 0}};
        const auto nearby = MovementScore(origin, ally, {0, 0, 0}, 640, threats);
        const auto safer = MovementScore(origin, ally, {500, 0, 0}, 640, threats);
        REQUIRE(nearby);
        REQUIRE(safer);
        CHECK(*safer < *nearby);
        CHECK(*MovementScore(origin, ally, {0, 0, 0}, 640, {}) == 0);
    }
}
