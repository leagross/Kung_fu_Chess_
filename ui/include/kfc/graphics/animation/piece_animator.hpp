#pragma once

#include <filesystem>
#include <optional>

#include "kfc/graphics/animation/piece_animation_set.hpp"
#include "kfc/graphics/geometry/board_geometry.hpp"
#include "kfc/model/position.hpp"
#include "kfc/realtime/motion.hpp"

namespace kfc::graphics {

/// One piece's presentation state machine -- the State pattern the graphics
/// lecture recommended. Owns exactly the state a single animated piece
/// needs: which of the 5 PieceStateName it's currently showing, and how
/// long it's been there. Deliberately does not decide anything about game
/// rules; it only reacts to what the backend already reports (a Motion in
/// flight, or that Motion having just disappeared) and to each state's own
/// next_state_when_finished from its AnimationClip -- the same transition
/// data config.json already encodes, not a second copy of it hand-written
/// into this class.
class PieceAnimator {
public:
    /// animation_set must outlive this PieceAnimator. Starts in Idle.
    explicit PieceAnimator(const PieceAnimationSet& animation_set);

    /// Advances this animator by ms of wall-clock time. Call once per render
    /// tick, per live piece, with that piece's current board cell (used
    /// whenever there is no Motion) and its in-flight Motion if any (from
    /// Game::motion_for). A Motion present last tick and gone this tick is
    /// read as "it just arrived" and triggers a state transition -- this is
    /// checked directly (comparing this tick's Motion to last tick's)
    /// rather than via Game::is_piece_busy, because a very short or even
    /// zero-length cooldown can make "busy but no Motion" too narrow a
    /// window for a render tick to ever land inside it.
    void advance(int ms, const kfc::model::Position& board_cell, const std::optional<kfc::model::Motion>& motion);

    /// Where to draw this piece's current sprite this frame -- interpolated
    /// between the in-flight Motion's source and destination while Moving,
    /// the piece's stationary board cell otherwise.
    PixelPoint pixel_position() const;

    /// Which sprite frame to draw this frame, in the current state.
    std::filesystem::path current_sprite_path() const;

    /// Fraction (0..1) of this rest state's own natural duration still
    /// remaining -- 1.0 the instant a piece enters ShortRest/LongRest,
    /// counting down to 0.0 as its own clip plays out. std::nullopt when
    /// not currently in a rest state (nothing to show a countdown for).
    /// This is presentation-only -- purely how long *this state's own
    /// animation* has left to play, unrelated to StandardCooldownPolicy's
    /// (currently 0ms) real game-rule cooldown.
    std::optional<double> rest_remaining_fraction() const;

private:
    void transition_to(PieceStateName state);
    int current_frame_index() const;

    const PieceAnimationSet& animation_set_;
    PieceStateName current_state_ = PieceStateName::Idle;
    int elapsed_in_state_ms_ = 0;
    kfc::model::Position static_cell_{0, 0};
    std::optional<kfc::model::Motion> current_motion_;
};

}  // namespace kfc::graphics
