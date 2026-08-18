#pragma once

#include <optional>
#include <string>

#include "kfc/model/piece.hpp"

namespace kfc::database {

class IUserStore;

/// Applies one finished game's ELO change to both players and persists it.
/// winner is std::nullopt for a draw. No-op if either username is unknown.
/// Call exactly once per finished game.
void apply_game_result(IUserStore& users, std::optional<kfc::model::PieceColor> winner,
                       const std::string& white_username, const std::string& black_username);

/// Applies the flat disconnect/timeout forfeit penalty (see kDisconnectPenalty)
/// to the player who dropped. No-op if the username is unknown.
void apply_forfeit(IUserStore& users, const std::string& loser_username);

}  // namespace kfc::database
