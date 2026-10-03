#pragma once

#include "Strings.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace mod::party_rules {
    inline bool ValidBinding(bool ownerMatches, std::uint32_t aliasID, std::string_view name, bool primary) {
        if (!ownerMatches) return false;
        if (primary) return aliasID == 0 && mod::strings::EqualsIgnoreCase(name, "Follower");
        constexpr std::array<std::string_view, 7> names{"ExtraFollower01", "ExtraFollower02", "ExtraFollower03",
            "ExtraFollower04", "ExtraFollower05", "ExtraFollower06", "ExtraFollower07"};
        return aliasID >= 2 && aliasID <= 8 && mod::strings::EqualsIgnoreCase(name, names[aliasID - 2]);
    }

    struct Slot {
        bool valid = false, occupied = false, dead = false;
        std::uint32_t actorID = 0;
    };

    inline std::int32_t FindActor(std::span<const Slot> slots, std::uint32_t actorID) {
        if (!actorID) return -1;
        for (std::size_t i = 0; i < slots.size(); ++i)
            if (slots[i].valid && slots[i].actorID == actorID) return static_cast<std::int32_t>(i);
        return -1;
    }

    inline std::int32_t FindFree(std::span<const Slot> slots) {
        for (std::size_t i = 0; i < slots.size(); ++i)
            if (slots[i].valid && !slots[i].occupied) return static_cast<std::int32_t>(i);
        return -1;
    }

    inline std::int32_t FindFirstLiving(std::span<const Slot> slots) {
        for (std::size_t i = 0; i < slots.size(); ++i)
            if (slots[i].valid && slots[i].actorID && !slots[i].dead) return static_cast<std::int32_t>(i);
        return -1;
    }

    inline std::int32_t CountLive(std::span<const Slot> slots) {
        std::int32_t count = 0;
        for (std::size_t i = 0; i < slots.size(); ++i) {
            const auto& slot = slots[i];
            if (!slot.valid || !slot.actorID || slot.dead) continue;
            // At most eight party slots: avoid a heap-allocated hash set.
            bool seen = false;
            for (std::size_t j = 0; j < i; ++j)
                if (slots[j].valid && !slots[j].dead && slots[j].actorID == slot.actorID) { seen = true; break; }
            if (!seen) ++count;
        }
        return count;
    }

    struct Globals { float vanillaCount, partyCount, recruitGate; };
    inline Globals CountGlobals(std::int32_t count, std::int32_t cap) {
        count = std::max(count, 0);
        cap = std::max(cap, 1);
        return {count > 0 ? 1.0f : 0.0f, static_cast<float>(count), count < cap ? 1.0f : 0.0f};
    }
}
