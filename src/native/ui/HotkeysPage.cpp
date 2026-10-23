#include "HotkeysPage.h"
#include "ui/PreferencesFooter.h"
#include "input/Hotkeys.h"
#include "settings/Settings.h"
#include "SKSEMCP/SKSEMenuFramework.hpp"
#include <algorithm>

namespace mod::ui::hotkeys_page {
    void Render() {
        std::scoped_lock lock(settings::Mutex);
        ImGuiMCP::ImVec2 available;
        ImGuiMCP::GetContentRegionAvail(&available);
        ImGuiMCP::BeginChild("Hotkeys body", {0, std::max(available.y - ImGuiMCP::GetFontSize() * 4.5f, 1.0f)});
        ImGuiMCP::SeparatorText("PARTY ACTIONS");
        ImGuiMCP::TextWrapped("Select a binding, then press a key combination. Escape cancels.");
        ImGuiMCP::Spacing();
        bool capturing = false;
        if (ImGuiMCP::BeginTable("Bindings", 3)) {
            ImGuiMCP::TableSetupColumn("Action", ImGuiMCP::ImGuiTableColumnFlags_WidthStretch);
            ImGuiMCP::TableSetupColumn("Binding", ImGuiMCP::ImGuiTableColumnFlags_WidthFixed,
                                       ImGuiMCP::GetFontSize() * 9);
            ImGuiMCP::TableSetupColumn("##Clear", ImGuiMCP::ImGuiTableColumnFlags_WidthFixed,
                                       ImGuiMCP::GetFontSize() * 3.5f);
            ImGuiMCP::TableHeadersRow();
            for (std::size_t i = 0; i < hotkey_rules::Definitions.size(); ++i) {
                const auto action = static_cast<hotkey_rules::Action>(i);
                const bool active = hotkeys::IsCapturing(action);
                capturing |= active;
                ImGuiMCP::PushID(static_cast<int>(i));
                ImGuiMCP::TableNextRow(0, ImGuiMCP::GetFontSize() * 2.2f);
                ImGuiMCP::TableNextColumn();
                ImGuiMCP::TextWrapped("%s", hotkey_rules::Definitions[i].name.data());
                ImGuiMCP::TableNextColumn();
                const auto label = active ? "Press keys..." : hotkeys::Describe(settings::Hotkeys[i]);
                if (active)
                    ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Button, ImGuiMCP::ImVec4{0.35f, 0.30f, 0.16f, 1});
                if (ImGuiMCP::Button(label.c_str(), {-1, 0}))
                    hotkeys::BeginCapture(action);
                if (active)
                    ImGuiMCP::PopStyleColor();
                const auto status = hotkeys::Status();
                const auto conflict = hotkey_rules::Conflict(settings::Hotkeys, settings::Hotkeys[i], i);
                if (conflict)
                    ImGuiMCP::TextWrapped("Also assigned to %s", hotkey_rules::Definitions[*conflict].name.data());
                else if (active && status.starts_with("Already assigned"))
                    ImGuiMCP::TextWrapped("%s", status.c_str());
                ImGuiMCP::TableNextColumn();
                if (ImGuiMCP::Button("Clear"))
                    hotkeys::Clear(action);
                ImGuiMCP::PopID();
            }
            ImGuiMCP::EndTable();
        }
        ImGuiMCP::Spacing();
        ImGuiMCP::TextWrapped("Hotkeys work outside menus and text entry. Other mods may use the same keys.");
        ImGuiMCP::TextWrapped(
            "Distance changes apply to the whole party, replacing individual choices. Dismiss All stays menu-only.");
        if (capturing && ImGuiMCP::Button("Cancel capture"))
            hotkeys::CancelCapture();
        const auto status = hotkeys::Status();
        if (!status.empty() && !status.starts_with("Already assigned"))
            ImGuiMCP::TextWrapped("%s", status.c_str());
        ImGuiMCP::EndChild();
        PreferencesFooter(false);
    }
}
