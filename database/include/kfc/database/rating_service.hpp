#pragma once

#include <optional>
#include <string>

#include "kfc/model/piece.hpp"

namespace kfc::database {

class IUserStore;

/// Applies one finished game's ELO change to both players; winner is nullopt for a draw. Call once per game.
void apply_game_result(IUserStore& users, std::optional<kfc::model::PieceColor> winner,
                       const std::string& white_username, const std::string& black_username);

/// Applies kDisconnectPenalty to the player who dropped.
void apply_forfeit(IUserStore& users, const std::string& loser_username);

}  // namespace kfc::database
