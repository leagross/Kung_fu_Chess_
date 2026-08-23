#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>

namespace kfc::util {

/// One enum's enumerators paired with written names; stays constexpr so covers_through can static_assert.
template <typename Enum, std::size_t N>
struct EnumNames {
    std::array<std::pair<Enum, std::string_view>, N> entries;

    [[nodiscard]] constexpr std::string_view name_of(Enum value) const {
        for (const auto& [candidate, name] : entries) {
            if (candidate == value) {
                return name;
            }
        }
        return {};
    }

    [[nodiscard]] constexpr std::optional<Enum> value_of(std::string_view name) const {
        for (const auto& [value, candidate] : entries) {
            if (candidate == name) {
                return value;
            }
        }
        return std::nullopt;
    }

    /// True when the table holds one entry per enumerator of a zero-based, gapless enum ending at last.
    [[nodiscard]] constexpr bool covers_through(Enum last) const {
        return N == static_cast<std::size_t>(last) + 1;
    }
};

}  // namespace kfc::util
