#include "Settings.h"
#include "SettingsRules.h"
#include "SettingsFile.h"
#include "SettingsDefaults.h"
#include "Debug.h"

#include <algorithm>
#include <limits>
#include <string>

namespace mod::settings {
    namespace {
        std::string_view DefaultValue(const char* section, const char* key) {
            return settings_rules::IniValue(settings_defaults::Ini, section, key).value_or("");
        }

        std::optional<std::string> ReadSetting(const char* section, const char* key) {
            constexpr char missing[] = "\x1F";
            char buffer[2048]{};
            const auto length =
                GetPrivateProfileStringA(section, key, missing, buffer, static_cast<DWORD>(sizeof(buffer)), kIniPath);
            if (std::string_view(buffer) == missing)
                return {};
            if (length == sizeof(buffer) - 1) {
                logger::warn("INI [{}] {} is too long; using its default", section, key);
                return {};
            }
            return buffer;
        }

        std::string ReadText(const char* section, const char* key) {
            const auto text = ReadSetting(section, key);
            return text ? *text : std::string(DefaultValue(section, key));
        }

        std::int32_t ReadInteger(const char* section, const char* key, std::int32_t minimum, std::int32_t maximum) {
            const auto fallback =
                settings_rules::ParseInteger(DefaultValue(section, key), minimum, maximum).value_or(0);
            const auto text = ReadSetting(section, key);
            if (!text)
                return fallback;
            if (const auto value = settings_rules::ParseInteger(*text, minimum, maximum))
                return *value;
            logger::warn("Invalid INI [{}] {}='{}'; using default {} without modifying the file", section, key, *text,
                         fallback);
            return fallback;
        }

        bool ReadBoolean(const char* section, const char* key) {
            return ReadInteger(section, key, 0, 1) == 1;
        }

        void CreateMissingIni() {
            const auto creation = settings_file::CreateMissing(kIniPath, settings_defaults::Ini);
            if (creation.result == settings_file::Result::Created)
                logger::info("Created default settings at {}", kIniPath);
            else if (creation.result == settings_file::Result::Failed)
                logger::warn("Could not create settings at {}: {}. Using available settings and built-in defaults",
                             kIniPath, creation.error.message());
        }

        std::string StripQuotes(std::string text) {
            std::erase(text, '"');
            return std::string(settings_rules::Trim(text));
        }

        std::optional<PerkSpec> ParsePerkSpec(const std::string& input) {
            const auto text = StripQuotes(input);
            auto separator = text.find('|');
            if (separator == text.npos)
                separator = text.find(':');
            if (separator == text.npos)
                return {};

            const auto file = StripQuotes(text.substr(0, separator));
            const auto formID = settings_rules::ParseFormID(StripQuotes(text.substr(separator + 1)));
            if (file.empty() || !formID)
                return {};
            return PerkSpec{file, *formID};
        }
    }

    void ParsePerkListIntoSpecs(const std::string& raw) {
        std::scoped_lock lock(Mutex);
        PerkSpecCount = 0;
        const auto s = StripQuotes(std::string(settings_rules::WithoutComment(raw)));
        if (s.empty())
            return;

        std::size_t start = 0;
        while (start < s.size() && PerkSpecCount < kMaxPerkSpecs) {
            std::size_t comma = s.find(',', start);
            if (comma == std::string::npos)
                comma = s.size();

            std::string token = StripQuotes(s.substr(start, comma - start));
            if (!token.empty()) {
                if (auto spec = ParsePerkSpec(token)) {
                    PerkSpecs[PerkSpecCount++] = std::move(*spec);
                } else
                    logger::warn("Invalid sPerkForms entry '{}'; ignored", token);
            }
            start = comma + 1;
        }
    }

    void BuildPerkListBuffer() {
        std::scoped_lock lock(Mutex);
        std::string result;
        for (std::size_t i = 0; i < PerkSpecCount; ++i) {
            const auto& spec = PerkSpecs[i];
            if (!result.empty())
                result += ",";
            result += fmt::format("{}|{:08X}", spec.file, spec.localID);
        }
        const auto length = std::min(result.size(), sizeof(PerkListBuffer) - 1);
        std::copy_n(result.data(), length, PerkListBuffer);
        PerkListBuffer[length] = '\0';
    }

    void Load(bool force) {
        std::scoped_lock lock(Mutex);
        if (Loaded && !force)
            return;
        Loaded = true;
        if (force)
            SaveStatus.clear();

        CreateMissingIni();
        MaxExtraFollowers = ReadInteger("General", "iMaxFollowers", 1, 8) - 1;
        FollowerPerkOption = ReadInteger("General", "bFollowerOptionSelector", 0, 2);
        SpeechLevelsPerSlot =
            ReadInteger("General", "iSpeechLevelsPerSlot", 1, std::numeric_limits<std::int32_t>::max());

        ParsePerkListIntoSpecs(ReadText("General", "sPerkForms"));
        BuildPerkListBuffer();

        FollowerEssential = ReadBoolean("General", "bFollowerEssential");
        FriendlyFire = ReadBoolean("General", "bFriendlyFireProtection");
        FollowerSandbox = ReadBoolean("General", "bFollowerSandbox");
        FollowerCrossfire = ReadBoolean("General", "bFollowerCrossfireProtection");
        FollowerHomes = ReadBoolean("General", "bFollowerHomes");
        FollowDistance = ReadInteger("General", "iFollowDistance", 0, 2);
        mod::debug::OptionsEnabled = ReadBoolean("Debug", "bEnabled");
        mod::debug::SetLogging(ReadBoolean("Debug", "bFlowLogging"));
        mod::debug::Trace("Settings.Load", nullptr, force ? "forced" : "initial");
    }

    bool Save() {
        std::scoped_lock lock(Mutex);
        mod::debug::Trace("Settings.Save");
        std::string failedKeys;
        auto write = [&](const char* section, const char* key, const char* value) {
            SetLastError(ERROR_SUCCESS);
            if (WritePrivateProfileStringA(section, key, value, kIniPath))
                return;
            const auto error = GetLastError();
            if (!failedKeys.empty())
                failedKeys += ", ";
            failedKeys += key;
            logger::error("Could not save INI setting [{}] {} to {} (Windows error {})", section, key, kIniPath, error);
        };
        auto writeInt = [&](const char* key, int val) { write("General", key, std::to_string(val).c_str()); };

        writeInt("iMaxFollowers", MaxExtraFollowers + 1);
        writeInt("bFollowerOptionSelector", FollowerPerkOption);
        writeInt("iSpeechLevelsPerSlot", SpeechLevelsPerSlot);
        writeInt("bFollowerEssential", FollowerEssential ? 1 : 0);
        writeInt("bFriendlyFireProtection", FriendlyFire ? 1 : 0);
        writeInt("bFollowerSandbox", FollowerSandbox ? 1 : 0);
        writeInt("bFollowerCrossfireProtection", FollowerCrossfire ? 1 : 0);
        writeInt("bFollowerHomes", FollowerHomes ? 1 : 0);
        writeInt("iFollowDistance", FollowDistance);
        ParsePerkListIntoSpecs(PerkListBuffer);
        BuildPerkListBuffer();
        write("General", "sPerkForms", PerkListBuffer);
        write("Debug", "bFlowLogging", mod::debug::Logging.load() ? "1" : "0");
        write("Debug", "bEnabled", mod::debug::OptionsEnabled.load() ? "1" : "0");
        const bool saved = failedKeys.empty();
        SaveStatus = saved ? fmt::format("Settings saved to {}.", kIniPath)
                           : fmt::format(
                                 "Could not save all settings to {}. Failed keys: {}. Some values may have been "
                                 "written; see the log.",
                                 kIniPath, failedKeys);
        if (saved)
            logger::info("{}", SaveStatus);
        spdlog::default_logger()->flush();
        return saved;
    }
}
