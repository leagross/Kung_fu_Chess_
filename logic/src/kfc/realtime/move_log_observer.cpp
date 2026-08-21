#include "../../../include/kfc/realtime/move_log_observer.hpp"

#include "../../../include/kfc/io/piece_token.hpp"
#include "../../../include/kfc/model/position.hpp"

namespace kfc::model {

namespace {

/// Blank for a Pawn (SAN never names it), kfc::io::letter_for_kind otherwise.
char algebraic_piece_letter(PieceKind kind) {
    return kind == PieceKind::Pawn ? '\0' : kfc::io::letter_for_kind(kind);
}

char algebraic_file(int col) {
    return static_cast<char>('a' + col);
}

std::string algebraic_square(const Position& pos, int board_height) {
    return std::string(1, algebraic_file(pos.col)) + std::to_string(board_height - pos.row);
}

/// The piece letter as a string, empty for a Pawn (avoids a stray '\0').
std::string algebraic_piece_prefix(PieceKind kind) {
    char letter = algebraic_piece_letter(kind);
    return letter == '\0' ? std::string() : std::string(1, letter);
}

std::string algebraic_notation(const ArrivalEvent& event, int board_height) {
    std::string destination = algebraic_square(event.destination, board_height);

    if (event.kind == MotionKind::JumpInPlace) {
        // Marked "(J)" so a jump landing back on its own square (source ==
        // destination) is never mistaken for a chess move like "Ne4".
        return algebraic_piece_prefix(event.moved_piece.kind) + destination + "(J)";
    }

    bool captured = event.captured_piece.has_value();

    if (event.was_promotion) {
        // Only a pawn promotes: rendered pawn-style (source file on a
        // capture) then "=Q", e.g. "e8=Q" or "exd8=Q".
        std::string base =
            captured ? std::string(1, algebraic_file(event.source.col)) + "x" + destination : destination;
        return base + "=" + std::string(1, kfc::io::letter_for_kind(event.moved_piece.kind));
    }

    if (event.moved_piece.kind == PieceKind::Pawn) {
        // A pawn capture is prefixed by its source file, not a piece letter.
        return captured ? std::string(1, algebraic_file(event.source.col)) + "x" + destination : destination;
    }

    std::string notation = algebraic_piece_prefix(event.moved_piece.kind);
    if (captured) {
        notation += "x";
    }
    return notation + destination;
}

}  // namespace

MoveLogObserver::MoveLogObserver(int board_height) : board_height_(board_height) {}

void MoveLogObserver::on_arrival(const ArrivalEvent& event) {
    std::string entry = kfc::io::piece_token_text(event.moved_piece.color, event.moved_piece.kind) + " " +
                         to_string(event.source) + "->" + to_string(event.destination);
    if (event.captured_piece.has_value()) {
        entry += " x" + kfc::io::piece_token_text(event.captured_piece->color, event.captured_piece->kind);
    }
    if (event.kind == MotionKind::JumpInPlace) {
        entry += " (jump)";
    } else if (event.was_promotion) {
        entry += " (promoted)";
    }

    MoveLogEntry log_entry{event.arrived_at_ms, algebraic_notation(event, board_height_)};

    if (event.moved_piece.color == PieceColor::White) {
        white_moves_.push_back(std::move(entry));
        white_entries_.push_back(std::move(log_entry));
    } else {
        black_moves_.push_back(std::move(entry));
        black_entries_.push_back(std::move(log_entry));
    }
}

const std::vector<std::string>& MoveLogObserver::moves(PieceColor color) const {
    return (color == PieceColor::White) ? white_moves_ : black_moves_;
}

const std::vector<MoveLogEntry>& MoveLogObserver::entries(PieceColor color) const {
    return (color == PieceColor::White) ? white_entries_ : black_entries_;
}

}  // namespace kfc::model
