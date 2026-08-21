#pragma once

#include "../../kfc/model/board.hpp"
#include "../../kfc/model/piece.hpp"

namespace kfc::model {

/// A Pawn landing on row 0 (White) or the last row (Black) becomes a Queen
/// -- a consequence of arrival, not of move legality (not PawnRule's
/// concern). Returns true if it promoted, for ArrivalEvent::was_promotion.
inline bool apply_pawn_promotion(Piece& arrived, const Board& board) {
    if (arrived.kind != PieceKind::Pawn) {
        return false;
    }
    int promotion_row = (arrived.color == PieceColor::White) ? 0 : board.height() - 1;
    if (arrived.cell.row == promotion_row) {
        arrived.kind = PieceKind::Queen;
        return true;
    }
    return false;
}

}  // namespace kfc::model
