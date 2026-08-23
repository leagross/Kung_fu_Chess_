#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "../../kfc/model/piece.hpp"
#include "../../kfc/model/position.hpp"

namespace kfc::model {

/// Rectangular arrangement of pieces; knows nothing about move legality (RuleEngine's job).
class Board {
public:
    Board(int width, int height);

    int width() const;
    int height() const;

    /// True when pos falls within [0, width) x [0, height).
    bool in_bounds(const Position& pos) const;

    /// Throws std::out_of_range if off-board, std::logic_error if already occupied.
    void add_piece(const Piece& piece);

    void remove_piece(const Position& pos);

    /// Assumes the caller already validated the move; no legality/occupancy checks here.
    void move_piece(const Position& from, const Position& to);

    std::optional<Piece> piece_at(const Position& pos) const;

    /// Leaves identity, color, kind, and cell untouched. Assumes pos is in bounds and occupied.
    void set_piece_state(const Position& pos, PieceState state);

private:
    std::size_t index(const Position& pos) const;

    int width_;
    int height_;
    std::vector<std::optional<Piece>> cells_;
};

}  // namespace kfc::model
