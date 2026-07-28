#pragma once

#include <optional>

#include "../../kfc/model/board.hpp"
#include "../../kfc/engine/move_requester.hpp"
#include "../../kfc/engine/move_result.hpp"
#include "../../kfc/input/board_mapper.hpp"
#include "../../kfc/model/position.hpp"

namespace kfc::input {

/// What a single Controller::click or Controller::jump call did. Selected/
/// SelectionCleared/Ignored never touch IMoveRequester; only MoveRequested
/// (from click) and JumpRequested (from jump) do, and only then is
/// move_result populated.
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

/// Translates pixel clicks into game commands. Never decides chess legality
/// and never touches Board or RuleEngine directly -- it only tracks which
/// cell is currently selected and, on a second in-board click, asks its
/// IMoveRequester (GameEngine locally, or a networked ServerLink) to
/// validate and (if legal) start the move.
class Controller {
public:
    /// board and move_requester must outlive this Controller. controlled_color
    /// restricts which pieces this client may pick up: pass a color for
    /// networked play (a player only commands their own side, so clicking an
    /// opponent's piece is ignored locally rather than sent and bounced back
    /// as "not_your_piece"), or std::nullopt for local hot-seat play, where
    /// one person legitimately moves both colors. This is a UX convenience
    /// only -- the server still authoritatively re-checks ownership.
    Controller(const kfc::model::Board& board, kfc::model::IMoveRequester& move_requester, BoardMapper board_mapper,
               std::optional<kfc::model::PieceColor> controlled_color = std::nullopt);

    /// Handles one click at pixel (x, y). See ClickOutcome for the exact
    /// selection policy: an out-of-board click is ignored while nothing is
    /// selected, and cancels the current selection (without calling
    /// IMoveRequester) while something is selected. An in-board click on an
    /// empty cell with nothing selected is ignored. An in-board click while
    /// something is selected either replaces the selection (if the clicked
    /// cell holds another piece of the same color -- moving onto your own
    /// piece is never legal, so this is a reselect, not a move) or clears
    /// the selection and requests a move (for an empty cell or an enemy
    /// piece), whether or not that move turns out to be legal.
    ControllerResult click(int x, int y);

    /// Handles a jump-in-place request at pixel (x, y). Unlike click, this
    /// is a single-step command with no selection: it never reads or
    /// modifies the current selection, and an out-of-board pixel is simply
    /// ignored (ClickOutcome::Ignored, no call to IMoveRequester). An in-board
    /// pixel always requests a jump via IMoveRequester::request_jump, whether
    /// or not that jump turns out to be legal.
    ControllerResult jump(int x, int y);

    /// The currently selected cell, or std::nullopt if nothing is selected
    /// -- also std::nullopt if the selected piece has since moved away or
    /// been captured (a stale selection is never reported as live; see
    /// selection_is_stale). Exposed for tests and for UI highlighting.
    std::optional<kfc::model::Position> selected_cell() const;

private:
    /// True if a selection exists but the piece actually at selected_cell_
    /// now (if any) is not the one that was selected -- e.g. it moved away
    /// in real time, or was captured and something else (or nothing) now
    /// occupies that cell. A stale selection must never be used to move
    /// whatever piece happens to be sitting there when the next click lands.
    bool selection_is_stale() const;
    void clear_selection();

    /// True if this client is allowed to pick up a piece of piece_color --
    /// always true in local play (controlled_color_ is nullopt), otherwise
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
