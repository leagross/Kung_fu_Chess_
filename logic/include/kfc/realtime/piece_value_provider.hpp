#pragma once

#include "../../kfc/model/piece.hpp"

namespace kfc::model {

/// Material value of a captured piece kind, used by ScoreObserver.
class IPieceValueProvider {
public:
    virtual ~IPieceValueProvider() = default;

    [[nodiscard]] virtual int value_of(PieceKind kind) const = 0;
};

/// Pawn 1, knight/bishop 3, rook 5, queen 9, king 0 (ends the game instead), Drone 1.
class StandardPieceValueProvider : public IPieceValueProvider {
public:
    [[nodiscard]] int value_of(PieceKind kind) const override;
};

extern const StandardPieceValueProvider kDefaultPieceValueProvider;

}  // namespace kfc::model
