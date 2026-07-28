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
                // Same PieceId, different PieceKind: a pawn just promoted.
                // Its AnimationSet (pawn sprites) no longer matches the
                // piece it now is -- discard the animator so the block
                // below rebuilds one against the right set.
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
        // Not found on the board this tick usually means truly captured --
        // erase its animator. But a piece can also go briefly unseen while
        // very much alive: when an attacker's Move resolves against an
        // Airborne defender, the attacker provisionally occupies the
        // defender's cell on Board until the defender's own jump lands and
        // (per CollisionResolver) captures it back -- see
        // RealTimeArbiter::resolve_arrival. During that window the defender
        // has no cell of its own to be found at, even though it is still
        // busy (its jump Motion is still ticking). Erasing its animator
        // here would lose track of the jump entirely, so a fresh Idle
        // animator gets built once it reappears -- the jump's own
        // next_state_when_finished (short_rest) never fires, and whatever
        // the attacker's own animator was doing (e.g. long_rest, once its
        // move resolved) is left rendered at that cell in the meantime.
        // Keeping the animator alive while the piece is still busy avoids
        // this: it simply stops receiving advance() calls until it
        // reappears, then resumes exactly where its own Motion left off.
        if (seen.count(it->first) == 0 && !game.is_piece_busy(it->first)) {
            animator_kinds_.erase(it->first);
            it = animators_.erase(it);
        } else {
            ++it;
        }
    }
}

}  // namespace kfc::graphics
