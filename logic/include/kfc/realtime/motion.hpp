#pragma once

#include "../../kfc/model/piece.hpp"
#include "../../kfc/model/position.hpp"
#include "../../kfc/realtime/motion_kind.hpp"

namespace kfc::model {

/// An in-flight move, tracked outside Board until it resolves on arrival.
/// moving_piece is a full snapshot, since two motions can race for one cell.
struct Motion {
    Piece moving_piece;
    Position source;
    Position destination;
    MotionKind kind;
    int duration_ms;
    int elapsed_ms;
    int cooldown_ms;  // computed once by MotionFactory, carried forward by RealTimeArbiter
};

}  // namespace kfc::model
