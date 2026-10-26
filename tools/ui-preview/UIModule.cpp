#include "UIAPI.h"
#include "Simulation.h"
#include "Style.h"
#include "ui/UI.h"
#include "input/Hotkeys.h"
#include "settings/PreferenceState.h"
#include <cstdio>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {
    preview::Session* session = nullptr;
    bool win32Ready = false, dxReady = false;

    void Detach() {
        if (!ImGui::GetCurrentContext())
            return;
        if (session)
            preview::SaveSession(*session);
        // Only module-owned callbacks are retained here; destroy them before unload.
        SKSEMenuFramework::Pages.clear();
        SKSEMenuFramework::Observer = nullptr;
        mod::hotkeys::CancelCapture();
        if (dxReady)
            ImGui_ImplDX11_Shutdown();
        if (win32Ready)
            ImGui_ImplWin32_Shutdown();
        dxReady = win32Ready = false;
        ImGui::DestroyContext();
        session = nullptr;
    }

    bool Attach(HWND window, ID3D11Device* device, ID3D11DeviceContext* context, preview::Session* state) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::GetIO().IniFilename = nullptr;
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        if (!preview::ApplyStyle()) {
            Detach();
            return false;
        }
        mod::settings::Load();
        preview::Reset(preview::Scenario::Party);
        preview::RestoreSession(*state);
        ImGui::GetIO().FontGlobalScale = state->scale;
        mod::ui::Register();
        if (window) {
            win32Ready = ImGui_ImplWin32_Init(window);
            dxReady = win32Ready && ImGui_ImplDX11_Init(device, context);
            if (!dxReady) {
                Detach();
                return false;
            }
        }
        session = state;
        return true;
    }

    void Draw() {
        preview::Tick();
        if (SKSEMenuFramework::Observer)
            SKSEMenuFramework::Observer();
        const auto* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("YLIWF preview", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
        auto& scenario = session->scenario;
        constexpr const char* scenarios[] = {"Party",           "Empty",          "Quest restrictions",
                                             "Pending command", "Failed command", "No game loaded"};
        ImGui::SetNextItemWidth(180);
        if (ImGui::Combo("Scenario", &scenario, scenarios, IM_ARRAYSIZE(scenarios)))
            preview::Reset(static_cast<preview::Scenario>(scenario));
        ImGui::SameLine();
        if (ImGui::Button("Reset"))
            preview::Reset(static_cast<preview::Scenario>(scenario));
        ImGui::SameLine();
        auto& scale = session->scale;
        ImGui::SetNextItemWidth(110);
        if (ImGui::SliderFloat("Scale", &scale, 0.75f, 1.75f, "%.2f"))
            ImGui::GetIO().FontGlobalScale = scale;
        ImGui::TextDisabled("Standalone preview | Sample data | Actions and settings stay in memory");
        ImGui::TextWrapped("%s", session->reloadStatus.c_str());
        if (ImGui::BeginTabBar("Pages")) {
            for (const auto& page : SKSEMenuFramework::Pages) {
                if (ImGui::BeginTabItem(page.name.c_str())) {
                    ImGui::BeginChild("Page content");
                    page.render();
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
        ImGui::End();
    }

    int SmokeTest() {
        if (session->valid &&
            (mod::settings::FollowDistance != session->distance || mod::settings::SaveStatus != session->saveStatus ||
             mod::debug::GetSnapshot().generation != session->party.generation))
            return 4;
        using namespace mod::settings;
        preference_state::RememberSaved();
        const auto originalDistance = FollowDistance;
        FollowDistance = (FollowDistance + 1) % 3;
        if (preference_state::HasChanges())
            return 5;
        FollowDistance = originalDistance;
        if (preference_state::HasChanges())
            return 6;
        const auto originalLogging = mod::debug::Logging.load();
        mod::debug::Logging = !originalLogging;
        if (!preference_state::HasChanges())
            return 7;
        Save();
        if (preference_state::HasChanges())
            return 8;
        mod::debug::Logging = originalLogging;
        preference_state::RememberSaved();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        unsigned char* pixels = nullptr;
        int width = 0, height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        if (!pixels || width <= 0 || height <= 0)
            return 1;
        mod::debug::OptionsEnabled = true;
        for (const float size : {640.0f, 960.0f}) {
            io.DisplaySize = {size, 720};
            io.DeltaTime = 1.0f / 60;
            for (int scenario = 0; scenario < 6; ++scenario) {
                preview::Reset(static_cast<preview::Scenario>(scenario));
                for (const auto& page : SKSEMenuFramework::Pages) {
                    for (int frame = 0; frame < 3; ++frame) {
                        ImGui::NewFrame();
                        ImGui::SetNextWindowSize(io.DisplaySize);
                        ImGui::Begin("Smoke test");
                        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
                        page.render();
                        ImGui::End();
                        ImGui::Render();
                        if (!ImGui::GetDrawData()->Valid || ImGui::GetDrawData()->TotalVtxCount <= 0)
                            return 2;
                    }
                }
            }
        }
        preview::Reset(preview::Scenario::Party);
        preview::Session actionFixture;
        preview::SaveSession(actionFixture);
        actionFixture.party.followers.clear();
        std::erase_if(actionFixture.party.adaptedFollowers, [](const auto& row) { return row.adapterActions.empty(); });
        if (actionFixture.party.adaptedFollowers.size() != 1)
            return 9;
        for (const bool enabled : {true, false}) {
            auto& action = actionFixture.party.adaptedFollowers.front().adapterActions.front();
            action.enabled = static_cast<std::uint32_t>(enabled);
            if (!enabled)
                std::snprintf(action.reason, sizeof(action.reason), "A quest currently controls this follower.");
            preview::RestoreSession(actionFixture);
            for (const float size : {640.0f, 960.0f}) {
                io.DisplaySize = {size, 720};
                ImGui::NewFrame();
                ImGui::SetNextWindowSize(io.DisplaySize);
                ImGui::Begin("Adapter actions smoke test");
                mod::ui::RenderFollowers();
                ImGui::End();
                ImGui::Render();
                if (!ImGui::GetDrawData()->Valid || ImGui::GetDrawData()->TotalVtxCount <= 0)
                    return 10;
            }
        }
        preview::Reset(preview::Scenario::Party);
        mod::settings::FollowDistance = 2;
        mod::settings::SaveStatus = "Reload persistence verified";
        return 0;
    }

    void Frame() {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        Draw();
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    LRESULT Message(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)
            preview::CaptureKey(static_cast<unsigned int>(wParam));
        return ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam);
    }
}

extern "C" __declspec(dllexport) const preview::UIAPI* Preview_GetUI() {
    static const preview::UIAPI api{
        1, sizeof(preview::Session), sizeof(mod::debug::Follower), Attach, Detach, Frame, Message, SmokeTest};
    return &api;
}
