#pragma once

#include "../../kfc/engine/move_result.hpp"
#include "../../kfc/model/position.hpp"

namespace kfc::model {

/// Narrower interface than GameEngine, so a networked client can satisfy Controller's dependency too.
class IMoveRequester {
public:
    virtual ~IMoveRequester() = default;

    virtual MoveResult request_move(const Position& source, const Position& destination) = 0;
    virtual MoveResult request_jump(const Position& cell) = 0;
};

}  // namespace kfc::model
