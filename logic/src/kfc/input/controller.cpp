#include "../../../include/kfc/input/controller.hpp"

#include <cassert>

namespace kfc::input {

Controller::Controller(const kfc::model::Board& board, kfc::model::IMoveRequester& move_requester,
                        BoardMapper board_mapper, std::optional<kfc::model::PieceColor> controlled_color)
    : board_(board),
      move_requester_(move_requester),
      board_mapper_(board_mapper),
      controlled_color_(controlled_color) {}

bool Controller::can_control(kfc::model::PieceColor piece_color) const {
    return !controlled_color_.has_value() || *controlled_color_ == piece_color;
}

bool Controller::selection_is_stale() const {
    if (!selected_cell_.has_value()) {
        return false;
    }
    // selected_piece_id_ is always set in lockstep with selected_cell_ (both
    // written together, both cleared together in clear_selection), so a
    // selected cell without a remembered id would be a logic error, not a
    // runtime input case.
    assert(selected_piece_id_.has_value());
    std::optional<kfc::model::Piece> piece = board_.piece_at(*selected_cell_);
    return !piece.has_value() || piece->id != *selected_piece_id_;
}

void Controller::clear_selection() {
    selected_cell_.reset();
    selected_piece_id_.reset();
}

ControllerResult Controller::click(int x, int y) {
    if (selection_is_stale()) {
        clear_selection();
    }

    std::optional<kfc::model::Position> cell = board_mapper_.pixel_to_cell(x, y);

    if (!cell.has_value()) {
        if (selected_cell_.has_value()) {
            clear_selection();
            return ControllerResult{ClickOutcome::SelectionCleared, std::nullopt};
        }
        return ControllerResult{ClickOutcome::Ignored, std::nullopt};
    }

    if (!selected_cell_.has_value()) {
        std::optional<kfc::model::Piece> piece = board_.piece_at(*cell);
        // Only pick up a piece this client is allowed to command -- in
        // networked play that excludes the opponent's pieces, so clicking one
        // is ignored here instead of being sent and bounced back as
        // "not_your_piece". can_control is always true in local hot-seat play.
        if (piece.has_value() && can_control(piece->color)) {
            selected_cell_ = *cell;
            selected_piece_id_ = piece->id;
            return ControllerResult{ClickOutcome::Selected, std::nullopt};
        }
        return ControllerResult{ClickOutcome::Ignored, std::nullopt};
    }

    // A second click landing on another piece of the same color replaces
    // the selection instead of requesting a move -- moving onto your own
    // piece is never legal, so treating it as a move request would just be
    // an expensive way to reselect. An empty cell or an enemy piece still
    // goes through request_move as usual, whether or not that move is legal.
    // selection_is_stale() already guaranteed the piece at selected_cell_
    // is still the one that was selected, by id, not just whatever is
    // sitting on that cell now.
    std::optional<kfc::model::Piece> selected_piece = board_.piece_at(*selected_cell_);
    std::optional<kfc::model::Piece> target_piece = board_.piece_at(*cell);
    if (target_piece.has_value() && target_piece->color == selected_piece->color) {
        selected_cell_ = *cell;
        selected_piece_id_ = target_piece->id;
        return ControllerResult{ClickOutcome::Selected, std::nullopt};
    }

    kfc::model::Position source = *selected_cell_;
    clear_selection();
    kfc::model::MoveResult result = move_requester_.request_move(source, *cell);
    return ControllerResult{ClickOutcome::MoveRequested, result};
}

ControllerResult Controller::jump(int x, int y) {
    std::optional<kfc::model::Position> cell = board_mapper_.pixel_to_cell(x, y);

    if (!cell.has_value()) {
        return ControllerResult{ClickOutcome::Ignored, std::nullopt};
    }

    kfc::model::MoveResult result = move_requester_.request_jump(*cell);
    return ControllerResult{ClickOutcome::JumpRequested, result};
}

std::optional<kfc::model::Position> Controller::selected_cell() const {
    if (selection_is_stale()) {
        return std::nullopt;
    }
    return selected_cell_;
}

}  // namespace kfc::input
