#pragma once

#include <filesystem>
#include <optional>

#include "kfc/graphics/animation/piece_animation_set.hpp"
#include "kfc/graphics/geometry/board_geometry.hpp"
#include "kfc/model/position.hpp"
#include "kfc/realtime/motion.hpp"

namespace kfc::graphics {

/// One piece's presentation state machine; carries no game rules.
class PieceAnimator {
public:
    /// animation_set must outlive this PieceAnimator.
    explicit PieceAnimator(const PieceAnimationSet& animation_set);

    /// A Motion present last tick and gone this tick reads as "just arrived" and triggers a transition.
    void advance(int ms, const kfc::model::Position& board_cell, const std::optional<kfc::model::Motion>& motion);

    /// Interpolated between Motion's source/destination while Moving, else the stationary board cell.
    PixelPoint pixel_position() const;

    std::filesystem::path current_sprite_path() const;

    /// std::nullopt outside a rest state. Presentation-only, unrelated to the real cooldown policy.
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
