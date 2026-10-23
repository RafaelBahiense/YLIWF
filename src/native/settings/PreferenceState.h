#pragma once

#include "settings/Settings.h"
#include "diagnostics/Debug.h"
#include <optional>
#include <tuple>

namespace mod::settings::preference_state {
    inline auto Current() {
        return std::tuple{MaxExtraFollowers,
                          FollowerPerkOption,
                          SpeechLevelsPerSlot,
                          FollowDistance,
                          Formation,
                          std::string(PerkListBuffer),
                          Hotkeys,
                          std::array{FollowerEssential, FriendlyFire, FollowerCrossfire, FollowerSandbox, FollowerHomes,
                                     debug::OptionsEnabled.load(), debug::Logging.load()}};
    }

    inline std::optional<decltype(Current())> Saved;

    inline void RememberSaved() {
        std::scoped_lock lock(Mutex);
        Saved = Current();
    }

    inline bool HasChanges() {
        std::scoped_lock lock(Mutex);
        return Saved && *Saved != Current();
    }
}
