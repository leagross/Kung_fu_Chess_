#pragma once

#include <optional>
#include <string_view>

#include "kfc/util/enum_names.hpp"

namespace kfc::graphics {

/// The five animation states every asset pack's piece defines (see
/// pieces_mine/*/states/). Deliberately separate from kfc::model::PieceState
/// (Idle/Moving/Captured) -- that enum is a lifecycle flag the engine uses
/// for game logic; this one is a presentation state machine with its own,
/// richer set of states (e.g. Move and Jump are both "Moving" to the engine,
/// but animate completely differently).
enum class PieceStateName {
    Idle,
    Move,
    Jump,
    ShortRest,
    LongRest,
};

/// Each state paired with its folder name under a piece's states/ directory.
///
/// This is the whole definition of the set: the list to iterate, the name to
/// write, and the name to parse, all from one table. It used to be three
/// separate things -- a hand-written array of the five values, a
/// std::unordered_map<PieceStateName, std::string> built at static
/// initialisation, and a linear scan of that map to invert it. The map cost a
/// heap allocation per entry before main() ran and another on every lookup
/// (the accessor returned std::string by value, and AnimationConfigLoader
/// calls it once per state per piece); the array cost a second place to
/// remember when a sixth state is added. Extending the set is now one line
/// here, and covers_through below makes forgetting it a build error.
inline constexpr kfc::util::EnumNames<PieceStateName, 5> kPieceStateFolders{{{
    {PieceStateName::Idle, "idle"},
    {PieceStateName::Move, "move"},
    {PieceStateName::Jump, "jump"},
    {PieceStateName::ShortRest, "short_rest"},
    {PieceStateName::LongRest, "long_rest"},
}}};

/// The folder name (under a piece's states/ directory) for state, e.g.
/// "short_rest" for PieceStateName::ShortRest. The exact inverse of
/// parse_piece_state_name. The view refers to a string literal with static
/// storage, so it stays valid for as long as the caller needs it.
[[nodiscard]] constexpr std::string_view piece_state_name_folder(PieceStateName state) {
    return kPieceStateFolders.name_of(state);
}

/// Parses a folder/config name like "idle" or "short_rest" into its
/// PieceStateName. Returns std::nullopt for anything unrecognized -- e.g. if
/// a config.json's next_state_when_finished names a state this build
/// doesn't know about, callers decide how to react rather than silently
/// guessing.
[[nodiscard]] constexpr std::optional<PieceStateName> parse_piece_state_name(std::string_view name) {
    return kPieceStateFolders.value_of(name);
}

// Adding a state without giving it a folder is a build error, not a missing
// animation discovered at runtime. LongRest is deliberately last -- keep it so.
static_assert(kPieceStateFolders.covers_through(PieceStateName::LongRest),
              "every PieceStateName needs a folder name");

}  // namespace kfc::graphics
