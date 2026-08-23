#pragma once

#include <unordered_map>

#include "kfc/graphics/animation/piece_animator.hpp"
#include "kfc/graphics/animation/piece_asset_library.hpp"
#include "kfc/model/piece.hpp"
#include "kfc/texttests/game_view.hpp"

namespace kfc::graphics {

/// Owns one PieceAnimator per currently-live piece, keyed by PieceId. Lifecycle only; drawing is separate.
class PieceAnimatorRegistry {
public:
    /// asset_library must outlive this PieceAnimatorRegistry.
    explicit PieceAnimatorRegistry(const PieceAssetLibrary& asset_library);

    /// Also rebuilds an animator whose PieceKind changed (promotion); a kind with no art is skipped.
    void advance(int ms, const kfc::texttests::IGameView& game);

    const std::unordered_map<kfc::model::PieceId, PieceAnimator>& animators() const {
        return animators_;
    }

private:
    const PieceAssetLibrary& asset_library_;
    std::unordered_map<kfc::model::PieceId, PieceAnimator> animators_;
    std::unordered_map<kfc::model::PieceId, kfc::model::PieceKind> animator_kinds_;  // to detect promotions
};

}  // namespace kfc::graphics
