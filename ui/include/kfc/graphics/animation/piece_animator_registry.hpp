#pragma once

#include <unordered_map>

#include "kfc/graphics/animation/piece_animator.hpp"
#include "kfc/graphics/animation/piece_asset_library.hpp"
#include "kfc/model/piece.hpp"
#include "kfc/texttests/game_view.hpp"

namespace kfc::graphics {

/// Owns one PieceAnimator per currently-live piece, keyed by PieceId.
/// Lifecycle only (create on first sight, advance, prune on capture) --
/// drawing is a separate concern.
class PieceAnimatorRegistry {
public:
    /// asset_library must outlive this PieceAnimatorRegistry.
    explicit PieceAnimatorRegistry(const PieceAssetLibrary& asset_library);

    /// Creates an animator for any new PieceId, advances all, discards
    /// captured ones, and rebuilds one whose PieceKind changed (promotion).
    /// A kind with no art in asset_library is skipped with a stderr warning.
    void advance(int ms, const kfc::texttests::IGameView& game);

    /// Read-only access to the live animators, for whatever draws them.
    const std::unordered_map<kfc::model::PieceId, PieceAnimator>& animators() const {
        return animators_;
    }

private:
    const PieceAssetLibrary& asset_library_;
    std::unordered_map<kfc::model::PieceId, PieceAnimator> animators_;
    /// Which PieceKind each animator was built for, to detect promotions.
    std::unordered_map<kfc::model::PieceId, kfc::model::PieceKind> animator_kinds_;
};

}  // namespace kfc::graphics
