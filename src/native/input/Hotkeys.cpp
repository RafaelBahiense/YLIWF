#include "input/Hotkeys.h"
#include "settings/Settings.h"
#include "ui/UI.h"
#include "diagnostics/Debug.h"
#include "followers/ControllerStorage.h"
#include "combat/CombatOrders.h"

namespace mod::hotkeys {
    namespace {
        using hotkey_rules::Action;
        hotkey_rules::Keyboard keyboard;
        hotkey_rules::CaptureEscape captureEscape;
        std::optional<Action> capture;
        std::string status;

        bool GameplayAllowed() {
            auto* ui = RE::UI::GetSingleton();
            auto* controls = RE::ControlMap::GetSingleton();
            auto* main = RE::Main::GetSingleton();
            if (!ui || !controls || !main || !main->gameActive)
                return false;
            bool menu = mod::ui::IsMenuOpen() || ui->numItemMenus || ui->numApplicationMenus;
            for (const auto name : {"Console", "Dialogue Menu", "Loading Menu", "Main Menu"})
                menu |= ui->IsMenuOpen(name);
            return hotkey_rules::GameplayAllowed(controller::storage::Available(), ui->GameIsPaused(), menu,
                                                 controls->textEntryCount > 0, controls->IsMovementControlsEnabled());
        }

        void Execute(Action action, std::uint32_t binding, std::uint64_t generation) {
            std::scoped_lock lock(settings::Mutex);
            if (!debug::IsCurrentGame(generation) || capture || !GameplayAllowed() ||
                settings::Hotkeys[static_cast<std::size_t>(action)] != binding)
                return;
            std::string message;
            switch (action) {
                case Action::FollowAll:
                case Action::WaitAll:
                    debug::RequestPartyAction(action == Action::FollowAll ? debug::Action::Follow : debug::Action::Wait,
                                              generation);
                    message = action == Action::FollowAll ? "Follow All requested" : "Wait All requested";
                    break;
                case Action::StandDown:
                    if (!combat_orders::Ready)
                        return;
                    combat_orders::Set(!combat_orders::Enabled);
                    message = combat_orders::Enabled ? "Normal combat behavior requested" : "Stand down requested";
                    break;
                case Action::Distance: {
                    settings::FollowDistance = (settings::FollowDistance + 1) % 3;
                    if (settings::FollowDistanceCallback)
                        settings::FollowDistanceCallback();
                    constexpr std::array names{"Close", "Normal", "Far"};
                    message = std::string("Following distance: ") + names[settings::FollowDistance];
                    break;
                }
                case Action::Formation:
                    settings::Formation =
                        static_cast<formation_geometry::Style>((static_cast<int>(settings::Formation) + 1) % 4);
                    if (settings::FormationCallback)
                        settings::FormationCallback();
                    message =
                        std::string("Travel formation: ") + std::string(formation_geometry::Name(settings::Formation));
                    break;
                default:
                    return;
            }
            RE::DebugNotification(message.c_str());
        }

        void KeyDown(std::uint32_t binding) {
            if (capture) {
                if (!mod::ui::IsMenuOpen()) {
                    capture.reset();
                    return;
                }
                const auto index = static_cast<std::size_t>(*capture);
                if (const auto conflict = hotkey_rules::Conflict(settings::Hotkeys, binding, index)) {
                    status =
                        std::string("Already assigned to ") + std::string(hotkey_rules::Definitions[*conflict].name);
                    return;
                }
                settings::Hotkeys[index] = binding;
                status = "Binding changed. Save to keep it after restarting Skyrim.";
                capture.reset();
                keyboard.Reset();
                return;
            }
            if (!GameplayAllowed())
                return;
            const auto action = hotkey_rules::Conflict(settings::Hotkeys, binding);
            auto* tasks = SKSE::GetTaskInterface();
            if (action && tasks) {
                const auto generation = debug::Generation();
                tasks->AddTask([action = static_cast<Action>(*action), binding, generation] {
                    Execute(action, binding, generation);
                });
            }
        }

        class InputSink final : public RE::BSTEventSink<RE::InputEvent*> {
        public:
            RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events,
                                                  RE::BSTEventSource<RE::InputEvent*>*) override {
                std::scoped_lock lock(settings::Mutex);
                for (auto* event = events ? *events : nullptr; event; event = event->next) {
                    const auto* button = event->AsButtonEvent();
                    if (!button || event->GetDevice() != RE::INPUT_DEVICE::kKeyboard)
                        continue;
                    const auto key = button->GetIDCode();
                    const auto binding = keyboard.Update(key, button->IsPressed(), button->IsDown());
                    if (capture && key == hotkey_rules::Escape && button->IsDown())
                        CancelCapture();
                    else if (binding)
                        KeyDown(*binding);
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        bool CaptureButton(const RE::ButtonEvent& button) {
            std::scoped_lock lock(settings::Mutex);
            if (capture && !mod::ui::IsMenuOpen())
                CancelCapture();
            const auto key = button.GetIDCode();
            if (key == hotkey_rules::Escape) {
                const bool consume = captureEscape.Consume(capture.has_value(), button.IsDown(), button.IsUp());
                if (capture && button.IsDown())
                    CancelCapture();
                return consume;
            }
            if (!capture)
                return false;
            if (const auto binding = keyboard.Update(key, button.IsPressed(), button.IsDown()))
                KeyDown(*binding);
            return true;
        }

        struct CaptureHook {
            static void Input(RE::BSTEventSource<RE::InputEvent*>* dispatcher, RE::InputEvent* const* events) {
                auto* head = events ? *events : nullptr;
                RE::InputEvent* previous = nullptr;
                std::vector<std::pair<RE::InputEvent*, RE::InputEvent*>> changedLinks;
                for (auto* event = head; event; event = event->next) {
                    const auto* button = event->AsButtonEvent();
                    if (!button || event->GetDevice() != RE::INPUT_DEVICE::kKeyboard || !CaptureButton(*button)) {
                        previous = event;
                        continue;
                    }
                    if (previous) {
                        changedLinks.emplace_back(previous, previous->next);
                        previous->next = event->next;
                    } else {
                        head = event->next;
                    }
                }
                original(dispatcher, &head);
                for (auto it = changedLinks.rbegin(); it != changedLinks.rend(); ++it)
                    it->first->next = it->second;
            }

            static inline REL::Relocation<decltype(Input)> original;
        };
    }

    void Install() {
        // The framework closes on Escape before its public callbacks or game
        // event sinks. Capture rebinding input first, chaining its existing hook.
        // https://github.com/QTR-Modding/SKSE-Menu-Framework-3/blob/master/src/Hooks.cpp
        SKSE::AllocTrampoline(32);
        CaptureHook::original = SKSE::GetTrampoline().write_call<5>(
            REL::RelocationID(67315, 68617).address() + REL::Relocate(0x7B, 0x7B, 0x81), CaptureHook::Input);
        if (auto* input = RE::BSInputDeviceManager::GetSingleton()) {
            static InputSink sink;
            input->AddEventSink(&sink);
        }
    }

    void Reset() {
        std::scoped_lock lock(settings::Mutex);
        keyboard.Reset();
        capture.reset();
        status.clear();
    }

    void BeginCapture(hotkey_rules::Action action) {
        std::scoped_lock lock(settings::Mutex);
        if (static_cast<std::size_t>(action) >= hotkey_rules::ActionCount)
            return;
        capture = action;
        keyboard.Reset();
        status = "Press a key combination. Escape cancels.";
    }

    void CancelCapture() {
        std::scoped_lock lock(settings::Mutex);
        if (capture) {
            capture.reset();
            keyboard.Reset();
            status = "Binding cancelled.";
        }
    }

    bool IsCapturing(hotkey_rules::Action action) {
        std::scoped_lock lock(settings::Mutex);
        return capture == action;
    }

    void Clear(hotkey_rules::Action action) {
        std::scoped_lock lock(settings::Mutex);
        if (static_cast<std::size_t>(action) >= hotkey_rules::ActionCount)
            return;
        capture.reset();
        settings::Hotkeys[static_cast<std::size_t>(action)] = 0;
        status = "Binding cleared. Save to keep it after restarting Skyrim.";
    }

    std::string Status() {
        std::scoped_lock lock(settings::Mutex);
        return status;
    }

    std::string Describe(std::uint32_t binding) {
        if (!binding)
            return "Unbound";
        std::string name;
        if (binding & hotkey_rules::Control)
            name += "Ctrl + ";
        if (binding & hotkey_rules::Shift)
            name += "Shift + ";
        if (binding & hotkey_rules::Alt)
            name += "Alt + ";
        const auto key = binding & 0xFF;
        char label[128]{};
        const auto scan = static_cast<LONG>((key & 0x7F) << 16 | (key & 0x80 ? 1u << 24 : 0));
        if (GetKeyNameTextA(scan, label, static_cast<int>(sizeof(label))))
            name += label;
        else
            name += fmt::format("Key {:02X}", key);
        return name;
    }
}
