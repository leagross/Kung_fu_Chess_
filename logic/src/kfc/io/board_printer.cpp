#include "../../../include/kfc/io/board_printer.hpp"

#include <optional>
#include <sstream>

#include "../../../include/kfc/io/piece_token.hpp"
#include "../../../include/kfc/model/piece.hpp"
#include "../../../include/kfc/model/position.hpp"

using kfc::model::Board;
using kfc::model::Piece;
using kfc::model::Position;

namespace kfc::io {

std::string BoardPrinter::print(const Board& board) const {
    std::ostringstream out;
    for (int row = 0; row < board.height(); ++row) {
        for (int col = 0; col < board.width(); ++col) {
            if (col > 0) {
                out << ' ';
            }
            std::optional<Piece> occupant = board.piece_at(Position{row, col});
            out << (occupant.has_value() ? piece_token_text(occupant->color, occupant->kind) : ".");
        }
        out << '\n';
    }
    return out.str();
}

}  // namespace kfc::io
