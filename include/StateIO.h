#pragma once

#include <bit>
#include <cstdint>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace mod::controller::storage {
    // Explicit little-endian wire format, bounded before allocating any data.
    class StateWriter {
    public:
        std::vector<std::uint8_t> bytes;

        template <class T>
        void Number(T value) {
            using U = std::conditional_t<sizeof(T) == 8, std::uint64_t, std::uint32_t>;
            U bits;
            if constexpr (std::is_same_v<T, float>)
                bits = std::bit_cast<std::uint32_t>(value);
            else
                bits = static_cast<U>(value);
            for (std::size_t i = 0; i < sizeof(T); ++i)
                bytes.push_back(static_cast<std::uint8_t>(bits >> (i * 8)));
        }

        void Text(const std::string& value) {
            Number(static_cast<std::uint32_t>(value.size()));
            bytes.insert(bytes.end(), value.begin(), value.end());
        }

        template <class T>
        void Array(const std::vector<T>& values) {
            Number(static_cast<std::uint32_t>(values.size()));
            for (auto value : values)
                Number(value);
        }
    };

    class StateReader {
        std::span<const std::uint8_t> bytes;
        std::size_t position = 0;

    public:
        explicit StateReader(std::span<const std::uint8_t> data) : bytes(data) {}

        template <class T>
        bool Number(T& value) {
            if (bytes.size() - position < sizeof(T))
                return false;
            using U = std::conditional_t<sizeof(T) == 8, std::uint64_t, std::uint32_t>;
            U bits = 0;
            for (std::size_t i = 0; i < sizeof(T); ++i)
                bits |= static_cast<U>(bytes[position++]) << (i * 8);
            if constexpr (std::is_same_v<T, float>)
                value = std::bit_cast<float>(static_cast<std::uint32_t>(bits));
            else if constexpr (std::is_signed_v<T>)
                value = std::bit_cast<T>(bits);
            else
                value = static_cast<T>(bits);
            return true;
        }

        bool Boolean(bool& value) {
            std::uint32_t bits;
            if (!Number(bits) || bits > 1)
                return false;
            value = bits != 0;
            return true;
        }

        bool Text(std::string& value, std::size_t maximum) {
            std::uint32_t length;
            if (!Number(length) || length > maximum || length > bytes.size() - position)
                return false;
            value.assign(reinterpret_cast<const char*>(bytes.data() + position), length);
            position += length;
            return true;
        }

        template <class T>
        bool Array(std::vector<T>& values, std::size_t maximum) {
            std::uint32_t count;
            if (!Number(count) || count > maximum || count > (bytes.size() - position) / sizeof(T))
                return false;
            values.resize(count);
            for (auto& value : values)
                if (!Number(value))
                    return false;
            return true;
        }

        bool Done() const {
            return position == bytes.size();
        }
    };
}
