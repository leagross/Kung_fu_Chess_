#pragma once

#include "../../kfc/engine/move_result.hpp"
#include "../../kfc/model/position.hpp"

namespace kfc::model {

/// What Controller actually depends on to turn a resolved click/jump into a
/// game command -- narrower than the concrete GameEngine class, so a caller
/// that isn't running GameEngine locally at all (a networked client, which
/// instead serializes the request and sends it to a server) can satisfy
/// Controller's dependency too, without Controller ever knowing the
/// difference. Lives alongside GameEngine (not under kfc::input) so the
/// project's documented dependency direction -- Controller depends on
/// GameEngine, never the reverse -- still holds: Controller now depends on
/// an abstraction owned by the engine layer, exactly as it already depends
/// on the concrete GameEngine class today.
class IMoveRequester {
public:
    virtual ~IMoveRequester() = default;

    /// Same contract as GameEngine::request_move.
    virtual MoveResult request_move(const Position& source, const Position& destination) = 0;

    /// Same contract as GameEngine::request_jump.
    virtual MoveResult request_jump(const Position& cell) = 0;
};

}  // namespace kfc::model
