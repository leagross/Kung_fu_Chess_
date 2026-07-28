#include "../../../include/kfc/realtime/piece_speed_provider.hpp"

namespace kfc::model {

double FixedPieceSpeedProvider::speed_m_per_sec(PieceKind kind) const {
    return kind == PieceKind::Drone ? 1.0 : 1.5;
}

}  // namespace kfc::model
