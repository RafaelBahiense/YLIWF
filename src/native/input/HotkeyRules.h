#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace mod::hotkey_rules {
    enum class Action : std::size_t { FollowAll, WaitAll, StandDown, Distance, Formation, Count };

    struct Definition {
        std::string_view name;
        const char* iniKey;
    };

    inline constexpr std::array Definitions{Definition{"Follow All", "iFollowAll"}, Definition{"Wait All", "iWaitAll"},
                                            Definition{"Toggle Stand down", "iStandDown"},
                                            Definition{"Cycle following distance", "iDistance"},
                                            Definition{"Cycle travel formation", "iFormation"}};
    inline constexpr std::size_t ActionCount = static_cast<std::size_t>(Action::Count);
    static_assert(Definitions.size() == ActionCount);
    using Bindings = std::array<std::uint32_t, ActionCount>;
    inline constexpr std::uint32_t Shift = 0x100, Control = 0x200, Alt = 0x400;
    inline constexpr std::uint32_t MaximumBinding = 0x7FF;
    inline constexpr std::uint32_t Escape = 0x01;

    class CaptureEscape {
    public:
        bool Consume(bool capturing, bool down, bool up) {
            if (capturing && down)
                swallowing = true;
            const bool consumed = swallowing;
            if (up)
                swallowing = false;
            return consumed;
        }

    private:
        bool swallowing = false;
    };

    constexpr std::uint32_t Modifier(std::uint32_t scanCode) {
        switch (scanCode) {
            case 0x2A:
            case 0x36:
                return Shift;
            case 0x1D:
            case 0x9D:
                return Control;
            case 0x38:
            case 0xB8:
                return Alt;
            default:
                return 0;
        }
    }

    constexpr bool Valid(std::uint32_t binding) {
        const auto key = binding & 0xFF;
        return !binding || (binding <= MaximumBinding && key > Escape && !Modifier(key) && key != 0xDB && key != 0xDC &&
                            key != 0xDD);
    }

    inline std::optional<std::size_t> Conflict(const Bindings& bindings, std::uint32_t binding,
                                               std::size_t except = ActionCount) {
        for (std::size_t i = 0; binding && i < bindings.size(); ++i)
            if (i != except && bindings[i] == binding)
                return i;
        return {};
    }

    class Keyboard {
    public:
        std::optional<std::uint32_t> Update(std::uint32_t key, bool pressed, bool initialDown) {
            if (key >= keys.size())
                return {};
            const bool wasPressed = keys[key];
            keys[key] = pressed;
            if (!pressed || !initialDown || wasPressed || !key || !Valid(key))
                return {};
            if (keys[0xDB] || keys[0xDC])
                return {};  // Windows shortcuts must not become unmodified game commands.
            std::uint32_t modifiers = 0;
            for (std::size_t i = 0; i < keys.size(); ++i)
                if (keys[i])
                    modifiers |= Modifier(static_cast<std::uint32_t>(i));
            return key | modifiers;
        }

        void Reset() {
            keys = {};
        }

    private:
        std::array<bool, 256> keys{};
    };

    constexpr bool GameplayAllowed(bool ready, bool paused, bool menuOpen, bool textEntry, bool controlsEnabled) {
        return ready && !paused && !menuOpen && !textEntry && controlsEnabled;
    }
}
