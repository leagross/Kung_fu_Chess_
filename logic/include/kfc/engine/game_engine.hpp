#pragma once

#include "../../kfc/engine/move_requester.hpp"
#include "../../kfc/engine/move_result.hpp"
#include "../../kfc/model/board.hpp"
#include "../../kfc/model/position.hpp"
#include "../../kfc/realtime/motion_factory.hpp"
#include "../../kfc/realtime/real_time_arbiter.hpp"
#include "../../kfc/rules/rule_engine.hpp"

namespace kfc::model {

/// The public command boundary for moves and jumps -- the only entry point
/// Controller/TextTestRunner use. Coordinates Board, RuleEngine,
/// RealTimeArbiter and MotionFactory; no piece-specific logic of its own.
class GameEngine : public IMoveRequester {
public:
    /// All four dependencies must outlive this GameEngine. real_time_arbiter
    /// should be wrapping the same board.
    GameEngine(const Board& board, const RuleEngine& rule_engine, RealTimeArbiter& real_time_arbiter,
               const MotionFactory& motion_factory);

    /// Rejects with "game_over" or "motion_in_progress" (per piece);
    /// otherwise delegates legality to RuleEngine and starts the move.
    MoveResult request_move(const Position& source, const Position& destination) override;

    /// Bypasses RuleEngine entirely -- a jump is not a chess move, it has
    /// its own timing (see MotionFactory).
    MoveResult request_jump(const Position& cell) override;

    /// Advances simulated time by ms via RealTimeArbiter; marks game over
    /// if any arrival captured a king. Returns arrivals for the caller to
    /// forward to observers -- this class never touches an observer list.
    ArrivalEvents wait(int ms);

    /// True once a king has been captured. Once true, request_move and
    /// request_jump always reject with "game_over".
    [[nodiscard]] bool is_game_over() const;

private:
    const Board& board_;
    const RuleEngine& rule_engine_;
    RealTimeArbiter& real_time_arbiter_;
    const MotionFactory& motion_factory_;
    bool game_over_;
};

}  // namespace kfc::model
