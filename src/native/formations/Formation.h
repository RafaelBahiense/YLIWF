#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>

namespace RE {
    class Actor;
    class TESFaction;

    namespace BSScript {
        class IVirtualMachine;
    }
}

namespace mod::formation {
    void Configure(RE::TESFaction* marker);
    void Loaded();
    void Invalidate();
    void Changed();
    void Observe(RE::Actor* actor);
    void RequestOffsetCleanup(std::uint32_t actorID, std::uint64_t generation);
    void Tick();
    bool HasWork();
    std::string Status();
    void WriteDiagnostics(std::ostream& out);
    void RegisterPapyrus(RE::BSScript::IVirtualMachine* vm);
}
