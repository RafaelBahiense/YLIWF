#include "ObserverPosition.h"
#include "RE/Skyrim.h"

namespace mod::combat_tests::observer_position {
    namespace {
        RE::NiPoint3 origin{}, assigned{};
        std::uint32_t cell = 0;
        bool active = false, returning = false;

        bool SameCell(const RE::Actor* player) {
            return player && player->GetParentCell() && player->GetParentCell()->GetFormID() == cell;
        }
    }

    bool CameraFree() {
        const auto* camera = RE::PlayerCamera::GetSingleton();
        return camera && camera->IsInFreeCameraMode();
    }

    bool Positioned() {
        const auto* player = RE::PlayerCharacter::GetSingleton();
        return active && !returning && SameCell(player) && player->GetPosition().GetDistance(assigned) <= 96;
    }

    bool Pending() {
        const auto* player = RE::PlayerCharacter::GetSingleton();
        if (returning && (!SameCell(player) || player->GetPosition().GetDistance(origin) <= 32))
            Reset();
        return returning;
    }

    bool Begin() {
        if (Pending())
            return false;
        if (active) {
            if (Positioned())
                return true;
            // A verified manual move ends the previous acquisition. A new run
            // captures the current location instead of retaining a stale origin.
            Reset();
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* data = RE::TESDataHandler::GetSingleton();
        const auto* marker =
            data ? data->LookupForm<RE::TESObjectREFR>(0x810, "YouLeadIWillFollow.CombatArena.esp") : nullptr;
        if (!player || !marker || !player->Is3DLoaded() || player->IsOnMount() || player->GetCurrentScene() ||
            player->GetParentCell() != marker->GetParentCell())
            return false;
        origin = player->GetPosition();
        assigned = marker->GetPosition();
        cell = marker->GetParentCell()->GetFormID();
        active = true;
        player->SetPosition(assigned, true);
        return Positioned();
    }

    void End() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!active)
            return;
        // Leaving the cell or walking away supersedes our position override.
        if (!returning && (!SameCell(player) || player->GetCurrentScene() || player->IsOnMount() ||
                           player->GetPosition().GetDistance(assigned) > 128)) {
            Reset();
            return;
        }
        if (!returning) {
            returning = true;
            player->SetPosition(origin, true);
        }
        Pending();
    }

    void Reset() {
        origin = assigned = {};
        cell = 0;
        active = returning = false;
    }
}
