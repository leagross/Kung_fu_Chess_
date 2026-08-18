#pragma once

#include "../../kfc/model/board.hpp"
#include "../../kfc/model/piece.hpp"

namespace kfc::model {

/// A Pawn that lands on row 0 (White) or the board's last row (Black)
/// becomes a Queen. Not PawnRule's concern: promotion is a consequence of
/// arrival, not of move legality. Returns true if promotion happened, so the
/// caller can record it on ArrivalEvent::was_promotion before arrived.kind
/// stops revealing the piece was ever a pawn.
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
