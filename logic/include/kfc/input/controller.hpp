#pragma once

#include <optional>

#include "../../kfc/model/board.hpp"
#include "../../kfc/engine/move_requester.hpp"
#include "../../kfc/engine/move_result.hpp"
#include "../../kfc/input/board_mapper.hpp"
#include "../../kfc/model/position.hpp"

namespace kfc::input {

/// Only MoveRequested/JumpRequested touch IMoveRequester and populate move_result.
enum class ClickOutcome {
    Ignored,
    Selected,
    SelectionCleared,
    MoveRequested,
    JumpRequested,
};

struct ControllerResult {
    ClickOutcome outcome;
    std::optional<kfc::model::MoveResult> move_result;
};

/// Translates pixel clicks into game commands; never decides chess legality itself.
class Controller {
public:
    /// controlled_color restricts which pieces this client may pick up (nullopt for local hot-seat play).
    Controller(const kfc::model::Board& board, kfc::model::IMoveRequester& move_requester, BoardMapper board_mapper,
               std::optional<kfc::model::PieceColor> controlled_color = std::nullopt);

    /// Clicking another same-color piece reselects; clicking elsewhere while selected requests a move.
    ControllerResult click(int x, int y);

    /// Unlike click, never reads or modifies the current selection.
    ControllerResult jump(int x, int y);

    std::optional<kfc::model::Position> selected_cell() const;

private:
    bool selection_is_stale() const;
    void clear_selection();
    bool can_control(kfc::model::PieceColor piece_color) const;

    const kfc::model::Board& board_;
    kfc::model::IMoveRequester& move_requester_;
    BoardMapper board_mapper_;
    std::optional<kfc::model::PieceColor> controlled_color_;
    std::optional<kfc::model::Position> selected_cell_;
    std::optional<kfc::model::PieceId> selected_piece_id_;
};

}  // namespace kfc::input
