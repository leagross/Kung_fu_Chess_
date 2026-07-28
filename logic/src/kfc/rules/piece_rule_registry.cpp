#include "../../../include/kfc/rules/piece_rule_registry.hpp"

#include <utility>

namespace kfc::model {

void PieceRuleRegistry::register_rule(PieceKind kind, std::unique_ptr<IMovementRule> rule) {
    rules_[kind] = std::move(rule);
}

const IMovementRule& PieceRuleRegistry::rule_for(PieceKind kind) const {
    return *rules_.at(kind);
}

}  // namespace kfc::model
