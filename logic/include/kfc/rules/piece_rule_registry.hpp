#pragma once

#include <memory>
#include <unordered_map>

#include "../../kfc/model/piece.hpp"
#include "../../kfc/rules/movement_rule.hpp"

namespace kfc::model {

/// Maps each PieceKind to the IMovementRule that knows how it moves.
class PieceRuleRegistry {
public:
    /// Replaces any rule previously registered for kind.
    void register_rule(PieceKind kind, std::unique_ptr<IMovementRule> rule);

    /// Throws std::out_of_range if none was registered.
    const IMovementRule& rule_for(PieceKind kind) const;

private:
    std::unordered_map<PieceKind, std::unique_ptr<IMovementRule>> rules_;
};

}  // namespace kfc::model
