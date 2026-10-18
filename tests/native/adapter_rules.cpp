#include <doctest/doctest.h>

#include "adapters/AdapterRules.h"

namespace {
    TEST_CASE("Adapter observations do not require a visible menu") {
        float seconds = 0;
        REQUIRE_FALSE(mod::adapter_rules::ObservationDue(seconds, 0));
        REQUIRE_FALSE(mod::adapter_rules::ObservationDue(seconds, -1));
        REQUIRE_FALSE(mod::adapter_rules::ObservationDue(seconds, 0.5f));
        REQUIRE(mod::adapter_rules::ObservationDue(seconds, 0.5f));
        REQUIRE(seconds == 0);
        REQUIRE(mod::adapter_rules::ObservationDue(seconds, 5));
        REQUIRE_FALSE(mod::adapter_rules::ObservationDue(seconds, 0));
    }

    using namespace yliwf::sdk;
    using mod::adapter_rules::Requests;
    using mod::adapter_rules::Valid;

    static_assert(sizeof(void*) == 8 && sizeof(FollowerState) == 204 && BaseAdapterSize == 56 &&
                  offsetof(Adapter, canRecruitThroughDialogue) == 64 &&
                  offsetof(Adapter, canReceiveCombatProtection) == 72 &&
                  offsetof(Adapter, canTravelInFormation) == 80 && sizeof(Adapter) == 104 && BaseAPISize == 24 &&
                  sizeof(API) == 32);

    Adapter ValidAdapter() {
        Adapter adapter;
        adapter.id = "test.controller";
        adapter.name = "Test controller";
        adapter.enumerate = [](void*, ActorID*, std::uint32_t) { return 0u; };
        adapter.inspect = [](void*, ActorID, FollowerState*) { return 1u; };
        adapter.start = [](void*, ActorID, Command, RequestID, char*, std::uint32_t) { return StartResult::Pending; };
        adapter.setFollowDistance = [](void*, ActorID, FollowDistance) { return 1u; };
        adapter.canRecruitThroughDialogue = [](void*, ActorID) { return 1u; };
        adapter.canReceiveCombatProtection = [](void*, ActorID) { return 1u; };
        adapter.canTravelInFormation = [](void*, ActorID) { return 1u; };
        adapter.setFormationPackage = [](void*, ActorID, std::uint32_t) { return 1u; };
        return adapter;
    }

    TEST_CASE("Stand-down is optional and never reads beyond an older adapter table") {
        auto adapter = ValidAdapter();
        adapter.setStandDownPackage = [](void*, ActorID, std::uint32_t) { return 1u; };
        CHECK(HasStandDown(adapter));
        adapter.size = 96;
        CHECK_FALSE(HasStandDown(adapter));
        adapter.size = sizeof(Adapter);
        adapter.setStandDownPackage = nullptr;
        CHECK_FALSE(HasStandDown(adapter));
    }

    TEST_CASE("Host Notification Compatibility") {
        API host;
        host.stateChanged = [](AdapterID) {};
        REQUIRE_MESSAGE((CanNotifyState(&host)), "Host notification extension unavailable");

        host.size = BaseAPISize;
        REQUIRE_MESSAGE((!CanNotifyState(&host) && !CanNotifyState(nullptr)),
                        "Old or absent host exposes optional callback");
    }

    TEST_CASE("Adapter Table Compatibility") {
        REQUIRE_MESSAGE((!Valid(Adapter{})), "Incomplete adapter accepted");

        auto adapter = ValidAdapter();
        REQUIRE_MESSAGE((HasTravelFormation(adapter)), "Complete travel capability unavailable");
        adapter.size = offsetof(Adapter, setFormationPackage);
        REQUIRE_MESSAGE((!HasTravelFormation(adapter)), "Partial travel extension accepted");
        adapter.size = offsetof(Adapter, canTravelInFormation);
        REQUIRE_MESSAGE((!HasTravelFormation(adapter) && HasCombatProtection(adapter)),
                        "Protection-only table exposes travel callbacks");
        REQUIRE_MESSAGE((Valid(adapter)), "Protection-only v1 table rejected after travel extension");
        adapter.size = sizeof(Adapter);
        adapter.setFormationPackage = nullptr;
        REQUIRE_MESSAGE((!HasTravelFormation(adapter)), "Permission without movement callback accepted");
        adapter = ValidAdapter();
        REQUIRE_MESSAGE(
            (HasFollowDistance(adapter) && HasRecruitmentEligibility(adapter) && HasCombatProtection(adapter)),
            "Optional callbacks unavailable");

        adapter.size = offsetof(Adapter, canReceiveCombatProtection);
        REQUIRE_MESSAGE(
            (HasFollowDistance(adapter) && HasRecruitmentEligibility(adapter) && !HasCombatProtection(adapter)),
            "Recruitment v1 table exposes a missing protection tail");
        REQUIRE_MESSAGE((Valid(adapter)), "Recruitment v1 table rejected after extension");

        adapter.size = offsetof(Adapter, canRecruitThroughDialogue);
        REQUIRE_MESSAGE(
            (HasFollowDistance(adapter) && !HasRecruitmentEligibility(adapter) && !HasCombatProtection(adapter)),
            "Distance-only v1 table lost its callback or exposes a missing recruitment tail");
        REQUIRE_MESSAGE((Valid(adapter)), "Valid version-one adapter rejected");

        adapter.size = BaseAdapterSize;
        REQUIRE_MESSAGE(
            (!HasFollowDistance(adapter) && !HasRecruitmentEligibility(adapter) && !HasCombatProtection(adapter)),
            "Original v1 table exposes optional callbacks");
        REQUIRE_MESSAGE((Valid(adapter)), "Original v1 adapter rejected after extension");

        adapter.size = sizeof(Adapter);
        adapter.version = 2;
        REQUIRE_MESSAGE((!Valid(adapter)), "Unsupported ABI accepted");

        adapter.version = 1;
        adapter.size = 8;
        REQUIRE_MESSAGE((!Valid(adapter)), "Truncated adapter table accepted");
    }

    TEST_CASE("Invalid Capability Bits") {
        FollowerState following;
        following.state = State::Following;
        following.commands = AllCommands;
        following.commands |= 128;
        REQUIRE_MESSAGE((!Supports(following, Command::Follow)), "Invalid capability bits accepted");
    }

    TEST_CASE("Combat Protection Ownership") {
        using mod::adapter_rules::CombatProtection;
        REQUIRE_MESSAGE((CombatProtection(true, State::Following, true, false, false) &&
                         CombatProtection(true, State::Waiting, true, false, false)),
                        "Active eligible followers do not receive protection");
        REQUIRE_MESSAGE((!CombatProtection(false, State::Following, true, false, false)),
                        "Disabled protection remains active");
        REQUIRE_MESSAGE((!CombatProtection(true, State::Inactive, true, false, false) &&
                         !CombatProtection(true, State::Unavailable, true, false, false)),
                        "Dismissed or conflicting followers retain protection");
        REQUIRE_MESSAGE((!CombatProtection(true, State::Following, false, false, false)), "Provider opt-out ignored");
        REQUIRE_MESSAGE((!CombatProtection(true, State::Following, true, true, false)),
                        "Dead actor receives protection");
        REQUIRE_MESSAGE((!CombatProtection(true, State::Following, true, false, true)),
                        "Adapter changes native-owned protection");
    }

    TEST_CASE("Request Ownership And Quarantine") {
        Requests requests;
        constexpr AdapterID owner = 1;
        constexpr AdapterID otherAdapter = 2;
        constexpr ActorID actor = 100;

        const auto request = requests.Add(owner, actor, Command::Wait);
        REQUIRE_MESSAGE((request.has_value() && !requests.Add(otherAdapter, actor, Command::Dismiss)),
                        "Duplicate controller command accepted");
        REQUIRE_MESSAGE((!requests.Find(otherAdapter, *request)), "Another adapter can acknowledge a request");

        auto* pending = requests.Find(owner, *request);
        pending->started = true;
        pending->quarantined = true;
        REQUIRE_MESSAGE((!requests.Add(owner, actor, Command::Wait)), "Timed-out call can be replayed");
        REQUIRE_MESSAGE((requests.Remove(owner, *request) && requests.Add(owner, actor, Command::Follow).has_value()),
                        "Late acknowledgement cannot release quarantine");

        requests.Reset();
        const auto afterLoad = requests.Add(owner, actor, Command::Dismiss);
        REQUIRE_MESSAGE((afterLoad && *afterLoad > *request && !requests.Find(owner, *request)),
                        "Old-save acknowledgement can finish a new request");

        for (ActorID nextActor = actor + 1; nextActor < actor + 16; ++nextActor)
            REQUIRE_MESSAGE((requests.Add(owner, nextActor, Command::Wait).has_value()), "Queue filled prematurely");
        REQUIRE_MESSAGE((!requests.Add(owner, actor + 16, Command::Wait)), "Adapter queue limit ignored");
    }
}
