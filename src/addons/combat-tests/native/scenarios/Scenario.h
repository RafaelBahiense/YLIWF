#pragma once
#include "../testing/Definition.h"
#include <cstdint>

namespace mod::combat_tests::scenarios {
    enum class Execution { Defender, Healer, Primitive, Natural };

    struct Entry {
        std::uint32_t id, role;
        const char* category;
        const char* name;
        const char* description;
        const char* requirement;
        const testing::Suite* suite;
        std::uint32_t caseID, equipmentRole, control;
        Execution execution;
    };
}
