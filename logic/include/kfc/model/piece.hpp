#pragma once

#include <cstddef>
#include <functional>

#include "../../kfc/model/position.hpp"

namespace kfc::model {

/// Stable piece identity, independent of board position; wrapped so it's never confused with a raw int.
struct PieceId {
    int value;
};

inline bool operator==(const PieceId& lhs, const PieceId& rhs) {
    return lhs.value == rhs.value;
}

inline bool operator!=(const PieceId& lhs, const PieceId& rhs) {
    return !(lhs == rhs);
}

enum class PieceColor {
    White,
    Black,
};

[[nodiscard]] constexpr PieceColor opposite_of(PieceColor color) {
    return color == PieceColor::White ? PieceColor::Black : PieceColor::White;
}

enum class PieceKind {
    King,
    Queen,
    Rook,
    Bishop,
    Knight,
    Pawn,
    Drone,
};

/// Lifecycle flag only -- destination/path/speed/elapsed time belong to Motion and RealTimeArbiter.
enum class PieceState {
    Idle,
    Moving,
    Airborne,  // mid-jump; an arriving mover passes through instead of capturing
    Captured,
};

/// has_moved tracks "has this piece ever moved" so pawn double-steps stay valid on any board size.
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

/// Lets PieceId be used directly as an unordered_map/unordered_set key.
template <>
struct hash<kfc::model::PieceId> {
    std::size_t operator()(const kfc::model::PieceId& id) const noexcept {
        return std::hash<int>{}(id.value);
    }
};

}  // namespace std
