#pragma once

#include <windows.h>
#include <filesystem>
#include <string_view>

namespace mod::settings_file {
    enum class Result { Created, Existing, Failed };
    struct Creation { Result result; std::error_code error; };
    inline Creation CreateMissing(const std::filesystem::path& path, std::string_view defaults) {
        std::error_code error;
        if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path(), error);
        if (error) return {Result::Failed, error};
        const auto file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            const auto code = GetLastError();
            if (code == ERROR_FILE_EXISTS || code == ERROR_ALREADY_EXISTS) return {Result::Existing, {}};
            return {Result::Failed, std::error_code(code, std::system_category())};
        }
        DWORD written = 0;
        const bool saved = WriteFile(file, defaults.data(), static_cast<DWORD>(defaults.size()), &written, nullptr) && written == defaults.size();
        const auto code = saved ? ERROR_SUCCESS : GetLastError();
        CloseHandle(file);
        return {saved ? Result::Created : Result::Failed, std::error_code(code ? code : (saved ? ERROR_SUCCESS : ERROR_WRITE_FAULT), std::system_category())};
    }
}
