#include <gtest/gtest.h>

#include "kfc/realtime/jump_cooldown_policy.hpp"
#include "kfc/realtime/motion_factory.hpp"
#include "kfc/realtime/piece_speed_provider.hpp"
#include "kfc/realtime/standard_cooldown_policy.hpp"

using namespace kfc::model;

namespace {
Piece make_piece(PieceKind kind, int id, PieceColor color, Position cell) {
    return Piece{PieceId{id}, color, kind, cell, PieceState::Idle};
}

/// A speed provider a test controls completely, independent of whatever
/// FixedPieceSpeedProvider's own numbers happen to be.
class StubSpeedProvider : public IPieceSpeedProvider {
public:
    explicit StubSpeedProvider(double speed) : speed_(speed) {}
    double speed_m_per_sec(PieceKind) const override {
        return speed_;
    }

private:
    double speed_;
};
}  // namespace

// --- FixedPieceSpeedProvider ---

TEST(FixedPieceSpeedProviderTest, EveryOrdinaryKindGetsTheSameSpeed) {
    FixedPieceSpeedProvider provider;

    EXPECT_EQ(provider.speed_m_per_sec(PieceKind::Pawn), provider.speed_m_per_sec(PieceKind::Queen));
    EXPECT_EQ(provider.speed_m_per_sec(PieceKind::Rook), provider.speed_m_per_sec(PieceKind::King));
}

TEST(FixedPieceSpeedProviderTest, DroneIsSlowerThanEveryOtherKind) {
    FixedPieceSpeedProvider provider;

    EXPECT_LT(provider.speed_m_per_sec(PieceKind::Drone), provider.speed_m_per_sec(PieceKind::Rook));
}

// --- MotionFactory, defaults (backward compatibility) ---

TEST(MotionFactoryDefaultsTest, ReproduceTheOldHardcoded1000MsPerCellStep) {
    StandardCooldownPolicy standard_policy;
    JumpCooldownPolicy jump_policy;
    MotionFactory factory(standard_policy, jump_policy);
    Piece rook = make_piece(PieceKind::Rook, 1, PieceColor::White, Position{4, 4});

    Motion motion = factory.create_move(rook, Position{4, 4}, Position{4, 6});

    EXPECT_EQ(motion.duration_ms, 2000);
}

TEST(MotionFactoryDefaultsTest, ReproduceTheOldHardcoded1500MsPerCellStepForDrone) {
    StandardCooldownPolicy standard_policy;
    JumpCooldownPolicy jump_policy;
    MotionFactory factory(standard_policy, jump_policy);
    Piece drone = make_piece(PieceKind::Drone, 1, PieceColor::White, Position{4, 4});

    Motion motion = factory.create_move(drone, Position{4, 4}, Position{4, 5});

    EXPECT_EQ(motion.duration_ms, 1500);
}

// --- MotionFactory, an injected speed provider ---

TEST(MotionFactorySpeedProviderTest, DurationIsCellStepsTimesMetersPerCellDividedBySpeed) {
    StandardCooldownPolicy standard_policy;
    JumpCooldownPolicy jump_policy;
    StubSpeedProvider speed_provider(2.0);
    // 2 cell-steps * 1.0 meters/cell / 2.0 m/s = 1s/step * 2 steps = 1000ms.
    MotionFactory factory(standard_policy, jump_policy, speed_provider, /*meters_per_cell=*/1.0);
    Piece rook = make_piece(PieceKind::Rook, 1, PieceColor::White, Position{4, 4});

    Motion motion = factory.create_move(rook, Position{4, 4}, Position{4, 6});

    EXPECT_EQ(motion.duration_ms, 1000);
}

TEST(MotionFactorySpeedProviderTest, ASmallerMetersPerCellMakesMovementFaster) {
    StandardCooldownPolicy standard_policy;
    JumpCooldownPolicy jump_policy;
    StubSpeedProvider fast_provider(1.5);
    MotionFactory fast_factory(standard_policy, jump_policy, fast_provider, /*meters_per_cell=*/0.5);
    MotionFactory slow_factory(standard_policy, jump_policy, fast_provider, /*meters_per_cell=*/1.5);
    Piece rook = make_piece(PieceKind::Rook, 1, PieceColor::White, Position{4, 4});

    Motion fast_motion = fast_factory.create_move(rook, Position{4, 4}, Position{4, 5});
    Motion slow_motion = slow_factory.create_move(rook, Position{4, 4}, Position{4, 5});

    EXPECT_LT(fast_motion.duration_ms, slow_motion.duration_ms);
}
