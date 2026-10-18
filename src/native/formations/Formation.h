#pragma once

#include "RE/Skyrim.h"
#include <iosfwd>
#include <string>

namespace mod::formation {
    void Configure(RE::TESFaction* marker);
    void Loaded();
    void Invalidate();
    void Changed();
    void Observe(RE::Actor* actor);
    void RequestOffsetCleanup(RE::FormID actorID, std::uint64_t generation);
    void Tick();
    bool HasWork();
    std::string Status();
    void WriteDiagnostics(std::ostream& out);
    void RegisterPapyrus(RE::BSScript::IVirtualMachine* vm);
}
