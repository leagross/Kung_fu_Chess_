#pragma once

#include <array>
#include <optional>
#include <string_view>
#include <utility>

#include "kfc/model/piece.hpp"

namespace kfc::model {

/// The one place a PieceKind and its written name are paired.
///
/// This mapping used to exist three separate times -- twice in the JSON codec
/// (one switch to write, one if-chain to read) and again in the gameplay-config
/// loader. Three copies of the same seven-way correspondence is three places to
/// forget when a piece is added, and the compiler cannot tell you that you did:
/// a missing case in one of them is a runtime "Unknown piece kind", found by a
/// user rather than a build.
///
/// Ordered to match the enum, so the table and the type are read together.
inline constexpr std::array<std::pair<PieceKind, std::string_view>, 7> kPieceKindNames{{
    {PieceKind::King, "King"},
    {PieceKind::Queen, "Queen"},
    {PieceKind::Rook, "Rook"},
    {PieceKind::Bishop, "Bishop"},
    {PieceKind::Knight, "Knight"},
    {PieceKind::Pawn, "Pawn"},
    {PieceKind::Drone, "Drone"},
}};

/// The written name of a kind. Total: every enumerator is in the table, and a
/// static_assert below keeps it that way.
[[nodiscard]] constexpr std::string_view name_of(PieceKind kind) {
    for (const auto& [candidate, name] : kPieceKindNames) {
        if (candidate == kind) {
            return name;
        }
    }
    return {};
}

/// The kind that name spells, or std::nullopt if it spells none of them --
/// which is what an unknown kind in a config file or an untrusted message is.
/// Takes a string_view so callers pass a literal, a std::string or a parsed
/// span without copying one into existence just to compare it.
[[nodiscard]] constexpr std::optional<PieceKind> piece_kind_from_name(std::string_view name) {
    for (const auto& [kind, candidate] : kPieceKindNames) {
        if (candidate == name) {
            return kind;
        }
    }
    return std::nullopt;
}

// Adding an enumerator without adding its name is a build error, not a runtime
// surprise. Drone is deliberately the last one -- keep it so.
static_assert(kPieceKindNames.size() == static_cast<std::size_t>(PieceKind::Drone) + 1,
              "every PieceKind needs a name in kPieceKindNames");
static_assert(name_of(PieceKind::Drone) == "Drone", "the table must line up with the enum");

}  // namespace kfc::model
