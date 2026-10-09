#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <functional>

#include "Party.h"

namespace mod::controller {
    struct Context : mod::party::Context {
        void (*restoreProtection)(RE::Actor*) = nullptr;
        std::int32_t (*getCap)() = nullptr;
        RE::TESQuest* homeQuest = nullptr;
        RE::TESFaction* homeFaction = nullptr;
        void (*applyDialogueGate)(std::int32_t count) = nullptr;
    };

    void Configure(Context context);
    void Invalidate();
    void GameLoaded();
    void ObservePause();
    std::string Status();

    struct PartySubmission {
        std::size_t targets = 0;
        std::string failure;
    };

    PartySubmission EnqueueParty(std::int32_t operation, std::int32_t uiTicket,
                                 std::function<void(bool, std::string)> completion = {});
    bool SubmitDebug(std::int32_t action, RE::Actor* actor, std::int32_t alias, std::int32_t ticket);
    void Activated(RE::Actor* actor);
    void Died(RE::Actor* actor);
    void CombatChanged(RE::Actor* actor, RE::Actor* target);
    void Unloaded(RE::Actor* actor);
    void RegisterPapyrus(RE::BSScript::IVirtualMachine* vm);
}
