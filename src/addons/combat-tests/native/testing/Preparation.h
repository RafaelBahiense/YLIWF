#pragma once
#include "Definition.h"
#include "Resources.h"

namespace mod::combat_tests::testing {
    inline Result PrepareArena(Context& context) {
        return context.PrepareArena();
    }

    inline Result PrepareArenaWithObserver(Context& context) {
        const auto arena = context.PrepareArena();
        return arena.state == State::Ready ? context.PreparePlayer() : arena;
    }
}
