#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>

namespace kfc::util {

/// One enum's enumerators paired with their written names, in one table, with
/// both conversions derived from it.
///
/// The pattern this replaces was a switch to write plus an if-chain to read,
/// repeated once per enum and sometimes more than once per enum. That shape has
/// three costs, in rising order of how much they hurt:
///
///  - it allocates: each branch returned a `std::string`, built from a literal,
///    for a value that is never modified and outlives every caller;
///  - it compares whole strings one at a time on the read side, so an unknown
///    name pays for every entry before failing;
///  - and it splits one fact across two functions, so adding an enumerator can
///    leave the read side silently unable to parse what the write side emits.
///    The switch at least warns; the if-chain never does.
///
/// Here the pairing exists once, as constexpr data. The names are
/// `std::string_view`s onto string literals -- no allocation, no static
/// initialisation order, nothing to free. Lookups are linear, which for enums
/// of this size (two to seven entries) beats a hash map that would have to
/// build itself at startup and chase a pointer to answer.
///
/// Linear also means the table stays usable at compile time: covers_through
/// below turns "every enumerator has a name" into a static_assert, so the
/// missing-name bug becomes a build error instead of a runtime throw a user
/// finds for you.
template <typename Enum, std::size_t N>
struct EnumNames {
    std::array<std::pair<Enum, std::string_view>, N> entries;

    /// The written name of value, or an empty view if the table has no entry
    /// for it -- which covers_through is there to make impossible.
    [[nodiscard]] constexpr std::string_view name_of(Enum value) const {
        for (const auto& [candidate, name] : entries) {
            if (candidate == value) {
                return name;
            }
        }
        return {};
    }

    /// The enumerator that name spells, or std::nullopt if it spells none of
    /// them -- which is what an unknown value in a config file or an untrusted
    /// message is. Takes a view so callers can pass a literal, a std::string or
    /// a parsed span without copying one into existence just to compare it.
    [[nodiscard]] constexpr std::optional<Enum> value_of(std::string_view name) const {
        for (const auto& [value, candidate] : entries) {
            if (candidate == name) {
                return value;
            }
        }
        return std::nullopt;
    }

    /// True when the table holds exactly one entry per enumerator of a
    /// zero-based, gapless enum ending at last. Meant for a static_assert next
    /// to the table: adding an enumerator without adding its name then fails
    /// the build rather than surfacing as an unreadable message at runtime.
    [[nodiscard]] constexpr bool covers_through(Enum last) const {
        return N == static_cast<std::size_t>(last) + 1;
    }
};

}  // namespace kfc::util
