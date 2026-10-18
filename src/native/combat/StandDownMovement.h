#pragma once

namespace RE {
    class Actor;
    class TESPackage;
}

namespace mod::stand_down_movement {
    bool Configure();
    bool Owned(const RE::TESPackage* package);
    RE::TESPackage* Select(RE::Actor* actor);
    void Release(RE::Actor* actor);
    void Invalidate();
}
