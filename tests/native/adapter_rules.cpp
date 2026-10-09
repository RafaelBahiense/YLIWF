#include "AdapterRules.h"
#include "../../src/addons/serana/native/Rules.h"
#include <cstdlib>
#include <iostream>
#include <cstring>

namespace {
    void Check(bool condition, const char* message) {
        if (!condition) {
            std::cerr << message << '\n';
            std::exit(1);
        }
    }
}

int main() {
    using namespace mod::adapter_rules;
    using namespace yliwf::sdk;
    static_assert(sizeof(void*) == 8 && sizeof(FollowerState) == 204 && BaseAdapterSize == 56 &&
                  sizeof(Adapter) == 64 && BaseAPISize == 24 && sizeof(API) == 32);
    API host;
    host.stateChanged = [](AdapterID) {};
    Check(CanNotifyState(&host), "Host notification extension unavailable");
    host.size = BaseAPISize;
    Check(!CanNotifyState(&host) && !CanNotifyState(nullptr), "Old or absent host exposes optional callback");
    Adapter adapter;
    Check(!Valid(adapter), "Incomplete adapter accepted");
    adapter.id = "test.controller";
    adapter.name = "Test controller";
    adapter.enumerate = [](void*, ActorID*, std::uint32_t) { return 0u; };
    adapter.inspect = [](void*, ActorID, FollowerState*) { return 1u; };
    adapter.start = [](void*, ActorID, Command, RequestID, char*, std::uint32_t) { return StartResult::Pending; };
    Check(Valid(adapter), "Valid version-one adapter rejected");
    adapter.size = BaseAdapterSize;
    Check(Valid(adapter), "Original v1 adapter rejected after extension");
    adapter.size = sizeof(Adapter);
    adapter.version = 2;
    Check(!Valid(adapter), "Unsupported ABI accepted");
    adapter.version = 1;
    adapter.size = 8;
    Check(!Valid(adapter), "Truncated adapter table accepted");

    auto following = mod::serana_rules::Describe({true, false, false, false, true, true});
    Check(Supports(following, Command::Follow) && Supports(following, Command::Wait) &&
              Supports(following, Command::Dismiss),
          "Normal Serana commands missing");
    auto locked = mod::serana_rules::Describe({true, false, false, true, true, true});
    Check(locked.state == State::Following && locked.commands == 0, "Quest-locked Serana can be changed");
    Check(std::strstr(locked.reason, "Follow, Wait and Dismiss"), "Quest lock does not explain disabled commands");
    auto restricted = mod::serana_rules::Describe({true, true, false, false, false, false});
    Check(restricted.state == State::Waiting && Supports(restricted, Command::Follow) &&
              !Supports(restricted, Command::Wait) && !Supports(restricted, Command::Dismiss),
          "Quest capabilities ignored");
    Check(std::strstr(restricted.reason, "Wait is unavailable") &&
              std::strstr(restricted.reason, "Dismiss is unavailable") &&
              !std::strstr(restricted.reason, "Follow is unavailable"),
          "Restrictions do not describe the actual disabled commands");
    const auto cannotFollow = mod::serana_rules::Describe({true, true, false, false, true, true, false});
    Check(!Supports(cannotFollow, Command::Follow) && Supports(cannotFollow, Command::Wait),
          "CanFollow restriction ignored");
    Check(std::strstr(cannotFollow.reason, "Follow is unavailable"), "Follow restriction has no explanation");
    const auto allRestricted = mod::serana_rules::Describe({true, false, false, false, false, false, false});
    Check(std::strstr(allRestricted.reason, "Follow is unavailable") &&
              std::strstr(allRestricted.reason, "Wait is unavailable") &&
              std::strstr(allRestricted.reason, "Dismiss is unavailable") &&
              std::strlen(allRestricted.reason) < sizeof(allRestricted.reason),
          "Combined restrictions overflow or omit a command");
    auto dismissed = mod::serana_rules::Describe({false, false, true, false, true, true});
    Check(dismissed.state == State::Inactive && !Supports(dismissed, Command::Follow),
          "Adapter can recruit inactive Serana");
    Check(Reached(dismissed, Command::Dismiss) && !Reached(following, Command::Wait),
          "Wrong controller state acknowledged");
    using mod::serana_rules::DistanceFlags;
    const mod::serana_rules::ControllerState freeFollowing{true, false, false, false, true, true};
    Check(DistanceFlags(freeFollowing, FollowDistance::Close) == std::array{false, false, true},
          "Close does not select exactly one distance flag");
    Check(DistanceFlags(freeFollowing, FollowDistance::Normal) == std::array{false, true, false},
          "Normal does not select medium distance");
    Check(DistanceFlags(freeFollowing, FollowDistance::Far) == std::array{true, false, false},
          "Far does not select far distance");
    Check(!DistanceFlags({true, false, false, true, true, true}, FollowDistance::Close),
          "Distance bypasses quest lock");
    Check(!DistanceFlags({false, false, true, false, true, true}, FollowDistance::Close),
          "Distance changes a dismissed follower");
    Check(!DistanceFlags({true, false, false, false, true, true, false}, FollowDistance::Close),
          "Distance bypasses CanFollow");
    Check(DistanceFlags({true, true, false, false, true, true}, FollowDistance::Far) == std::array{true, false, false},
          "Waiting follower cannot choose spacing without resuming");
    Check(!DistanceFlags(freeFollowing, static_cast<FollowDistance>(3)), "Invalid distance accepted");
    following.commands |= 128;
    Check(!Supports(following, Command::Follow), "Invalid capability bits accepted");

    Requests requests;
    const auto first = requests.Add(1, 100, Command::Wait);
    Check(first.has_value() && !requests.Add(2, 100, Command::Dismiss), "Duplicate controller command accepted");
    Check(!requests.Find(2, *first), "Another adapter can acknowledge a request");
    auto* pending = requests.Find(1, *first);
    pending->started = true;
    pending->quarantined = true;
    Check(!requests.Add(1, 100, Command::Wait), "Timed-out call can be replayed");
    Check(requests.Remove(1, *first) && requests.Add(1, 100, Command::Follow).has_value(),
          "Late acknowledgement cannot release quarantine");
    requests.Reset();
    const auto afterLoad = requests.Add(1, 100, Command::Dismiss);
    Check(afterLoad && *afterLoad > *first && !requests.Find(1, *first),
          "Old-save acknowledgement can finish a new request");
    for (ActorID actor = 101; actor < 116; ++actor)
        Check(requests.Add(1, actor, Command::Wait).has_value(), "Queue filled prematurely");
    Check(!requests.Add(1, 116, Command::Wait), "Adapter queue limit ignored");
    std::cout << "Adapter ABI, capabilities, ownership, quarantine, and load-generation tests passed.\n";
}
