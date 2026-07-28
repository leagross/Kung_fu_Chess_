#pragma once

#include <memory>
#include <unordered_map>

#include "../../kfc/model/piece.hpp"
#include "../../kfc/rules/movement_rule.hpp"

namespace kfc::model {

/// Maps each PieceKind to the IMovementRule that knows how it moves. Adding
/// a new piece kind's behavior means registering one rule here -- no other
/// class in the model or rules layer needs to change.
class PieceRuleRegistry {
public:
    /// Binds kind to rule, replacing any rule previously registered for it.
    void register_rule(PieceKind kind, std::unique_ptr<IMovementRule> rule);

    /// The rule registered for kind. Throws std::out_of_range if none was
    /// registered.
    const IMovementRule& rule_for(PieceKind kind) const;

private:
    std::unordered_map<PieceKind, std::unique_ptr<IMovementRule>> rules_;
};

}  // namespace kfc::model
