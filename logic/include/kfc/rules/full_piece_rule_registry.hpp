#pragma once

#include "../../kfc/rules/piece_rule_registry.hpp"

namespace kfc::model {

/// A PieceRuleRegistry with every known PieceKind's movement rule already
/// registered -- the one registry any composition root (Game, server::Match)
/// needs, kept in one place so a new piece kind only has to be wired up
/// here, not separately in every place that assembles a playable game.
[[nodiscard]] PieceRuleRegistry make_full_piece_rule_registry();

}  // namespace kfc::model
