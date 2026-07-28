#pragma once

#include <unordered_map>

#include "kfc/graphics/animation/piece_animator.hpp"
#include "kfc/graphics/animation/piece_asset_library.hpp"
#include "kfc/model/piece.hpp"
#include "kfc/texttests/game_view.hpp"

namespace kfc::graphics {

/// Owns one PieceAnimator per currently-live piece, keyed by PieceId --
/// the class that makes simultaneous per-piece animation actually happen:
/// every live piece's animator advances independently, mirroring how
/// RealTimeArbiter already tracks each piece's Motion independently on the
/// backend side. Lifecycle only (create on first sight, advance, prune on
/// capture) -- drawing is a separate concern, not this class's job.
class PieceAnimatorRegistry {
public:
    /// asset_library must outlive this PieceAnimatorRegistry.
    explicit PieceAnimatorRegistry(const PieceAssetLibrary& asset_library);

    /// Advances every live piece's animator by ms: creates one (from
    /// asset_library) for any PieceId seen for the first time, advances
    /// every animator using game's current board/Motion state for that
    /// piece, and discards any animator whose piece is no longer on the
    /// board (captured). Also rebuilds an animator whose piece's PieceKind
    /// changed since it was created -- same PieceId, different kind means a
    /// pawn just promoted, and its old (pawn) AnimationSet no longer
    /// matches what it now is. A piece kind asset_library has no art for is
    /// skipped with a stderr warning, the same way the old PieceRenderer did
    /// for plain idle sprites.
    void advance(int ms, const kfc::texttests::IGameView& game);

    /// Read-only access to the live animators, for whatever draws them.
    const std::unordered_map<kfc::model::PieceId, PieceAnimator>& animators() const {
        return animators_;
    }

private:
    const PieceAssetLibrary& asset_library_;
    std::unordered_map<kfc::model::PieceId, PieceAnimator> animators_;
    /// Which PieceKind each animators_ entry was built for -- the only way
    /// to detect a promotion, since a PieceId never changes but its kind can.
    std::unordered_map<kfc::model::PieceId, kfc::model::PieceKind> animator_kinds_;
};

}  // namespace kfc::graphics
