#include "kfc/database/rating_service.hpp"

#include <utility>

#include "kfc/database/elo.hpp"
#include "kfc/database/user_repository.hpp"

namespace kfc::database {

void apply_game_result(UserRepository& users, std::optional<kfc::model::PieceColor> winner,
                       const std::string& white_username, const std::string& black_username) {
    // Score from White's point of view: 1 win, 0.5 draw, 0 loss. Black's is the
    // complement, so the exchange is symmetric.
    double white_score = !winner.has_value()                           ? 0.5
                         : (*winner == kfc::model::PieceColor::White) ? 1.0
                                                                      : 0.0;

    // Read, computed and written as one step by the store. Doing it here --
    // two rating_of calls, then two set_rating calls -- would let a game
    // finishing on another thread slip in between and have its result
    // overwritten. See UserRepository::rerate_pair.
    //
    // Nothing is written when either player is unknown to the store (a game
    // that ended before two named players were seated), which is the documented
    // no-op and not a failure worth reporting.
    (void)users.rerate_pair(white_username, black_username, [white_score](int white, int black) {
        return std::pair{elo_updated_rating(white, black, white_score),
                         elo_updated_rating(black, white, 1.0 - white_score)};
    });
}

void apply_forfeit(UserRepository& users, const std::string& loser_username) {
    (void)users.rerate(loser_username, [](int rating) { return rating - kDisconnectPenalty; });
}

}  // namespace kfc::database
