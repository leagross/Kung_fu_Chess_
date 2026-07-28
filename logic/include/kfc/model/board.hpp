#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "../../kfc/model/piece.hpp"
#include "../../kfc/model/position.hpp"

namespace kfc::model {

/// Owns the logical arrangement of pieces on a rectangular board. Knows what
/// exists and where; knows nothing about which chess moves are legal -- that
/// is RuleEngine's job. All access to the pieces goes through this class;
/// nothing outside Board can reach the underlying storage directly.
class Board {
public:
    /// Creates an empty board of the given size. Every cell starts unoccupied.
    Board(int width, int height);

    /// Number of columns.
    int width() const;

    /// Number of rows.
    int height() const;

    /// True when pos falls within [0, width) x [0, height).
    bool in_bounds(const Position& pos) const;

    /// Places piece at piece.cell. Throws std::out_of_range if the cell is
    /// outside the board, or std::logic_error if the cell is already occupied.
    void add_piece(const Piece& piece);

    /// Clears whatever occupies pos, if anything. No-op on an already-empty cell.
    void remove_piece(const Position& pos);

    /// Relocates the piece at from to to, overwriting whatever was at to.
    /// Assumes the caller already validated the move; performs no legality
    /// or occupancy checks itself.
    void move_piece(const Position& from, const Position& to);

    /// The piece at pos, or std::nullopt if the cell is empty or out of bounds.
    std::optional<Piece> piece_at(const Position& pos) const;

    /// Updates the lifecycle state (Piece::state) of whatever occupies pos,
    /// leaving identity, color, kind, and cell untouched. Assumes pos is
    /// in bounds and occupied -- callers (RealTimeArbiter) already know
    /// this from the Motion they are processing.
    void set_piece_state(const Position& pos, PieceState state);

private:
    // Converts a board-relative Position into a flat index into cells_.
    // Caller must have already confirmed the position is in bounds.
    std::size_t index(const Position& pos) const;

    int width_;
    int height_;
    std::vector<std::optional<Piece>> cells_;
};

}  // namespace kfc::model
