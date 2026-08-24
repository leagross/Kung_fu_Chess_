#pragma once

#include "../../kfc/rules/piece_rule_registry.hpp"

namespace kfc::model {

/// A PieceRuleRegistry with every known PieceKind's movement rule already registered.
[[nodiscard]] PieceRuleRegistry make_full_piece_rule_registry();

}  // namespace kfc::model
