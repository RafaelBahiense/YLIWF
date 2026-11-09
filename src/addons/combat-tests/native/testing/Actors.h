#pragma once
#include "Resources.h"
#include <cstdint>
#include <string>

namespace RE {
    class Actor;
    class TESNPC;
    class TESObjectREFR;
}

namespace mod::combat_tests::testing {
    // Main-thread services. The journal owns cleanup, not the returned pointer.
    RE::Actor* SpawnActor(Resources& resources, Scope scope, const std::string& key, RE::TESNPC* base,
                          RE::TESObjectREFR* anchor);
    Result AdoptActor(Resources& resources, Scope scope, const std::string& key, RE::Actor* actor,
                      std::uint32_t expectedBase);
    Result CheckArena(RE::Actor* subject, RE::TESObjectREFR* anchor);
}
