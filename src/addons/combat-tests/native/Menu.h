#pragma once

#include "MenuTabs.h"

namespace mod::combat_tests {
    bool RegisterMenu(const yliwf::menu::API* api);
    void RegisterMenuControl();
    void RequestMenu(bool open);
    void ClearMenuRequest();
}
