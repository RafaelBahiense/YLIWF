#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace mod {
    // Runs only the wake callback; engine/VM work belongs on the main thread.
    class DeadlineScheduler {
    public:
        using WakeCallback = void (*)();

        explicit DeadlineScheduler(WakeCallback wake);
        void Set(bool value, bool fast = false);

    private:
        void Run(std::stop_token stop);

        const WakeCallback wake;
        std::mutex mutex;
        std::condition_variable_any changed;
        bool active = false;
        std::chrono::milliseconds interval{1000};
        std::jthread worker;
    };
}
