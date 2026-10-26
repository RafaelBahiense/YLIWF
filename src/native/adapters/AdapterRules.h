#pragma once

#include "FollowerAdapter.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <vector>

namespace mod::adapter_rules {
    using namespace yliwf::sdk;
    inline constexpr std::size_t Capacity = 16;
    inline constexpr float AcknowledgementSeconds = 30;

    inline bool ValidAction(const FollowerAction& action) {
        return action.size >= sizeof(FollowerAction) && action.id && action.enabled <= 1 &&
               static_cast<std::uint32_t>(action.kind) <= 1 && action.value <= 1 &&
               (action.kind == ActionKind::Toggle || action.value == 0) && action.label[0] &&
               std::ranges::find(action.label, '\0') != std::end(action.label) &&
               std::ranges::find(action.description, '\0') != std::end(action.description) &&
               std::ranges::find(action.reason, '\0') != std::end(action.reason);
    }

    inline bool ValidActionValue(const FollowerAction& action, std::uint32_t value) {
        return ValidAction(action) && action.enabled && (action.kind == ActionKind::Toggle ? value <= 1 : value == 0);
    }

    inline bool ObservationDue(float& seconds, float elapsed) {
        if (std::isfinite(elapsed) && elapsed > 0)
            seconds += elapsed;
        if (seconds < 1)
            return false;
        seconds = 0;
        return true;
    }

    inline bool Valid(const Adapter& adapter) {
        const bool hasMovement =
            adapter.size >= offsetof(Adapter, coreMovementFeatures) + sizeof(adapter.coreMovementFeatures);
        const bool validMovement = !hasMovement || (!(adapter.coreMovementFeatures & ~AllMovement) &&
                                                    (!adapter.coreMovementFeatures || adapter.movementPermissions));
        const bool validActions = adapter.size < offsetof(Adapter, invokeAction) + sizeof(adapter.invokeAction) ||
                                  (static_cast<bool>(adapter.actions) == static_cast<bool>(adapter.invokeAction));
        return validActions && validMovement && adapter.size >= BaseAdapterSize &&
               adapter.version == InterfaceVersion && adapter.id && *adapter.id && adapter.name && *adapter.name &&
               adapter.enumerate && adapter.inspect && adapter.start;
    }

    inline bool CombatProtection(bool enabled, State state, bool eligible, bool dead, bool nativeOwned) {
        return enabled && eligible && !dead && !nativeOwned && (state == State::Following || state == State::Waiting);
    }

    // A timed-out call remains quarantined until its late acknowledgement or a
    // save load. It must not be issued again while its effects are uncertain.
    struct Ticket {
        RequestID id = 0;
        AdapterID adapter = 0;
        ActorID actor = 0;
        Command command = Command::Follow;
        bool started = false, quarantined = false;
        float elapsed = 0;
    };

    class Requests {
    public:
        std::optional<RequestID> Add(AdapterID adapter, ActorID actor, Command command) {
            if (!adapter || !actor || items.size() >= Capacity || sequence == UINT64_MAX ||
                std::ranges::any_of(items, [actor](const auto& item) { return item.actor == actor; }))
                return {};
            items.push_back({++sequence, adapter, actor, command});
            return sequence;
        }

        Ticket* Find(AdapterID adapter, RequestID id) {
            for (auto& item : items)
                if (item.adapter == adapter && item.id == id)
                    return &item;
            return nullptr;
        }

        bool Remove(AdapterID adapter, RequestID id) {
            return std::erase_if(items, [=](const auto& item) { return item.adapter == adapter && item.id == id; }) !=
                   0;
        }

        void Reset() {
            items.clear();
        }  // Sequence deliberately survives loads.

        std::vector<Ticket> items;

    private:
        RequestID sequence = 0;
    };
}
