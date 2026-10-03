#pragma once

namespace RE {
    class Actor;
    class TESQuest;
    class TESFaction;
}

namespace mod::party {
    struct Context {
        RE::TESQuest* quest = nullptr;
        RE::TESFaction* currentFaction = nullptr;
        RE::TESFaction* potentialFaction = nullptr;
        void (*applyProtection)(RE::Actor*) = nullptr;
    };
}
