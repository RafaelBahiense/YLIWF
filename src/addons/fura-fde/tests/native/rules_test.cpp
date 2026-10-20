#include <doctest/doctest.h>
#include "Rules.h"
#include <initializer_list>

namespace {
    using namespace yliwf::policy;
    using mod::fura_rules::Restrictions;
    using mod::fura_rules::RestrictionsState;

    TEST_CASE("Normal FDE recruitment enables all core features after the introduction") {
        CHECK(Restrictions({.introduced = true}) == 0);
        CHECK(Restrictions({}) == Bit(Action::Recruit));
    }

    TEST_CASE("FDE quest movement and scenes suspend core commands and combat control") {
        for (auto field :
             {&RestrictionsState::unavailable, &RestrictionsState::questLocked, &RestrictionsState::atSawmill,
              &RestrictionsState::hostile, &RestrictionsState::inScene, &RestrictionsState::specialPackage}) {
            RestrictionsState state{.introduced = true};
            state.*field = true;
            CHECK(Restrictions(state) == All);
            for (auto action : {Action::Recruit, Action::Follow, Action::Wait, Action::Dismiss, Action::Movement,
                                Action::Combat, Action::Home})
                CHECK((Restrictions(state) & Bit(action)) != 0);
        }
    }

    TEST_CASE("Quest policy validation rejects missing owners and unsupported action flags") {
        State state;
        CHECK_FALSE(Valid(state));
        state.ownerQuest = 0x123;
        CHECK(Valid(state));
        state.blocked = All + 1;
        CHECK_FALSE(Valid(state));
        state.blocked = 0;
        state.size = 0;
        CHECK_FALSE(Valid(state));
    }

    TEST_CASE("Only the registered quest owner can release an actor during quest locks") {
        State state;
        state.ownerQuest = 0x123;
        state.blocked = All;
        CHECK(CanRelease(state, 0x123));
        CHECK_FALSE(CanRelease(state, 0x124));
        state.ownerQuest = 0;
        CHECK_FALSE(CanRelease(state));
    }
}
