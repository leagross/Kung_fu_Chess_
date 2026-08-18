#pragma once

#include <optional>
#include <string_view>

#include "kfc/util/enum_names.hpp"

namespace kfc::graphics {

/// Presentation states, distinct from kfc::model::PieceState's game-logic
/// lifecycle flag (e.g. Move and Jump are both "Moving" to the engine but
/// animate differently).
enum class PieceStateName {
    Idle,
    Move,
    Jump,
    ShortRest,
    LongRest,
};

/// Each state paired with its folder name under a piece's states/ directory;
/// the single source for iterating, writing, and parsing state names.
inline constexpr kfc::util::EnumNames<PieceStateName, 5> kPieceStateFolders{{{
    {PieceStateName::Idle, "idle"},
    {PieceStateName::Move, "move"},
    {PieceStateName::Jump, "jump"},
    {PieceStateName::ShortRest, "short_rest"},
    {PieceStateName::LongRest, "long_rest"},
}}};

/// Folder name for state, e.g. "short_rest" for ShortRest.
[[nodiscard]] constexpr std::string_view piece_state_name_folder(PieceStateName state) {
    return kPieceStateFolders.name_of(state);
}

/// Inverse of piece_state_name_folder; std::nullopt if unrecognized.
[[nodiscard]] constexpr std::optional<PieceStateName> parse_piece_state_name(std::string_view name) {
    return kPieceStateFolders.value_of(name);
}

// Forgetting to add a folder for a new state is now a build error.
static_assert(kPieceStateFolders.covers_through(PieceStateName::LongRest),
              "every PieceStateName needs a folder name");

}  // namespace kfc::graphics
