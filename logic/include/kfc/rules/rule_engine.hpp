#pragma once

#include "../../kfc/model/board.hpp"
#include "../../kfc/model/position.hpp"
#include "../../kfc/rules/move_validation.hpp"
#include "../../kfc/rules/piece_rule_registry.hpp"

namespace kfc::model {

/// Read-only rule-level legality check; whose turn it is and game-over belong to GameEngine.
class RuleEngine {
public:
    /// rules must outlive this RuleEngine.
    explicit RuleEngine(const PieceRuleRegistry& rules);

    MoveValidation validate_move(const Board& board, const Position& source,
                                  const Position& destination) const;

private:
    const PieceRuleRegistry& rules_;
};

}  // namespace kfc::model
