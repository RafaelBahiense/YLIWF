#pragma once

#include "FollowerPolicy.h"
#include "followers/ControllerRules.h"
#include <string>

namespace mod::quest_policies {
    using yliwf::policy::Action;
    yliwf::policy::State Inspect(RE::Actor* actor);
    bool Allows(RE::Actor* actor, Action action);
    bool CanRelease(RE::Actor* actor, RE::TESQuest* owner = nullptr);
    bool AllowsOperation(RE::Actor* actor, controller_rules::Operation operation);
}
