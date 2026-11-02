#pragma once
#include "Guard.h"
#include "Bash.h"
#include "Attack.h"

namespace mod::combat_actions {
    GuardSample ReadGuard(RE::Actor* actor);
    BashSample ReadBash(RE::Actor* actor);
    AttackSample ReadAttack(RE::Actor* actor);
}
