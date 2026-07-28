#include "../../../include/kfc/engine/game_engine.hpp"

#include "../../../include/kfc/rules/move_reasons.hpp"

namespace kfc::model {

GameEngine::GameEngine(const Board& board, const RuleEngine& rule_engine, RealTimeArbiter& real_time_arbiter,
                        const MotionFactory& motion_factory)
    : board_(board),
      rule_engine_(rule_engine),
      real_time_arbiter_(real_time_arbiter),
      motion_factory_(motion_factory),
      game_over_(false) {}

MoveResult GameEngine::request_move(const Position& source, const Position& destination) {
    if (game_over_) {
        return MoveResult{false, move_reasons::kGameOver};
    }

    std::optional<Piece> moving = board_.piece_at(source);
    if (moving.has_value() && real_time_arbiter_.is_piece_busy(moving->id)) {
        return MoveResult{false, move_reasons::kMotionInProgress};
    }

    MoveValidation validation = rule_engine_.validate_move(board_, source, destination);
    if (!validation.is_valid) {
        return MoveResult{false, validation.reason};
    }

    Motion motion = motion_factory_.create_move(*moving, source, destination);
    real_time_arbiter_.start_motion(motion);

    return MoveResult{true, move_reasons::kOk};
}

MoveResult GameEngine::request_jump(const Position& cell) {
    if (game_over_) {
        return MoveResult{false, move_reasons::kGameOver};
    }

    std::optional<Piece> piece = board_.piece_at(cell);
    if (!piece.has_value()) {
        return MoveResult{false, move_reasons::kEmptySource};
    }
    if (real_time_arbiter_.is_piece_busy(piece->id)) {
        return MoveResult{false, move_reasons::kMotionInProgress};
    }

    Motion motion = motion_factory_.create_jump(*piece, cell);
    real_time_arbiter_.start_motion(motion);

    return MoveResult{true, move_reasons::kOk};
}

ArrivalEvents GameEngine::wait(int ms) {
    // Once the game is over, no further motion -- even one already in
    // flight before the deciding capture -- may keep mutating the board,
    // score, or move history. The batch that actually decides the game
    // (game_over_ flips to true partway through the loop below) still
    // returns in full, so every observer sees the events that ended it;
    // only calls after that point become no-ops.
    if (game_over_) {
        return {};
    }

    ArrivalEvents events = real_time_arbiter_.advance_time(ms);

    for (const ArrivalEvent& event : events) {
        if (captured_a_king(event)) {
            game_over_ = true;
        }
    }

    return events;
}

bool GameEngine::is_game_over() const {
    return game_over_;
}

}  // namespace kfc::model
