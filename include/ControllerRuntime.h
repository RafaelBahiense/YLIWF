#pragma once

#include "Controller.h"
#include "ControllerStorage.h"

// Native controller services. Papyrus handles/tags belong only in the public bridge.
namespace mod::controller::detail {
    const Context& GetContext();
    storage::Script Object();
    std::int32_t Enqueue(controller_rules::Operation operation, RE::Actor* actor,
        std::int32_t selected = -1, std::int32_t message = 0, std::int32_t sayLine = 1, std::int32_t debugTicket = 0,
        std::optional<storage::Caller> caller = std::nullopt);
    bool StartNext();
    bool StartCounts();
    bool Validate(const storage::Step& step);
    bool Acknowledge(std::int32_t ticket, std::int32_t cursor, bool succeeded);
    bool Complete(bool succeeded);
    std::int32_t Cap();
    std::int32_t Count();
}
