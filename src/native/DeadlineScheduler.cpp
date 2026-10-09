#include "DeadlineScheduler.h"

namespace mod {
    DeadlineScheduler::DeadlineScheduler(WakeCallback wake)
        : wake(wake), worker([this](std::stop_token stop) { Run(stop); }) {}

    void DeadlineScheduler::Set(bool value, bool fast) {
        std::scoped_lock lock(mutex);
        active = value;
        interval = std::chrono::milliseconds(fast ? 50 : 1000);
        changed.notify_all();
    }

    void DeadlineScheduler::Run(std::stop_token stop) {
        std::unique_lock lock(mutex);
        while (!stop.stop_requested()) {
            changed.wait(lock, stop, [this] { return active; });
            if (stop.stop_requested())
                break;

            if (!changed.wait_for(lock, stop, interval, [this] { return !active; })) {
                lock.unlock();
                wake();
                lock.lock();
            }
        }
    }
}
