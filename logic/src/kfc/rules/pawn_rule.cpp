#include "../../../include/kfc/rules/pawn_rule.hpp"

namespace kfc::model {

std::vector<Position> PawnRule::legal_destinations(const Board& board, const Piece& piece) const {
    int forward_step = (piece.color == PieceColor::White) ? -1 : 1;
    std::vector<Position> destinations;

    Position forward{piece.cell.row + forward_step, piece.cell.col};
    bool forward_is_clear = board.in_bounds(forward) && !board.piece_at(forward).has_value();
    if (forward_is_clear) {
        destinations.push_back(forward);

        if (!piece.has_moved) {
            Position double_forward{piece.cell.row + 2 * forward_step, piece.cell.col};
            if (board.in_bounds(double_forward) && !board.piece_at(double_forward).has_value()) {
                destinations.push_back(double_forward);
            }
        }
    }

    for (int col_offset : {-1, 1}) {
        Position diagonal{piece.cell.row + forward_step, piece.cell.col + col_offset};
        if (!board.in_bounds(diagonal)) {
            continue;
        }
        std::optional<Piece> occupant = board.piece_at(diagonal);
        if (occupant.has_value() && occupant->color != piece.color) {
            destinations.push_back(diagonal);
        }
    }

    return destinations;
}

}  // namespace kfc::model
