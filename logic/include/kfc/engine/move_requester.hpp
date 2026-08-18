#pragma once

#include "../../kfc/engine/move_result.hpp"
#include "../../kfc/model/position.hpp"

namespace kfc::model {

/// Narrower interface than GameEngine so a networked client (which
/// serializes the request instead of running GameEngine locally) can
/// satisfy Controller's dependency too.
class IMoveRequester {
public:
    virtual ~IMoveRequester() = default;

    /// Same contract as GameEngine::request_move.
    virtual MoveResult request_move(const Position& source, const Position& destination) = 0;

    /// Same contract as GameEngine::request_jump.
    virtual MoveResult request_jump(const Position& cell) = 0;
};

}  // namespace kfc::model
