#pragma once

#include <optional>
#include <string>

#include "kfc/model/piece.hpp"

namespace kfc::database {

class UserRepository;

/// Applies one finished game's ELO change to both players and persists it.
/// winner is std::nullopt for a draw. Reads each player's current rating from
/// users, computes their new ratings (see elo.hpp), and writes them back. A
/// no-op if either username is unknown to users (e.g. a game that ended before
/// two named players were seated). Call exactly once per finished game.
void apply_game_result(UserRepository& users, std::optional<kfc::model::PieceColor> winner,
                       const std::string& white_username, const std::string& black_username);

/// Applies the flat disconnect/timeout forfeit penalty (kDisconnectPenalty, see
/// elo.hpp) to the player who dropped, and persists it. Unlike apply_game_result
/// this is a fixed dock, not an ELO exchange, and touches only the loser. A
/// no-op if the username is unknown to users.
void apply_forfeit(UserRepository& users, const std::string& loser_username);

}  // namespace kfc::database
