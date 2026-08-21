#pragma once

#include <optional>

#include "../../kfc/model/board.hpp"
#include "../../kfc/engine/move_requester.hpp"
#include "../../kfc/engine/move_result.hpp"
#include "../../kfc/input/board_mapper.hpp"
#include "../../kfc/model/position.hpp"

namespace kfc::input {

/// Outcome of a Controller::click or Controller::jump call. Only
/// MoveRequested/JumpRequested touch IMoveRequester and populate move_result.
enum class ClickOutcome {
    Ignored,
    Selected,
    SelectionCleared,
    MoveRequested,
    JumpRequested,
};

/// Result of one click or jump. move_result is only meaningful when
/// outcome is MoveRequested or JumpRequested.
struct ControllerResult {
    ClickOutcome outcome;
    std::optional<kfc::model::MoveResult> move_result;
};

/// Translates pixel clicks into game commands. Never decides chess legality;
/// tracks the current selection and asks IMoveRequester to validate moves.
class Controller {
public:
    /// board and move_requester must outlive this Controller. controlled_color
    /// restricts which pieces this client may pick up (nullopt for local
    /// hot-seat play); the server still authoritatively re-checks ownership.
    Controller(const kfc::model::Board& board, kfc::model::IMoveRequester& move_requester, BoardMapper board_mapper,
               std::optional<kfc::model::PieceColor> controlled_color = std::nullopt);

    /// Handles one click at pixel (x, y). See ClickOutcome for the selection
    /// policy: clicking another same-color piece reselects rather than moves;
    /// clicking elsewhere while selected clears selection and requests a move.
    ControllerResult click(int x, int y);

    /// Handles a jump-in-place request at pixel (x, y). Unlike click, never
    /// reads or modifies the current selection.
    ControllerResult jump(int x, int y);

    /// The currently selected cell, or std::nullopt if nothing is selected or
    /// the selected piece has since moved away or been captured.
    std::optional<kfc::model::Position> selected_cell() const;

private:
    /// True if the piece at selected_cell_ is no longer the one selected
    /// (moved away or captured); a stale selection must never be reused.
    bool selection_is_stale() const;
    void clear_selection();

    /// Always true in local play (controlled_color_ is nullopt), otherwise
    /// only for the client's own color.
    bool can_control(kfc::model::PieceColor piece_color) const;

    const kfc::model::Board& board_;
    kfc::model::IMoveRequester& move_requester_;
    BoardMapper board_mapper_;
    std::optional<kfc::model::PieceColor> controlled_color_;
    std::optional<kfc::model::Position> selected_cell_;
    std::optional<kfc::model::PieceId> selected_piece_id_;
};

}  // namespace kfc::input
