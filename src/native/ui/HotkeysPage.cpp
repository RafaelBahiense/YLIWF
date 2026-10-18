#include "HotkeysPage.h"
#include "input/Hotkeys.h"
#include "settings/Settings.h"
#include "SKSEMCP/SKSEMenuFramework.hpp"

namespace mod::ui::hotkeys_page {
    void Render() {
        std::scoped_lock lock(settings::Mutex);
        ImGuiMCP::TextWrapped("Click a binding, then press a key or a Shift/Ctrl/Alt combination. Escape cancels.");
        ImGuiMCP::TextWrapped(
            "Hotkeys work during gameplay, outside menus and text entry. Other mods may use the same keys.");
        ImGuiMCP::Spacing();
        for (std::size_t i = 0; i < hotkey_rules::Definitions.size(); ++i) {
            const auto action = static_cast<hotkey_rules::Action>(i);
            ImGuiMCP::PushID(static_cast<int>(i));
            ImGuiMCP::TextUnformatted(hotkey_rules::Definitions[i].name.data());
            const auto label = hotkeys::IsCapturing(action) ? "Press keys..." : hotkeys::Describe(settings::Hotkeys[i]);
            if (ImGuiMCP::Button(label.c_str()))
                hotkeys::BeginCapture(action);
            ImGuiMCP::SameLine();
            if (ImGuiMCP::Button("Clear"))
                hotkeys::Clear(action);
            ImGuiMCP::PopID();
            ImGuiMCP::Spacing();
        }
        ImGuiMCP::TextWrapped(
            "Distance changes apply to the whole party, replacing individual choices. Dismiss All stays menu-only.");
        if (ImGuiMCP::Button("Save settings"))
            settings::Save();
        ImGuiMCP::SameLine();
        if (ImGuiMCP::Button("Cancel capture"))
            hotkeys::CancelCapture();
        const auto status = hotkeys::Status();
        if (!status.empty())
            ImGuiMCP::TextWrapped("%s", status.c_str());
        if (!settings::SaveStatus.empty())
            ImGuiMCP::TextWrapped("%s", settings::SaveStatus.c_str());
    }
}
