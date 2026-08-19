#include "kfc/graphics/net/motion_predictor.hpp"

#include <algorithm>

namespace kfc::graphics::net {

void MotionPredictor::start(const kfc::model::Motion& motion, std::chrono::steady_clock::time_point started_at) {
    predictions_[motion.moving_piece.id] = Entry{motion, started_at};
}

void MotionPredictor::discard(kfc::model::PieceId piece_id) {
    predictions_.erase(piece_id);
}

void MotionPredictor::tick(std::chrono::steady_clock::time_point now) {
    for (auto& [id, entry] : predictions_) {
        int real_elapsed_ms =
            static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(now - entry.started_at).count());
        entry.motion.elapsed_ms = std::clamp(real_elapsed_ms, 0, entry.motion.duration_ms);
    }
}

std::optional<kfc::model::Motion> MotionPredictor::motion_for(kfc::model::PieceId piece_id) const {
    auto it = predictions_.find(piece_id);
    if (it == predictions_.end()) {
        return std::nullopt;
    }
    return it->second.motion;
}

bool MotionPredictor::is_tracked(kfc::model::PieceId piece_id) const {
    return predictions_.count(piece_id) > 0;
}

}  // namespace kfc::graphics::net
