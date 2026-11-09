#pragma once

#include <cstdint>
#include <span>

namespace SKSE {
    class SerializationInterface;
}

namespace mod::combat_tests::travel {
    inline constexpr std::uint32_t Record = 0x4C565254;  // TRVL

    struct Snapshot {
        std::uint32_t size = sizeof(Snapshot), atArena = 0, canEnter = 0, canReturn = 0, pending = 0, hasReturn = 0;
        char location[128]{}, status[256]{}, blocked[192]{};
    };

    // Runner mutex protects these calls. Update is main-thread only;
    // snapshot reads, request submission and serialization never access the engine.
    void Update(bool ready, bool testBusy);
    bool Request(bool returnToOrigin);
    Snapshot Read();
    bool NeedsUpdate();
    bool InvalidRecord();
    void Reset();
    void Save(SKSE::SerializationInterface* serialization);
    void Load(SKSE::SerializationInterface* serialization, std::uint32_t version, std::span<const std::uint8_t> bytes);
}
