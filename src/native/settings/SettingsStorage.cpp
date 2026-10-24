#include "SettingsStorage.h"
#include "Settings.h"

namespace mod::settings::storage {
    namespace {
        struct Record {
            std::uint32_t version;
            std::vector<std::uint8_t> bytes;
        };

        std::vector<Record> records;
        auto& warning = GameplayWarning;
    }

    void Reset() {
        std::scoped_lock lock(Mutex);
        records.clear();
        warning.clear();
    }

    void Read(SKSE::SerializationInterface* serialization, std::uint32_t version, std::uint32_t length) {
        if (length > MaxGameplayBytes || records.size() >= 2) {
            warning =
                "Gameplay settings records exceed supported limits. Using INI defaults; settings cannot be saved.";
            return;
        }
        Record record{version, std::vector<std::uint8_t>(length)};
        if (serialization->ReadRecordData(record.bytes.data(), length) != length) {
            warning = "Gameplay settings are truncated. Using INI defaults; settings cannot be saved.";
            return;
        }
        records.push_back(std::move(record));
    }

    void Loaded() {
        if (warning.empty() && !records.empty()) {
            if (records.size() != 1 || records.front().version != GameplayVersion) {
                warning = "Duplicate or unsupported gameplay settings. Using INI defaults; original records retained.";
            } else if (const auto value = DecodeGameplay(records.front().bytes)) {
                ApplyGameplay(*value);
            } else {
                warning = "Invalid gameplay settings. Using INI defaults; original record retained.";
            }
        }
        if (!warning.empty())
            logger::warn("{}", warning);
    }

    void Save(SKSE::SerializationInterface* serialization) {
        const auto write = [&](std::uint32_t version, const std::vector<std::uint8_t>& bytes) {
            return serialization->WriteRecord(GameplayRecord, version, bytes.data(),
                                              static_cast<std::uint32_t>(bytes.size()));
        };
        if (!warning.empty()) {
            for (const auto& record : records)
                if (!write(record.version, record.bytes))
                    logger::error("Could not preserve gameplay settings record");
            return;
        }
        const auto bytes = EncodeGameplay(CaptureGameplay());
        if (bytes.empty() || !write(GameplayVersion, bytes))
            logger::error("Could not save gameplay settings");
    }

}
