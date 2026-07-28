#include "../../../../include/kfc/graphics/animation/piece_animator.hpp"

#include <algorithm>

#include "kfc/graphics/constants.hpp"

namespace kfc::graphics {

PieceAnimator::PieceAnimator(const PieceAnimationSet& animation_set) : animation_set_(animation_set) {}

void PieceAnimator::transition_to(PieceStateName state) {
    current_state_ = state;
    elapsed_in_state_ms_ = 0;
}

void PieceAnimator::advance(int ms, const kfc::model::Position& board_cell,
                             const std::optional<kfc::model::Motion>& motion) {
    static_cell_ = board_cell;
    bool had_motion = current_motion_.has_value();
    current_motion_ = motion;

    if (motion.has_value()) {
        PieceStateName desired =
            motion->kind == kfc::model::MotionKind::Move ? PieceStateName::Move : PieceStateName::Jump;
        if (current_state_ != desired) {
            transition_to(desired);
        }
        elapsed_in_state_ms_ = motion->elapsed_ms;
        return;
    }

    if (had_motion && (current_state_ == PieceStateName::Move || current_state_ == PieceStateName::Jump)) {
        // The Motion that was in flight last tick is gone this tick -- it
        // arrived. Move on to whichever state that clip's own config names,
        // regardless of the backend's cooldown timing (StandardCooldownPolicy
        // is currently 0ms -- there may be no "busy but no Motion" tick to
        // catch at all, so detecting the Motion's disappearance directly,
        // not polling is_piece_busy, is what makes this transition reliable
        // no matter how short that cooldown is).
        transition_to(animation_set_.clip(current_state_).next_state_when_finished);
        return;
    }

    // A non-looping clip (a rest state) that has played out its own natural
    // duration moves on to its own next_state_when_finished -- a looping
    // clip (idle) never triggers this on its own.
    if (current_state_ != PieceStateName::Idle) {
        const AnimationClip& clip = animation_set_.clip(current_state_);
        int natural_duration_ms = clip.frame_count * 1000 / clip.frames_per_sec;
        if (!clip.is_loop && elapsed_in_state_ms_ >= natural_duration_ms) {
            transition_to(clip.next_state_when_finished);
            return;
        }
    }
    elapsed_in_state_ms_ += ms;
}

int PieceAnimator::current_frame_index() const {
    const AnimationClip& clip = animation_set_.clip(current_state_);
    if (clip.frame_count <= 0) {
        return 0;
    }
    int raw_index = elapsed_in_state_ms_ * clip.frames_per_sec / 1000;
    if (clip.is_loop) {
        return raw_index % clip.frame_count;
    }
    return std::min(raw_index, clip.frame_count - 1);
}

namespace {
/// Ease-in-out: slow start, fast middle, slow finish -- reads as a natural
/// glide instead of linear motion's constant-speed, faintly mechanical
/// feel. A plain smoothstep (3t^2 - 2t^3), symmetric around t=0.5 and with
/// zero velocity at both endpoints.
double ease_in_out(double t) {
    return t * t * (3.0 - 2.0 * t);
}
}  // namespace

PixelPoint PieceAnimator::pixel_position() const {
    if (current_motion_.has_value() && current_motion_->kind == kfc::model::MotionKind::Move) {
        const kfc::model::Motion& motion = *current_motion_;
        double t = motion.duration_ms > 0 ? static_cast<double>(motion.elapsed_ms) / motion.duration_ms : 1.0;
        t = ease_in_out(std::clamp(t, 0.0, 1.0));

        PixelPoint from = cell_top_left(motion.source);
        PixelPoint to = cell_top_left(motion.destination);
        return PixelPoint{
            from.x + static_cast<int>((to.x - from.x) * t),
            from.y + static_cast<int>((to.y - from.y) * t),
        };
    }
    return cell_top_left(static_cell_);
}

std::filesystem::path PieceAnimator::current_sprite_path() const {
    const AnimationClip& clip = animation_set_.clip(current_state_);
    return clip.sprites_dir / sprite_frame_filename(current_frame_index() + 1);
}

std::optional<double> PieceAnimator::rest_remaining_fraction() const {
    if (current_state_ != PieceStateName::ShortRest && current_state_ != PieceStateName::LongRest) {
        return std::nullopt;
    }

    const AnimationClip& clip = animation_set_.clip(current_state_);
    int natural_duration_ms = clip.frame_count * 1000 / clip.frames_per_sec;
    if (natural_duration_ms <= 0) {
        return 0.0;
    }

    double elapsed_fraction = static_cast<double>(elapsed_in_state_ms_) / natural_duration_ms;
    return std::clamp(1.0 - elapsed_fraction, 0.0, 1.0);
}

}  // namespace kfc::graphics
