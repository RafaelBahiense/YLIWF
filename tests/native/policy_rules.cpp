#include <doctest/doctest.h>
#include "followers/PolicyRules.h"
#include <algorithm>
#include <array>

using namespace yliwf::policy;
using mod::policy_rules::Aggregate;

namespace {
    State Request(PackagePreference mode, std::uint32_t id, std::uint32_t priority) {
        State value;
        value.blocked = 0;
        value.packagePreference = mode;
        value.package = id;
        value.packagePriority = priority;
        return value;
    }
}

TEST_CASE("Supplementary restrictions combine without claiming quest-release ownership") {
    State owner;
    owner.blocked = Bit(Action::Dismiss);
    owner.ownerQuest = 0x100;
    auto shared = Request(PackagePreference::Follower, 0, 0);
    shared.blocked = Bit(Action::Home);
    for (bool reverse : {false, true}) {
        Aggregate combined;
        combined.Add(reverse ? owner : shared);
        combined.Add(reverse ? shared : owner);
        auto result = combined.Result();
        CHECK(result.blocked == (Bit(Action::Home) | Bit(Action::Dismiss)));
        CHECK(HasPackageRequest(result));
        CHECK(CanRelease(result, 0x100));
        CHECK_FALSE(CanRelease(result, 0x101));
    }
    Aggregate supplementary;
    supplementary.Add(shared);
    CHECK_FALSE(CanRelease(supplementary.Result()));
    shared.blocked |= Bit(Action::Movement);
    supplementary.Add(shared);
    CHECK_FALSE(HasPackageRequest(supplementary.Result()));
}

TEST_CASE("Highest package priority wins independently of provider registration order") {
    const std::array values{
        Request(PackagePreference::Follower, 0, 0), Request(PackagePreference::Requested, 0x200, 10),
        Request(PackagePreference::Requested, 0x300, 10), Request(PackagePreference::Requested, 0x400, 20)};
    std::array order{0, 1, 2, 3};
    do {
        Aggregate combined;
        for (auto index : order)
            combined.Add(values[index]);
        const auto result = combined.Result();
        CHECK(result.package == 0x400);
        CHECK(result.packagePriority == 20);
        CHECK(HasPackageRequest(result));
    } while (std::next_permutation(order.begin(), order.end()));
}

TEST_CASE("Tied incompatible packages withdraw only package control and preserve restrictions and owner") {
    auto first = Request(PackagePreference::Requested, 0x200, 10);
    first.ownerQuest = 0x100;
    first.blocked = Bit(Action::Dismiss);
    const auto second = Request(PackagePreference::Requested, 0x300, 10);
    for (bool reverse : {false, true}) {
        Aggregate combined;
        combined.Add(reverse ? first : second);
        combined.Add(reverse ? second : first);
        const auto result = combined.Result();
        CHECK_FALSE(HasPackageRequest(result));
        CHECK(result.blocked == Bit(Action::Dismiss));
        CHECK(CanRelease(result, 0x100));
        CHECK(result.reason[0] != 0);
    }
    Aggregate identical;
    identical.Add(first);
    identical.Add(first);
    CHECK(HasPackageRequest(identical.Result()));
}

TEST_CASE("Invalid results and conflicting owners cannot be repaired by later providers") {
    State owner;
    owner.blocked = 0;
    owner.ownerQuest = 0x100;
    auto other = owner;
    other.ownerQuest = 0x101;
    Aggregate conflict;
    conflict.Add(owner);
    conflict.Add(other);
    conflict.Add(owner);
    CHECK(conflict.Result().blocked == All);
    CHECK_FALSE(CanRelease(conflict.Result()));
    const auto ready = Request(PackagePreference::Follower, 0, 0);
    for (int variant = 0; variant < 5; ++variant) {
        auto invalid = ready;
        if (variant == 0)
            invalid.size = 0;
        if (variant == 1)
            invalid.blocked = All + 1;
        if (variant == 2)
            invalid.packagePreference = static_cast<PackagePreference>(99);
        if (variant == 3)
            invalid.package = 0x200;
        if (variant == 4)
            invalid.packagePreference = PackagePreference::Requested;
        for (bool reverse : {false, true}) {
            Aggregate combined;
            combined.Add(reverse ? invalid : owner);
            combined.Add(reverse ? owner : invalid);
            CHECK(combined.Result().blocked == All);
            CHECK_FALSE(CanRelease(combined.Result()));
            CHECK_FALSE(HasPackageRequest(combined.Result()));
        }
    }
}

TEST_CASE("Requests are rebuilt from live providers after withdrawal or load") {
    Aggregate active;
    active.Add(Request(PackagePreference::Requested, 0x200, 10));
    CHECK(HasPackageRequest(active.Result()));
    Aggregate refreshed;
    CHECK_FALSE(HasPackageRequest(refreshed.Result()));
    refreshed.Add(Request(PackagePreference::Follower, 0, 0));
    CHECK(refreshed.Result().packagePreference == PackagePreference::Follower);
}
