#include "../../../include/kfc/model/board.hpp"

#include <stdexcept>

namespace kfc::model {


Board::Board(int width, int height)
    : width_(width),
      height_(height),
      cells_((width < 0 || height < 0)
                 // Guard before the size_t multiply below: a negative int
                 // wraps to an enormous size_t, which would ask the vector to
                 // allocate gigabytes instead of failing cleanly.
                 ? throw std::invalid_argument("Board: width and height must be non-negative")
                 : static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {}

int Board::width() const {
    return width_;
}

int Board::height() const {
    return height_;
}

bool Board::in_bounds(const Position& pos) const {
    return pos.row >= 0 && pos.row < height_ && pos.col >= 0 && pos.col < width_;
}

std::size_t Board::index(const Position& pos) const {
    return static_cast<std::size_t>(pos.row) * static_cast<std::size_t>(width_) +
           static_cast<std::size_t>(pos.col);
}

std::optional<Piece> Board::piece_at(const Position& pos) const {
    if (!in_bounds(pos)) {
        return std::nullopt;
    }
    return cells_[index(pos)];
}

void Board::add_piece(const Piece& piece) {
    if (!in_bounds(piece.cell)) {
        throw std::out_of_range("Board::add_piece: position out of bounds");
    }
    if (cells_[index(piece.cell)].has_value()) {
        throw std::logic_error("Board::add_piece: cell already occupied");
    }
    cells_[index(piece.cell)] = piece;
}

void Board::set_piece_state(const Position& pos, PieceState state) {
    if (!in_bounds(pos)) {
        throw std::out_of_range("Board::set_piece_state: position out of bounds");
    }
    std::optional<Piece>& cell = cells_[index(pos)];
    if (!cell.has_value()) {
        throw std::logic_error("Board::set_piece_state: cell is empty");
    }
    cell->state = state;
}

void Board::remove_piece(const Position& pos) {
    if (!in_bounds(pos)) {
        throw std::out_of_range("Board::remove_piece: position out of bounds");
    }
    cells_[index(pos)] = std::nullopt;
}

void Board::move_piece(const Position& from, const Position& to) {
    if (!in_bounds(from) || !in_bounds(to)) {
        throw std::out_of_range("Board::move_piece: position out of bounds");
    }
    std::optional<Piece> moving = cells_[index(from)];
    if (!moving.has_value()) {
        throw std::logic_error("Board::move_piece: source cell is empty");
    }
    cells_[index(from)] = std::nullopt;
    moving->cell = to;
    cells_[index(to)] = moving;
}

}  // namespace kfc::model
