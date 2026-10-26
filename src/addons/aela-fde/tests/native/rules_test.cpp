#include <doctest/doctest.h>
#include "Rules.h"
#include <initializer_list>

namespace {
    using namespace yliwf::policy;
    using mod::aela_rules::Restrictions;
    using mod::aela_rules::RestrictionsState;

    TEST_CASE("Aela uses ordinary core control after her story unlock") {
        CHECK(Restrictions({}) == 0);
        CHECK(Restrictions({.eligible = false}) == Bit(Action::Recruit));
    }

    TEST_CASE("Aela retains her slot and can be dismissed during a transformation") {
        for (auto field : {&RestrictionsState::transforming, &RestrictionsState::beastForm}) {
            RestrictionsState state;
            state.*field = true;
            CHECK(Restrictions(state) == (All & ~Bit(Action::Dismiss)));
            CHECK((Restrictions(state) & Bit(Action::Dismiss)) == 0);
            CHECK((Restrictions(state) & Bit(Action::Movement)) != 0);
            CHECK((Restrictions(state) & Bit(Action::Combat)) != 0);
        }
    }

    TEST_CASE("Authored scenes and lockouts take priority over beast form dismissal") {
        for (auto field : {&RestrictionsState::unavailable, &RestrictionsState::hostile, &RestrictionsState::inScene,
                           &RestrictionsState::questPackage}) {
            RestrictionsState state{.beastForm = true};
            state.*field = true;
            CHECK(Restrictions(state) == All);
        }
        CHECK(Restrictions({.compatible = false}) == All);
    }
}
