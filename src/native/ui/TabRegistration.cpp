#include "ui/TabRegistration.h"
#include "ui/MenuTabRegistry.h"
#include "SKSEMCP/SKSEMenuFramework.hpp"
#include "ModInfo.h"
#include <mutex>
#include <utility>

namespace {
    std::mutex mutex;
    mod::menu_tabs::Registry registry;

    template <std::size_t Index>
    void __stdcall Render() {
        yliwf::menu::Tab tab;
        {
            std::scoped_lock lock(mutex);
            const auto* registered = registry.At(static_cast<std::uint32_t>(Index));
            if (!registered)
                return;
            tab = *registered;
        }
        mod::ui::RenderExtension(tab);
    }

    template <std::size_t... Index>
    constexpr auto Callbacks(std::index_sequence<Index...>) {
        return std::array{&Render<Index>...};
    }

    std::uint32_t Register(const yliwf::menu::Tab* tab) {
        if (!tab || !SKSEMenuFramework::IsInstalled())
            return 0;
        std::scoped_lock lock(mutex);
        const auto result = registry.Add(*tab);
        if (result.inserted) {
            constexpr auto callbacks = Callbacks(std::make_index_sequence<yliwf::menu::MaxTabs>{});
            const auto* stored = registry.At(result.handle - 1);
            SKSEMenuFramework::SetSection(mod::info::DisplayName);
            SKSEMenuFramework::AddSectionItem(stored->title, callbacks[result.handle - 1]);
        }
        return result.handle;
    }
}

const yliwf::menu::API* mod::ui::MenuAPI() {
    static const yliwf::menu::API api{sizeof(api), yliwf::menu::Version, Register};
    return &api;
}

#ifdef YLIWF_UI_PREVIEW
void mod::ui::ResetPreviewTabs() {
    std::scoped_lock lock(mutex);
    registry = {};
}
#endif

extern "C" __declspec(dllexport) const yliwf::menu::API* YLIWF_GetMenuAPI(std::uint32_t version) {
    return version == yliwf::menu::Version ? mod::ui::MenuAPI() : nullptr;
}
