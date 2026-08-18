#pragma once

#include <optional>
#include <string_view>

#include "kfc/model/piece.hpp"
#include "kfc/util/enum_names.hpp"

namespace kfc::model {

/// The one place a piece enum and its written name are paired. Each table is
/// ordered to match its enum and followed by a static_assert that it still
/// covers the whole enum, so a missing branch is a build error, not a runtime
/// "Unknown PieceKind". These are readable full words, not the compact
/// chess-notation letters kfc::io uses.

inline constexpr kfc::util::EnumNames<PieceKind, 7> kPieceKindNames{{{
    {PieceKind::King, "King"},
    {PieceKind::Queen, "Queen"},
    {PieceKind::Rook, "Rook"},
    {PieceKind::Bishop, "Bishop"},
    {PieceKind::Knight, "Knight"},
    {PieceKind::Pawn, "Pawn"},
    {PieceKind::Drone, "Drone"},
}}};

inline constexpr kfc::util::EnumNames<PieceColor, 2> kPieceColorNames{{{
    {PieceColor::White, "White"},
    {PieceColor::Black, "Black"},
}}};

inline constexpr kfc::util::EnumNames<PieceState, 4> kPieceStateNames{{{
    {PieceState::Idle, "Idle"},
    {PieceState::Moving, "Moving"},
    {PieceState::Airborne, "Airborne"},
    {PieceState::Captured, "Captured"},
}}};

/// The written name of a kind/colour/state. Total: every enumerator is in its
/// table, and the static_asserts below keep it that way. Overloaded on the enum
/// so call sites read the same whichever one they hold.
[[nodiscard]] constexpr std::string_view name_of(PieceKind kind) {
    return kPieceKindNames.name_of(kind);
}

[[nodiscard]] constexpr std::string_view name_of(PieceColor color) {
    return kPieceColorNames.name_of(color);
}

[[nodiscard]] constexpr std::string_view name_of(PieceState state) {
    return kPieceStateNames.name_of(state);
}

/// The value that name spells, or std::nullopt if it spells none of them.
/// Named per enum rather than overloaded, because the argument alone cannot say
/// which one the caller meant.
[[nodiscard]] constexpr std::optional<PieceKind> piece_kind_from_name(std::string_view name) {
    return kPieceKindNames.value_of(name);
}

[[nodiscard]] constexpr std::optional<PieceColor> piece_color_from_name(std::string_view name) {
    return kPieceColorNames.value_of(name);
}

[[nodiscard]] constexpr std::optional<PieceState> piece_state_from_name(std::string_view name) {
    return kPieceStateNames.value_of(name);
}

// Adding an enumerator without adding its name is a build error, not a runtime
// surprise. Each check names the enum's last member -- keep it last.
static_assert(kPieceKindNames.covers_through(PieceKind::Drone), "every PieceKind needs a name");
static_assert(kPieceColorNames.covers_through(PieceColor::Black), "every PieceColor needs a name");
static_assert(kPieceStateNames.covers_through(PieceState::Captured), "every PieceState needs a name");
static_assert(name_of(PieceKind::Drone) == "Drone", "the table must line up with the enum");
static_assert(piece_state_from_name("Airborne") == PieceState::Airborne, "reading must invert writing");

}  // namespace kfc::model
