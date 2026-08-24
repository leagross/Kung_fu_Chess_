#pragma once

#include "../../kfc/engine/move_requester.hpp"
#include "../../kfc/engine/move_result.hpp"
#include "../../kfc/model/board.hpp"
#include "../../kfc/model/position.hpp"
#include "../../kfc/realtime/motion_factory.hpp"
#include "../../kfc/realtime/real_time_arbiter.hpp"
#include "../../kfc/rules/rule_engine.hpp"

namespace kfc::model {

/// Public command boundary for moves/jumps; coordinates Board, RuleEngine, RealTimeArbiter, MotionFactory.
class GameEngine : public IMoveRequester {
public:
    /// All four dependencies must outlive this GameEngine.
    GameEngine(const Board& board, const RuleEngine& rule_engine, RealTimeArbiter& real_time_arbiter,
               const MotionFactory& motion_factory);

    MoveResult request_move(const Position& source, const Position& destination) override;

    /// Bypasses RuleEngine -- a jump isn't a chess move, it has its own timing.
    MoveResult request_jump(const Position& cell) override;

    /// Returns arrivals for the caller to forward to observers.
    ArrivalEvents wait(int ms);

    [[nodiscard]] bool is_game_over() const;

private:
    const Board& board_;
    const RuleEngine& rule_engine_;
    RealTimeArbiter& real_time_arbiter_;
    const MotionFactory& motion_factory_;
    bool game_over_;
};

}  // namespace kfc::model
