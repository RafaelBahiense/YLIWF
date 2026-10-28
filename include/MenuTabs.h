#pragma once

#include <cstdint>

namespace yliwf::menu {
    inline constexpr std::uint32_t Version = 1;
    inline constexpr char QueryExport[] = "YLIWF_GetMenuAPI";
    inline constexpr std::uint32_t MaxTabs = 16;

    struct Follower {
        std::uint32_t actor = 0, loaded = 0, waiting = 0, dead = 0, readyRoles = 0;
        char name[128]{};
    };

    // Cached core-managed followers. Valid only during the render callback;
    // do not retain the context or its follower pointer. No engine access.
    struct Context {
        std::uint32_t size = sizeof(Context), ready = 0, count = 0;
        const Follower* followers = nullptr;
    };

    struct Tab {
        std::uint32_t size = sizeof(Tab), version = Version;
        char id[64]{}, title[64]{};
        void* context = nullptr;
        // Menu render thread. Use ImGuiMCP for drawing; queue engine operations
        // through a validated service. No exceptions across this boundary.
        void(__stdcall* render)(void*, const Context*) = nullptr;
    };

    struct API {
        std::uint32_t size = sizeof(API), version = Version;
        // Register during SKSE PostLoad, before menus open. Returns a nonzero
        // handle or 0 on rejection. Tab strings are copied. Callbacks/context
        // must remain valid until process exit; unloading is unsupported.
        std::uint32_t (*registerTab)(const Tab*) = nullptr;
    };

    using QueryAPI = const API* (*)(std::uint32_t);
    static_assert(sizeof(Follower) == 148 && sizeof(Context) == 24 && sizeof(Tab) == 152 && sizeof(API) == 16);
}
