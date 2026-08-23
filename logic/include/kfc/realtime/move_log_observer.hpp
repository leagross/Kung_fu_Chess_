#pragma once

#include <string>
#include <vector>

#include "../../kfc/model/piece.hpp"
#include "../../kfc/realtime/game_observer.hpp"

namespace kfc::model {

/// time_ms is RealTimeArbiter's clock, same as ArrivalEvent::arrived_at_ms.
struct MoveLogEntry {
    long long time_ms;
    std::string notation;
};

/// Human-readable move history per side, one line per arrival.
class MoveLogObserver : public IGameObserver {
public:
    /// board_height turns a Position's row into a chess rank for entries()'s notation.
    explicit MoveLogObserver(int board_height = 8);

    void on_arrival(const ArrivalEvent& event) override;

    /// "<piece token> <source>-><destination>", with " x<captured token>" on a capture.
    [[nodiscard]] const std::vector<std::string>& moves(PieceColor color) const;

    /// Simplified algebraic notation, not full SAN (no check/mate, disambiguation, en passant, castling).
    [[nodiscard]] const std::vector<MoveLogEntry>& entries(PieceColor color) const;

private:
    int board_height_;
    std::vector<std::string> white_moves_;
    std::vector<std::string> black_moves_;
    std::vector<MoveLogEntry> white_entries_;
    std::vector<MoveLogEntry> black_entries_;
};

}  // namespace kfc::model
