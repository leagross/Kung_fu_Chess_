#pragma once

#include "../../kfc/model/board.hpp"
#include "../../kfc/model/position.hpp"
#include "../../kfc/rules/move_validation.hpp"
#include "../../kfc/rules/piece_rule_registry.hpp"

namespace kfc::model {

/// Answers "given source and destination, is this move legal right now?".
/// Read-only with respect to Board -- never moves, captures, or otherwise
/// mutates anything. Knows nothing about whose turn it is or whether the
/// game has already ended; that belongs to GameEngine.
class RuleEngine {
public:
    /// rules must outlive this RuleEngine.
    explicit RuleEngine(const PieceRuleRegistry& rules);

    /// Validates a requested move at the rule level only.
    MoveValidation validate_move(const Board& board, const Position& source,
                                  const Position& destination) const;

private:
    const PieceRuleRegistry& rules_;
};

}  // namespace kfc::model
