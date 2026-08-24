#include "kfc/database/rating_service.hpp"

#include <utility>

#include "kfc/database/elo.hpp"
#include "kfc/database/user_store.hpp"

namespace kfc::database {

void apply_game_result(IUserStore& users, std::optional<kfc::model::PieceColor> winner,
                       const std::string& white_username, const std::string& black_username) {
    // Score from White's point of view; Black's is the complement.
    double white_score = !winner.has_value()                           ? 0.5
                         : (*winner == kfc::model::PieceColor::White) ? 1.0
                                                                      : 0.0;

    (void)users.rerate_pair(white_username, black_username, [white_score](int white, int black) {
        return std::pair{elo_updated_rating(white, black, white_score),
                         elo_updated_rating(black, white, 1.0 - white_score)};
    });
}

void apply_forfeit(IUserStore& users, const std::string& loser_username) {
    (void)users.rerate(loser_username, [](int rating) { return rating - kDisconnectPenalty; });
}

}  // namespace kfc::database
