#pragma once

#include <string>

namespace kfc::model {

/// Outcome of a move or jump request at the application-service boundary.
/// For an accepted command, reason is "ok"; app-level rejections use their
/// own stable reasons ("game_over", "motion_in_progress" -- now per piece,
/// not global); rule-level rejections copy the reason RuleEngine returned,
/// unchanged. Its own header (rather than living inside game_engine.hpp) so
/// IMoveRequester can return it without depending on GameEngine itself.
/// [[nodiscard]] on the type (not each returning function) so every current
/// and future function that hands one back is covered by one annotation: a
/// MoveResult carries a possible rejection reason, and silently dropping it
/// means never noticing a move was refused.
struct [[nodiscard]] MoveResult {
    bool is_accepted;
    std::string reason;
};

}  // namespace kfc::model
