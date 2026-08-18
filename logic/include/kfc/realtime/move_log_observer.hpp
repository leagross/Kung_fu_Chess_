#pragma once

#include <string>
#include <vector>

#include "../../kfc/model/piece.hpp"
#include "../../kfc/realtime/game_observer.hpp"

namespace kfc::model {

/// One row of the timestamped move table: simulated arrival time
/// (RealTimeArbiter's clock, same as ArrivalEvent::arrived_at_ms) plus a
/// short algebraic-style description.
struct MoveLogEntry {
    long long time_ms;
    std::string notation;
};

/// Keeps a human-readable move history per side, one line per arrival.
/// Pure text bookkeeping; screen formatting is a renderer's job.
class MoveLogObserver : public IGameObserver {
public:
    /// board_height turns a Position's row into a chess rank for
    /// entries()'s notation; moves() doesn't use it.
    explicit MoveLogObserver(int board_height = 8);

    void on_arrival(const ArrivalEvent& event) override;

    /// This side's moves so far, oldest first: "<piece token>
    /// <source>-><destination>", with " x<captured token>" on a capture.
    [[nodiscard]] const std::vector<std::string>& moves(PieceColor color) const;

    /// This side's moves so far, oldest first, as timestamp plus a
    /// simplified algebraic notation -- not full SAN: no check/mate marks,
    /// no disambiguation, no en passant or castling.
    [[nodiscard]] const std::vector<MoveLogEntry>& entries(PieceColor color) const;

private:
    int board_height_;
    std::vector<std::string> white_moves_;
    std::vector<std::string> black_moves_;
    std::vector<MoveLogEntry> white_entries_;
    std::vector<MoveLogEntry> black_entries_;
};

}  // namespace kfc::model
