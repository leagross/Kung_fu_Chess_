#pragma once

#include "../../kfc/engine/move_requester.hpp"
#include "../../kfc/engine/move_result.hpp"
#include "../../kfc/model/board.hpp"
#include "../../kfc/model/position.hpp"
#include "../../kfc/realtime/motion_factory.hpp"
#include "../../kfc/realtime/real_time_arbiter.hpp"
#include "../../kfc/rules/rule_engine.hpp"

namespace kfc::model {

/// The public command boundary for the common route plus the jump-in-place
/// extension -- the only entry point Controller and TextTestRunner use to
/// request a move, a jump, or to advance time. Coordinates Board,
/// RuleEngine, RealTimeArbiter, and MotionFactory; contains no
/// piece-specific movement logic, rendering code, input parsing, or DSL
/// parsing of its own. Implements IMoveRequester so a networked client's
/// Controller can be driven by something else entirely (see ServerLink)
/// without GameEngine itself changing.
class GameEngine : public IMoveRequester {
public:
    /// All four dependencies must outlive this GameEngine. real_time_arbiter
    /// should be wrapping the same board.
    GameEngine(const Board& board, const RuleEngine& rule_engine, RealTimeArbiter& real_time_arbiter,
               const MotionFactory& motion_factory);

    /// Requests a move from source to destination. Rejects with "game_over"
    /// if the game has already ended, or "motion_in_progress" if the piece
    /// at source already has a motion in flight or is in cooldown --
    /// unrelated pieces may move or jump at the same time. Otherwise
    /// delegates rule-level legality to RuleEngine and, if legal, starts an
    /// ordinary move via MotionFactory/RealTimeArbiter.
    MoveResult request_move(const Position& source, const Position& destination) override;

    /// Requests a jump-in-place for the piece at cell. Rejects with
    /// "game_over" or "motion_in_progress" under the same conditions as
    /// request_move, and "empty_source" if cell holds no piece. Bypasses
    /// RuleEngine entirely -- a jump is not a chess move, it is a distinct
    /// defensive action with its own timing (see MotionFactory).
    MoveResult request_jump(const Position& cell) override;

    /// Advances simulated time by ms, delegating entirely to
    /// RealTimeArbiter. If any arrival captured a king, marks the game over.
    /// Never touches Board directly. Returns whatever arrivals happened, so
    /// a caller (Game) can forward them to observers -- GameEngine itself
    /// never touches an observer list; that would mix "what a move is
    /// allowed to do" with "who cares once it happens".
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
