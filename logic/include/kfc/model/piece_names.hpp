#pragma once

#include <optional>
#include <string_view>

#include "kfc/model/piece.hpp"
#include "kfc/util/enum_names.hpp"

namespace kfc::model {

/// Full words, not kfc::io's notation letters; static_asserts below keep each table complete.
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

[[nodiscard]] constexpr std::string_view name_of(PieceKind kind) {
    return kPieceKindNames.name_of(kind);
}

[[nodiscard]] constexpr std::string_view name_of(PieceColor color) {
    return kPieceColorNames.name_of(color);
}

[[nodiscard]] constexpr std::string_view name_of(PieceState state) {
    return kPieceStateNames.name_of(state);
}

/// Named per enum rather than overloaded, since the argument alone can't say which enum is meant.
[[nodiscard]] constexpr std::optional<PieceKind> piece_kind_from_name(std::string_view name) {
    return kPieceKindNames.value_of(name);
}

[[nodiscard]] constexpr std::optional<PieceColor> piece_color_from_name(std::string_view name) {
    return kPieceColorNames.value_of(name);
}

[[nodiscard]] constexpr std::optional<PieceState> piece_state_from_name(std::string_view name) {
    return kPieceStateNames.value_of(name);
}

// Each check names the enum's last member -- keep it last.
static_assert(kPieceKindNames.covers_through(PieceKind::Drone), "every PieceKind needs a name");
static_assert(kPieceColorNames.covers_through(PieceColor::Black), "every PieceColor needs a name");
static_assert(kPieceStateNames.covers_through(PieceState::Captured), "every PieceState needs a name");
static_assert(name_of(PieceKind::Drone) == "Drone", "the table must line up with the enum");
static_assert(piece_state_from_name("Airborne") == PieceState::Airborne, "reading must invert writing");

}  // namespace kfc::model
