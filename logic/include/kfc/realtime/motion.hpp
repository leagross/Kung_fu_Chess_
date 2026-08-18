#pragma once

#include "../../kfc/model/piece.hpp"
#include "../../kfc/model/position.hpp"
#include "../../kfc/realtime/motion_kind.hpp"

namespace kfc::model {

/// An in-flight move, tracked outside Board -- the moving piece stays put on
/// Board until this motion resolves on arrival.
///
/// moving_piece is a full snapshot taken at motion start, not just an id:
/// once two motions can race for the same destination cell, one resolving
/// first may rewrite that cell before the other resolves, so trusting
/// Board's current contents to identify the mover would be wrong.
struct Motion {
    Piece moving_piece;
    Position source;
    Position destination;
    MotionKind kind;
    int duration_ms;
    int elapsed_ms;
    /// Computed once by MotionFactory; RealTimeArbiter just carries it
    /// forward to start the piece's cooldown on arrival.
    int cooldown_ms;
};

}  // namespace kfc::model
