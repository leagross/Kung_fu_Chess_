#include "../../../include/kfc/io/board_parser.hpp"

#include <optional>

#include "../../../include/kfc/io/parse_error.hpp"
#include "../../../include/kfc/io/piece_token.hpp"
#include "../../../include/kfc/io/text_tokenizer.hpp"
#include "../../../include/kfc/model/piece.hpp"
#include "../../../include/kfc/model/position.hpp"

using kfc::model::Board;
using kfc::model::Piece;
using kfc::model::PieceId;
using kfc::model::PieceState;
using kfc::model::Position;

namespace kfc::io {

Board BoardParser::parse(const std::vector<std::string>& board_lines) const {
    if (board_lines.empty()) {
        throw ParseError("EMPTY_BOARD");
    }

    std::vector<std::vector<std::string>> grid;
    grid.reserve(board_lines.size());
    for (const std::string& line : board_lines) {
        grid.push_back(tokenize(line));
    }

    std::size_t width = grid.front().size();
    // A line that is blank (or only whitespace) tokenizes to nothing -- a
    // zero-width board, which would otherwise be built as a valid empty board.
    if (width == 0) {
        throw ParseError("EMPTY_BOARD");
    }
    for (const std::vector<std::string>& row : grid) {
        if (row.size() != width) {
            throw ParseError("ROW_WIDTH_MISMATCH");
        }
    }

    // Parse every cell exactly once (std::nullopt == an empty "." cell),
    // validating as we go, instead of parsing each token here and then again
    // when the board is built below.
    std::vector<std::vector<std::optional<PieceToken>>> parsed(grid.size());
    for (std::size_t row = 0; row < grid.size(); ++row) {
        parsed[row].reserve(width);
        for (const std::string& token : grid[row]) {
            if (token == ".") {
                parsed[row].push_back(std::nullopt);
                continue;
            }
            std::optional<PieceToken> piece = parse_piece_token(token);
            if (!piece.has_value()) {
                throw ParseError("UNKNOWN_TOKEN");
            }
            parsed[row].push_back(piece);
        }
    }

    Board board(static_cast<int>(width), static_cast<int>(grid.size()));
    int next_id = 1;
    for (int row = 0; row < static_cast<int>(grid.size()); ++row) {
        for (int col = 0; col < static_cast<int>(width); ++col) {
            const std::optional<PieceToken>& cell = parsed[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)];
            if (!cell.has_value()) {
                continue;
            }
            board.add_piece(Piece{PieceId{next_id++}, cell->color, cell->kind, Position{row, col}, PieceState::Idle});
        }
    }
    return board;
}

}  // namespace kfc::io
