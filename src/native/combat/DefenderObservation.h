#pragma once
#include "DefenderTactics.h"
#include <vector>

namespace RE {
    class Actor;
}

namespace mod::defender_observation {
    std::vector<defender_tactics::Threat> Threats(RE::Actor* actor);
}
