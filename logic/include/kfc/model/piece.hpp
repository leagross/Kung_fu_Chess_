#pragma once

#include <cstddef>
#include <functional>

#include "../../kfc/model/position.hpp"

namespace kfc::model {

/// Stable identity for a single piece, independent of where it currently sits
/// on the board. Wrapped instead of a raw int so an id can never be silently
/// confused with an unrelated integer.
struct PieceId {
    int value;
};

/// True when both ids carry the same underlying value.
inline bool operator==(const PieceId& lhs, const PieceId& rhs) {
    return lhs.value == rhs.value;
}

/// True when the ids differ.
inline bool operator!=(const PieceId& lhs, const PieceId& rhs) {
    return !(lhs == rhs);
}

/// Which side a piece belongs to.
enum class PieceColor {
    White,
    Black,
};

/// The other side. Used wherever "whoever didn't just do X" needs a value --
/// the winner of a forfeit or a resign, for instance -- rather than every
/// caller re-deriving it with its own White/Black ternary.
[[nodiscard]] constexpr PieceColor opposite_of(PieceColor color) {
    return color == PieceColor::White ? PieceColor::Black : PieceColor::White;
}

/// The chess role a piece plays. Extending this list (e.g. adding a custom
/// piece type) is the one place in the model layer that must be touched by
/// hand; movement behavior for the new kind belongs in a PieceRules strategy,
/// not here.
enum class PieceKind {
    King,
    Queen,
    Rook,
    Bishop,
    Knight,
    Pawn,
    Drone,
};

/// Lifecycle flag only -- never a destination, path, speed, or elapsed time.
/// Those belong to Motion and RealTimeArbiter, not to the piece itself.
enum class PieceState {
    Idle,
    Moving,
    /// Mid-flight on a JumpInPlace. Distinct from Moving so CollisionResolver
    /// can tell "an ordinary move is in progress" apart from "this piece is
    /// in the air and not really occupying its cell" -- an arriving mover
    /// passes through an Airborne occupant instead of capturing it.
    Airborne,
    Captured,
};

/// A single chess piece: identity, side, role, current cell, lifecycle
/// state, and whether it has ever completed a real move. Knows nothing
/// about rendering, mouse input, or movement rules.
///
/// has_moved exists so a pawn's two-cell opening move can be board-size
/// independent: "has this piece ever moved" instead of "is it sitting on a
/// row computed from board height", which breaks down on the small ad-hoc
/// boards this project's fixtures use (there is no single row offset that
/// means "start row" for every board size). RealTimeArbiter sets this to
/// true on arrival for an ordinary move, never for a jump-in-place (the
/// piece never left its cell).
struct Piece {
    PieceId id;
    PieceColor color;
    PieceKind kind;
    Position cell;
    PieceState state;
    bool has_moved = false;
};

}  // namespace kfc::model

namespace std {

/// Lets PieceId be used as an unordered_map/unordered_set key directly,
/// instead of every caller unwrapping .value first.
template <>
struct hash<kfc::model::PieceId> {
    std::size_t operator()(const kfc::model::PieceId& id) const noexcept {
        return std::hash<int>{}(id.value);
    }
};

}  // namespace std
