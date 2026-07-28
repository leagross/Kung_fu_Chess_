#pragma once

#include "../../kfc/model/piece.hpp"

namespace kfc::model {

/// Strategy interface: the material value of a captured piece kind, used by
/// ScoreObserver. Mirrors IPieceSpeedProvider exactly -- the backend defines
/// the interface plus a fixed default here, while a config.json-driven
/// implementation can live in the graphics layer, so piece values become
/// tunable gameplay data instead of a constant that needs a recompile to
/// change (e.g. making a Drone worth 2 instead of 1).
class IPieceValueProvider {
public:
    virtual ~IPieceValueProvider() = default;

    /// Points awarded to the side that captures a piece of this kind.
    [[nodiscard]] virtual int value_of(PieceKind kind) const = 0;
};

/// The standard chess material values ScoreObserver used to hardcode: pawn 1,
/// knight/bishop 3, rook 5, queen 9, king 0 (capturing a king ends the game,
/// GameEngine::is_game_over, rather than scoring it), and Drone 1.
class StandardPieceValueProvider : public IPieceValueProvider {
public:
    [[nodiscard]] int value_of(PieceKind kind) const override;
};

/// The default every ScoreObserver uses unless handed another. Has static
/// lifetime so ScoreObserver can hold it by reference -- mirrors
/// kDefaultPieceSpeedProvider in motion_factory.hpp.
extern const StandardPieceValueProvider kDefaultPieceValueProvider;

}  // namespace kfc::model
