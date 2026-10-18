#pragma once
#include "SKSEMCP/SKSEMenuFramework.hpp"

namespace mod::ui {
    void Register();
    bool IsRefreshVisible();
    bool IsMenuOpen();

    void __stdcall RenderSettings();
    void __stdcall RenderFollowers();
    void __stdcall RenderDebug();
    void __stdcall RenderHotkeys();
}
