#pragma once

#include "../../kfc/model/piece.hpp"
#include "../../kfc/model/position.hpp"
#include "../../kfc/realtime/motion_kind.hpp"

namespace kfc::model {

/// An in-flight move, tracked outside Board. Board keeps representing only
/// logical occupancy -- the moving piece stays put there until this motion
/// resolves on arrival, which is what makes print/snapshot output
/// deterministic regardless of when it is queried mid-flight.
///
/// moving_piece is a full snapshot taken when the motion started, not just
/// an id. This matters once two motions can share a destination cell (a
/// jump landing on the same cell an attacker is arriving at): resolving one
/// motion may rewrite that cell before the other resolves, so trusting
/// "whatever Board currently holds at source" to identify the mover would
/// be wrong. The snapshot makes each motion self-contained.
struct Motion {
    Piece moving_piece;
    Position source;
    Position destination;
    MotionKind kind;
    int duration_ms;
    int elapsed_ms;
    /// How long the piece rests once this motion arrives. Computed once, by
    /// MotionFactory, at creation time -- RealTimeArbiter just carries it
    /// forward and starts the piece's cooldown countdown with it on arrival.
    int cooldown_ms;
};

}  // namespace kfc::model
