#pragma once

#include "../../kfc/model/piece.hpp"
#include "../../kfc/model/position.hpp"
#include "../../kfc/realtime/motion_kind.hpp"

namespace kfc::model {

/// An in-flight move, tracked outside Board until it resolves on arrival.
/// moving_piece is a full snapshot at motion start, not just an id -- two
/// motions racing for one cell means Board's current contents can't be
/// trusted to identify the mover.
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
