#pragma once

#include "SKSEMCP/SKSEMenuFramework.hpp"
#include "settings/PreferenceState.h"

namespace mod::ui {
    inline void PreferencesFooter(bool allowReload = true) {
        std::scoped_lock lock(settings::Mutex);
        ImGuiMCP::Spacing();
        ImGuiMCP::Separator();
        ImGuiMCP::Spacing();
        if (ImGuiMCP::Button("Save settings"))
            settings::Save();
        if (allowReload) {
            ImGuiMCP::SameLine();
            if (ImGuiMCP::Button("Reload")) {
                settings::Load(true);
                for (const auto callback :
                     {settings::FriendlyFireCallback, settings::CrossfireCallback, settings::SandboxCallback,
                      settings::HomesCallback, settings::EssentialCallback, settings::FormationCallback,
                      settings::ApplyGateCallback})
                    if (callback)
                        callback();
            }
        }
        ImGuiMCP::SameLine();
        const bool changed = settings::preference_state::HasChanges();
        ImGuiMCP::TextColored(
            changed ? ImGuiMCP::ImVec4{0.85f, 0.72f, 0.40f, 1} : ImGuiMCP::ImVec4{0.65f, 0.65f, 0.65f, 1}, "%s",
            changed ? "Unsaved changes" : "No unsaved changes");
        if (!settings::SaveStatus.empty())
            ImGuiMCP::TextWrapped("%s", settings::SaveStatus.c_str());
    }
}
