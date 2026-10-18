#pragma once

#include "input/HotkeyRules.h"
#include <string>

namespace mod::hotkeys {
    void Install();
    void Reset();
    void BeginCapture(hotkey_rules::Action action);
    void CancelCapture();
    bool IsCapturing(hotkey_rules::Action action);
    void Clear(hotkey_rules::Action action);
    std::string Status();
    std::string Describe(std::uint32_t binding);
}
