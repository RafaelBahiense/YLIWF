#pragma once

#include "ControllerRuntime.h"

namespace mod::controller::executor {
    void Wake();
    void Kick();
    bool CanStartNow();
    void RequestCounts();
    void Invalidate();
    void Loaded();
    void ObservePause();
    void RegisterPapyrus(RE::BSScript::IVirtualMachine* vm);
    bool SubmitDebug(std::int32_t action, RE::Actor* actor, std::int32_t alias, std::int32_t ticket);
}
