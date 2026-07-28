#include "../../../include/kfc/rules/movement_geometry.hpp"

namespace kfc::model {

std::vector<Position> sliding_destinations(const Board& board, const Piece& piece,
                                            const std::vector<std::pair<int, int>>& directions) {
    std::vector<Position> destinations;

    for (const auto& [row_step, col_step] : directions) {
        Position current{piece.cell.row + row_step, piece.cell.col + col_step};

        while (board.in_bounds(current)) {
            std::optional<Piece> occupant = board.piece_at(current);
            if (!occupant.has_value()) {
                destinations.push_back(current);
                current = Position{current.row + row_step, current.col + col_step};
                continue;
            }
            if (occupant->color != piece.color) {
                destinations.push_back(current);
            }
            break;
        }
    }

    return destinations;
}

std::vector<Position> stepping_destinations(const Board& board, const Piece& piece,
                                             const std::vector<std::pair<int, int>>& offsets) {
    std::vector<Position> destinations;

    for (const auto& [row_offset, col_offset] : offsets) {
        Position candidate{piece.cell.row + row_offset, piece.cell.col + col_offset};
        if (!board.in_bounds(candidate)) {
            continue;
        }
        std::optional<Piece> occupant = board.piece_at(candidate);
        if (occupant.has_value() && occupant->color == piece.color) {
            continue;
        }
        destinations.push_back(candidate);
    }

    return destinations;
}

}  // namespace kfc::model
