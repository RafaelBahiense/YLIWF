#pragma once

namespace RE {
    class Actor;
}

namespace mod::serana_reanimation {
    void Initialize(RE::Actor* actor);
    void RegisterSerialization();
    void Invalidate();
    void NewGame();
    void GameLoaded();
    bool Available();
    bool Allowed();
    bool SetAllowed(bool allowed);
}
