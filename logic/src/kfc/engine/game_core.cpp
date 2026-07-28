#include "../../../include/kfc/engine/game_core.hpp"

#include "../../../include/kfc/rules/full_piece_rule_registry.hpp"

namespace kfc::model {

GameCore::GameCore(Board board, const ICooldownPolicy& standard_policy, const ICooldownPolicy& jump_policy,
                   const IPieceSpeedProvider& speed_provider, double meters_per_cell)
    : board_(std::move(board)),
      registry_(make_full_piece_rule_registry()),
      rule_engine_(registry_),
      arbiter_(board_),
      motion_factory_(standard_policy, jump_policy, speed_provider, meters_per_cell),
      engine_(board_, rule_engine_, arbiter_, motion_factory_) {}

}  // namespace kfc::model
