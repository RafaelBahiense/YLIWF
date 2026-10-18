#pragma once
#include "SKSEMCP/SKSEMenuFramework.hpp"

namespace mod::ui {
    void Register();
    bool IsRefreshVisible();

    void __stdcall RenderSettings();
    void __stdcall RenderFollowers();
    void __stdcall RenderDebug();
}
