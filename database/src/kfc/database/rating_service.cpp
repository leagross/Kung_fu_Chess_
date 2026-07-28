#include "kfc/database/rating_service.hpp"

#include "kfc/database/elo.hpp"
#include "kfc/database/user_repository.hpp"

namespace kfc::database {

void apply_game_result(UserRepository& users, std::optional<kfc::model::PieceColor> winner,
                       const std::string& white_username, const std::string& black_username) {
    std::optional<int> white = users.rating_of(white_username);
    std::optional<int> black = users.rating_of(black_username);
    if (!white.has_value() || !black.has_value()) {
        return;
    }

    // Score from White's point of view: 1 win, 0.5 draw, 0 loss. Black's is the
    // complement, so the exchange is symmetric.
    double white_score = !winner.has_value()                           ? 0.5
                         : (*winner == kfc::model::PieceColor::White) ? 1.0
                                                                      : 0.0;

    int new_white = elo_updated_rating(*white, *black, white_score);
    int new_black = elo_updated_rating(*black, *white, 1.0 - white_score);
    users.set_rating(white_username, new_white);
    users.set_rating(black_username, new_black);
}

void apply_forfeit(UserRepository& users, const std::string& loser_username) {
    std::optional<int> rating = users.rating_of(loser_username);
    if (!rating.has_value()) {
        return;
    }
    users.set_rating(loser_username, *rating - kDisconnectPenalty);
}

}  // namespace kfc::database
