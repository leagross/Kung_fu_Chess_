#include "../../../include/kfc/realtime/piece_value_provider.hpp"

namespace kfc::model {

const StandardPieceValueProvider kDefaultPieceValueProvider;

int StandardPieceValueProvider::value_of(PieceKind kind) const {
    switch (kind) {
        case PieceKind::Pawn:
            return 1;
        case PieceKind::Knight:
        case PieceKind::Bishop:
            return 3;
        case PieceKind::Rook:
            return 5;
        case PieceKind::Queen:
            return 9;
        case PieceKind::King:
            return 0;
        case PieceKind::Drone:
            return 1;
    }
    return 0;
}

}  // namespace kfc::model
