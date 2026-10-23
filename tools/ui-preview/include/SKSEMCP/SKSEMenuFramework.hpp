#pragma once

// Preview backend for the production UI sources. Drawing uses real Dear ImGui;
// framework registration is hosted by the preview window instead of SKSE.
#include <windows.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <atomic>
#include <string>
#include <vector>

namespace ImGuiMCP {
    using namespace ImGui;
    using ::ImGuiCol_Border;
    using ::ImGuiCol_Button;
    using ::ImGuiCol_ChildBg;
    using ::ImGuiCol_Header;
    using ::ImGuiCol_HeaderActive;
    using ::ImGuiCol_HeaderHovered;
    using ::ImGuiCol_Text;
    using ::ImGuiHoveredFlags_AllowWhenDisabled;
    using ::ImGuiItemFlags_Disabled;
    using ::ImGuiStyleVar_Alpha;
    using ::ImGuiStyleVar_ChildBorderSize;
    using ::ImGuiStyleVar_WindowPadding;
    using ::ImGuiTableColumnFlags_WidthFixed;
    using ::ImGuiTableColumnFlags_WidthStretch;
    using ::ImGuiTableFlags_SizingFixedFit;
    using ::ImGuiWindowFlags_AlwaysAutoResize;
    using ::ImGuiWindowFlags_AlwaysVerticalScrollbar;
    using ::ImVec2;
    using ::ImVec4;
    inline constexpr auto ImGuiChildFlags_Border = ::ImGuiChildFlags_Borders;

    inline ImGuiStyle* GetStyle() {
        return &ImGui::GetStyle();
    }

    inline void GetContentRegionAvail(ImVec2* output) {
        *output = ImGui::GetContentRegionAvail();
    }

    inline void GetCursorPos(ImVec2* output) {
        *output = ImGui::GetCursorPos();
    }

    inline void CalcTextSize(ImVec2* output, const char* text, const char* end, bool hide, float wrap) {
        *output = ImGui::CalcTextSize(text, end, hide, wrap);
    }
}

namespace SKSEMenuFramework {
    namespace Model {
        struct WindowInterface {
            std::atomic_bool IsOpen{true};
        };

        namespace Internal {
            template <class T>
            T GetFunction(const char*) {
                return nullptr;
            }
        }
    }

    using RenderFunction = void(__stdcall*)();

    struct Page {
        std::string name;
        RenderFunction render;
    };

    inline std::vector<Page> Pages;
    inline RenderFunction Observer = nullptr;

    inline bool IsInstalled() {
        return true;
    }

    inline bool IsAnyBlockingWindowOpened() {
        return true;
    }

    inline void AddHudElement(RenderFunction callback) {
        Observer = callback;
    }

    inline void SetSection(const char*) {}

    inline void AddSectionItem(const char* name, RenderFunction render) {
        Pages.push_back({name, render});
    }
}
