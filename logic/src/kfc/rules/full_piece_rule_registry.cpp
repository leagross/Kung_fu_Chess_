#include "../../../include/kfc/rules/full_piece_rule_registry.hpp"

#include <memory>

#include "../../../include/kfc/rules/bishop_rule.hpp"
#include "../../../include/kfc/rules/drone_rule.hpp"
#include "../../../include/kfc/rules/king_rule.hpp"
#include "../../../include/kfc/rules/knight_rule.hpp"
#include "../../../include/kfc/rules/pawn_rule.hpp"
#include "../../../include/kfc/rules/queen_rule.hpp"
#include "../../../include/kfc/rules/rook_rule.hpp"

namespace kfc::model {

PieceRuleRegistry make_full_piece_rule_registry() {
    PieceRuleRegistry registry;
    registry.register_rule(PieceKind::Rook, std::make_unique<RookRule>());
    registry.register_rule(PieceKind::Bishop, std::make_unique<BishopRule>());
    registry.register_rule(PieceKind::Queen, std::make_unique<QueenRule>());
    registry.register_rule(PieceKind::Knight, std::make_unique<KnightRule>());
    registry.register_rule(PieceKind::King, std::make_unique<KingRule>());
    registry.register_rule(PieceKind::Pawn, std::make_unique<PawnRule>());
    registry.register_rule(PieceKind::Drone, std::make_unique<DroneRule>());
    return registry;
}

}  // namespace kfc::model
