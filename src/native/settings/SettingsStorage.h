#pragma once

#include "GameplaySettings.h"

namespace SKSE {
    class SerializationInterface;
}

namespace mod::settings::storage {
    // Called by the core's sole serialization owner; no extra SKSE callbacks.
    void Reset();
    void Read(SKSE::SerializationInterface* serialization, std::uint32_t version, std::uint32_t length);
    void Loaded();
    void Save(SKSE::SerializationInterface* serialization);
}
