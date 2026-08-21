#include "../../../../include/kfc/graphics/animation/piece_animator_registry.hpp"

#include <iostream>
#include <optional>
#include <unordered_set>

#include "kfc/model/board.hpp"

namespace kfc::graphics {

PieceAnimatorRegistry::PieceAnimatorRegistry(const PieceAssetLibrary& asset_library)
    : asset_library_(asset_library) {}

void PieceAnimatorRegistry::advance(int ms, const kfc::texttests::IGameView& game) {
    const kfc::model::Board& board = game.board();
    std::unordered_set<kfc::model::PieceId> seen;

    for (int row = 0; row < board.height(); ++row) {
        for (int col = 0; col < board.width(); ++col) {
            std::optional<kfc::model::Piece> piece = board.piece_at(kfc::model::Position{row, col});
            if (!piece.has_value()) {
                continue;
            }
            seen.insert(piece->id);

            auto animator = animators_.find(piece->id);
            bool kind_changed = animator != animators_.end() && animator_kinds_.at(piece->id) != piece->kind;
            if (animator != animators_.end() && kind_changed) {
                // Same PieceId, different kind: a pawn promoted. Discard so
                // the block below rebuilds against the right AnimationSet.
                animators_.erase(animator);
                animator = animators_.end();
            }

            if (animator == animators_.end()) {
                try {
                    const PieceAnimationSet& set = asset_library_.animation_set_for(piece->kind, piece->color);
                    animator = animators_.emplace(piece->id, PieceAnimator(set)).first;
                    animator_kinds_[piece->id] = piece->kind;
                } catch (const std::exception& e) {
                    std::cerr << "Skipping animator for piece at " << kfc::model::to_string(piece->cell) << ": "
                              << e.what() << "\n";
                    continue;
                }
            }

            animator->second.advance(ms, piece->cell, game.motion_for(piece->id));
        }
    }

    for (auto it = animators_.begin(); it != animators_.end();) {
        // A piece missing from the board but still busy (e.g. an Airborne
        // defender an attacker's Move provisionally occupies, per
        // RealTimeArbiter::resolve_arrival) is mid-jump, not captured; keep
        // its animator so it resumes once it reappears instead of resetting.
        if (seen.count(it->first) == 0 && !game.is_piece_busy(it->first)) {
            animator_kinds_.erase(it->first);
            it = animators_.erase(it);
        } else {
            ++it;
        }
    }
}

}  // namespace kfc::graphics
