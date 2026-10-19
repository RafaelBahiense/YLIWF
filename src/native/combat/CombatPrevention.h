#pragma once

namespace RE {
    class Actor;
}

namespace mod::combat_prevention {
    bool Configure();
    bool Acquire(RE::Actor* actor);
    void Release(RE::Actor* actor);
    void ReconcileLoadedActors();
}
