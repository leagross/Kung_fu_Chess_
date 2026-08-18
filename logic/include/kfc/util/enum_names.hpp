#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>

namespace kfc::util {

/// One enum's enumerators paired with their written names, in one constexpr
/// table, with both conversions derived from it. Linear lookup, but table
/// sizes here are two to seven entries, and staying constexpr lets
/// covers_through turn "every enumerator has a name" into a static_assert.
template <typename Enum, std::size_t N>
struct EnumNames {
    std::array<std::pair<Enum, std::string_view>, N> entries;

    /// Empty view if the table has no entry for value.
    [[nodiscard]] constexpr std::string_view name_of(Enum value) const {
        for (const auto& [candidate, name] : entries) {
            if (candidate == value) {
                return name;
            }
        }
        return {};
    }

    /// The enumerator that name spells, or std::nullopt if it spells none of them.
    [[nodiscard]] constexpr std::optional<Enum> value_of(std::string_view name) const {
        for (const auto& [value, candidate] : entries) {
            if (candidate == name) {
                return value;
            }
        }
        return std::nullopt;
    }

    /// True when the table holds exactly one entry per enumerator of a
    /// zero-based, gapless enum ending at last.
    [[nodiscard]] constexpr bool covers_through(Enum last) const {
        return N == static_cast<std::size_t>(last) + 1;
    }
};

}  // namespace kfc::util
