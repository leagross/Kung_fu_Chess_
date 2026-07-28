#pragma once

#include <array>
#include <optional>
#include <string>

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

/// Every PieceStateName, for code that must process all five (e.g.
/// AnimationConfigLoader reading every states/<name>/ subfolder). Extending
/// this list is the one place that must be touched by hand if a pack ever
/// defines a sixth state -- the same trade-off kfc::model::PieceKind makes.
inline constexpr std::array<PieceStateName, 5> kAllPieceStateNames = {
    PieceStateName::Idle, PieceStateName::Move, PieceStateName::Jump, PieceStateName::ShortRest,
    PieceStateName::LongRest,
};

/// The folder name (under a piece's states/ directory) for state, e.g.
/// "short_rest" for PieceStateName::ShortRest. The exact inverse of
/// parse_piece_state_name.
std::string piece_state_name_folder(PieceStateName state);

/// Parses a folder/config name like "idle" or "short_rest" into its
/// PieceStateName. Returns std::nullopt for anything unrecognized -- e.g. if
/// a config.json's next_state_when_finished names a state this build
/// doesn't know about, callers decide how to react rather than silently
/// guessing.
std::optional<PieceStateName> parse_piece_state_name(const std::string& name);

}  // namespace kfc::graphics
