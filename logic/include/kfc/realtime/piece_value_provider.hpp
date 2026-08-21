#pragma once

#include "../../kfc/model/piece.hpp"

namespace kfc::model {

/// Strategy interface: the material value of a captured piece kind, used by
/// ScoreObserver. Mirrors IPieceSpeedProvider, so values are tunable
/// gameplay data instead of a constant needing a recompile to change.
class IPieceValueProvider {
public:
    virtual ~IPieceValueProvider() = default;

    /// Points awarded to the side that captures a piece of this kind.
    [[nodiscard]] virtual int value_of(PieceKind kind) const = 0;
};

/// Standard chess material values: pawn 1, knight/bishop 3, rook 5, queen 9,
/// king 0 (capturing a king ends the game rather than scoring it), Drone 1.
class StandardPieceValueProvider : public IPieceValueProvider {
public:
    [[nodiscard]] int value_of(PieceKind kind) const override;
};

/// The default every ScoreObserver uses unless handed another.
extern const StandardPieceValueProvider kDefaultPieceValueProvider;

}  // namespace kfc::model
