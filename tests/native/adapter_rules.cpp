#include "AdapterRules.h"

#include <cstdlib>
#include <iostream>

namespace {
    using namespace yliwf::sdk;
    using mod::adapter_rules::Requests;
    using mod::adapter_rules::Valid;

    static_assert(sizeof(void*) == 8 && sizeof(FollowerState) == 204 && BaseAdapterSize == 56 &&
                  offsetof(Adapter, canRecruitThroughDialogue) == 64 &&
                  offsetof(Adapter, canReceiveCombatProtection) == 72 && sizeof(Adapter) == 80 && BaseAPISize == 24 &&
                  sizeof(API) == 32);

    void Check(bool condition, const char* message) {
        if (!condition) {
            std::cerr << message << '\n';
            std::exit(1);
        }
    }

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
        return adapter;
    }

    void CheckHostNotificationCompatibility() {
        API host;
        host.stateChanged = [](AdapterID) {};
        Check(CanNotifyState(&host), "Host notification extension unavailable");

        host.size = BaseAPISize;
        Check(!CanNotifyState(&host) && !CanNotifyState(nullptr), "Old or absent host exposes optional callback");
    }

    void CheckAdapterTableCompatibility() {
        Check(!Valid(Adapter{}), "Incomplete adapter accepted");

        auto adapter = ValidAdapter();
        Check(HasFollowDistance(adapter) && HasRecruitmentEligibility(adapter) && HasCombatProtection(adapter),
              "Optional callbacks unavailable");

        adapter.size = offsetof(Adapter, canReceiveCombatProtection);
        Check(HasFollowDistance(adapter) && HasRecruitmentEligibility(adapter) && !HasCombatProtection(adapter),
              "Recruitment v1 table exposes a missing protection tail");
        Check(Valid(adapter), "Recruitment v1 table rejected after extension");

        adapter.size = offsetof(Adapter, canRecruitThroughDialogue);
        Check(HasFollowDistance(adapter) && !HasRecruitmentEligibility(adapter) && !HasCombatProtection(adapter),
              "Distance-only v1 table lost its callback or exposes a missing recruitment tail");
        Check(Valid(adapter), "Valid version-one adapter rejected");

        adapter.size = BaseAdapterSize;
        Check(!HasFollowDistance(adapter) && !HasRecruitmentEligibility(adapter) && !HasCombatProtection(adapter),
              "Original v1 table exposes optional callbacks");
        Check(Valid(adapter), "Original v1 adapter rejected after extension");

        adapter.size = sizeof(Adapter);
        adapter.version = 2;
        Check(!Valid(adapter), "Unsupported ABI accepted");

        adapter.version = 1;
        adapter.size = 8;
        Check(!Valid(adapter), "Truncated adapter table accepted");
    }

    void CheckInvalidCapabilityBits() {
        FollowerState following;
        following.state = State::Following;
        following.commands = AllCommands;
        following.commands |= 128;
        Check(!Supports(following, Command::Follow), "Invalid capability bits accepted");
    }

    void CheckCombatProtectionOwnership() {
        using mod::adapter_rules::CombatProtection;
        Check(CombatProtection(true, State::Following, true, false, false) &&
                  CombatProtection(true, State::Waiting, true, false, false),
              "Active eligible followers do not receive protection");
        Check(!CombatProtection(false, State::Following, true, false, false), "Disabled protection remains active");
        Check(!CombatProtection(true, State::Inactive, true, false, false) &&
                  !CombatProtection(true, State::Unavailable, true, false, false),
              "Dismissed or conflicting followers retain protection");
        Check(!CombatProtection(true, State::Following, false, false, false), "Provider opt-out ignored");
        Check(!CombatProtection(true, State::Following, true, true, false), "Dead actor receives protection");
        Check(!CombatProtection(true, State::Following, true, false, true), "Adapter changes native-owned protection");
    }

    void CheckRequestOwnershipAndQuarantine() {
        Requests requests;
        constexpr AdapterID owner = 1;
        constexpr AdapterID otherAdapter = 2;
        constexpr ActorID actor = 100;

        const auto request = requests.Add(owner, actor, Command::Wait);
        Check(request.has_value() && !requests.Add(otherAdapter, actor, Command::Dismiss),
              "Duplicate controller command accepted");
        Check(!requests.Find(otherAdapter, *request), "Another adapter can acknowledge a request");

        auto* pending = requests.Find(owner, *request);
        pending->started = true;
        pending->quarantined = true;
        Check(!requests.Add(owner, actor, Command::Wait), "Timed-out call can be replayed");
        Check(requests.Remove(owner, *request) && requests.Add(owner, actor, Command::Follow).has_value(),
              "Late acknowledgement cannot release quarantine");

        requests.Reset();
        const auto afterLoad = requests.Add(owner, actor, Command::Dismiss);
        Check(afterLoad && *afterLoad > *request && !requests.Find(owner, *request),
              "Old-save acknowledgement can finish a new request");

        for (ActorID nextActor = actor + 1; nextActor < actor + 16; ++nextActor)
            Check(requests.Add(owner, nextActor, Command::Wait).has_value(), "Queue filled prematurely");
        Check(!requests.Add(owner, actor + 16, Command::Wait), "Adapter queue limit ignored");
    }
}

int main() {
    CheckHostNotificationCompatibility();
    CheckAdapterTableCompatibility();
    CheckInvalidCapabilityBits();
    CheckCombatProtectionOwnership();
    CheckRequestOwnershipAndQuarantine();

    std::cout << "Adapter ABI, capabilities, ownership, quarantine, and load-generation tests passed.\n";
}
