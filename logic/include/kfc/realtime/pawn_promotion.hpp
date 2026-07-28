#pragma once

#include "../../kfc/model/board.hpp"
#include "../../kfc/model/piece.hpp"

namespace kfc::model {

/// A Pawn that lands on row 0 (White) or the board's last row (Black)
/// becomes a Queen -- pure, board-shape-aware logic pulled out of
/// RealTimeArbiter::resolve_arrival, which otherwise mixes scheduling,
/// collision handling, board mutation, cooldown bookkeeping, and this. Not
/// PawnRule's concern: promotion is a consequence of arrival, not of move
/// legality.
/// Returns true if a promotion actually happened, so the caller can record
/// it on the ArrivalEvent (see ArrivalEvent::was_promotion) -- the move log
/// needs to know, since after this runs arrived.kind no longer reveals that
/// the piece was a pawn a moment ago.
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
