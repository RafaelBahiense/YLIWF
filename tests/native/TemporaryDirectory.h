#pragma once

#include <chrono>
#include <filesystem>

namespace tests {
    struct TemporaryDirectory {
        const std::filesystem::path path =
            std::filesystem::current_path() /
            ("settings-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

        ~TemporaryDirectory() {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    };
}
