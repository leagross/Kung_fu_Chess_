#include "../../../include/kfc/rules/rule_engine.hpp"

#include <algorithm>

#include "../../../include/kfc/rules/move_reasons.hpp"

namespace kfc::model {

RuleEngine::RuleEngine(const PieceRuleRegistry& rules) : rules_(rules) {}

MoveValidation RuleEngine::validate_move(const Board& board, const Position& source,
                                          const Position& destination) const {
    if (!board.in_bounds(source) || !board.in_bounds(destination)) {
        return MoveValidation{false, move_reasons::kOutsideBoard};
    }

    std::optional<Piece> moving = board.piece_at(source);
    if (!moving.has_value()) {
        return MoveValidation{false, move_reasons::kEmptySource};
    }

    std::optional<Piece> occupant = board.piece_at(destination);
    if (occupant.has_value() && occupant->color == moving->color) {
        return MoveValidation{false, move_reasons::kFriendlyDestination};
    }

    const IMovementRule& rule = rules_.rule_for(moving->kind);
    std::vector<Position> destinations = rule.legal_destinations(board, *moving);
    bool is_legal = std::find(destinations.begin(), destinations.end(), destination) != destinations.end();
    if (!is_legal) {
        return MoveValidation{false, move_reasons::kIllegalPieceMove};
    }

    return MoveValidation{true, move_reasons::kOk};
}

}  // namespace kfc::model
