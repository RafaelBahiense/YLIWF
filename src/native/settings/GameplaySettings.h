#pragma once

#include "common/StateIO.h"
#include <algorithm>
#include <optional>

namespace mod::settings {
    // Gameplay defaults use INI names at the file boundary; saves contain values,
    // never load-order-dependent resolved perk pointers or form IDs.
    struct GameplaySettings {
        std::int32_t extraFollowers = 7;
        std::int32_t limitMode = 0;
        std::int32_t speechLevels = 10;
        std::int32_t distance = 1;
        std::int32_t formation = 0;
        bool essential = false, friendlyFire = false, sandbox = false, crossfire = false, homes = false;
        std::string perks;

        bool operator==(const GameplaySettings&) const = default;
    };

    inline constexpr std::uint32_t GameplayRecord = 0x53455454;  // SETT
    inline constexpr std::uint32_t GameplayVersion = 1;
    inline constexpr std::size_t MaxGameplayBytes = 4096;

    inline bool ValidGameplay(const GameplaySettings& value) {
        return value.extraFollowers >= 0 && value.extraFollowers <= 7 && value.limitMode >= 0 && value.limitMode <= 2 &&
               value.speechLevels >= 1 && value.distance >= 0 && value.distance <= 2 && value.formation >= 0 &&
               value.formation <= 3 && value.perks.size() < 2048 &&
               std::ranges::none_of(value.perks, [](unsigned char c) { return c < 32; });
    }

    inline std::vector<std::uint8_t> EncodeGameplay(const GameplaySettings& value) {
        if (!ValidGameplay(value))
            return {};
        controller::storage::StateWriter writer;
        for (auto number : {value.extraFollowers, value.limitMode, value.speechLevels, value.distance, value.formation})
            writer.Number(number);
        for (auto flag : {value.essential, value.friendlyFire, value.sandbox, value.crossfire, value.homes})
            writer.Number<std::uint32_t>(flag);
        writer.Text(value.perks);
        return std::move(writer.bytes);
    }

    inline std::optional<GameplaySettings> DecodeGameplay(std::span<const std::uint8_t> bytes) {
        if (bytes.size() > MaxGameplayBytes)
            return {};
        controller::storage::StateReader reader(bytes);
        GameplaySettings value;
        for (auto* number :
             {&value.extraFollowers, &value.limitMode, &value.speechLevels, &value.distance, &value.formation})
            if (!reader.Number(*number))
                return {};
        for (auto* flag : {&value.essential, &value.friendlyFire, &value.sandbox, &value.crossfire, &value.homes})
            if (!reader.Boolean(*flag))
                return {};
        if (!reader.Text(value.perks, 2047) || !reader.Done() || !ValidGameplay(value))
            return {};
        return value;
    }
}
