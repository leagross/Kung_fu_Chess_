#include <gtest/gtest.h>

#include "kfc/realtime/collision_resolver.hpp"

using namespace kfc::model;

namespace {
Piece make_piece(int id, PieceColor color, Position cell) {
    return Piece{PieceId{id}, color, PieceKind::Rook, cell, PieceState::Idle};
}
}  // namespace

TEST(CollisionResolverTest, VacatedCellWhenDestinationIsEmpty) {
    Piece mover = make_piece(1, PieceColor::White, Position{4, 4});

    CollisionResult result = CollisionResolver::resolve(mover, std::nullopt);

    EXPECT_EQ(result.kind, CollisionKind::VacatedCell);
    EXPECT_FALSE(result.captured_piece.has_value());
}

TEST(CollisionResolverTest, VacatedCellWhenTheOnlyOccupantIsTheMoverItself) {
    Piece mover = make_piece(1, PieceColor::White, Position{4, 4});

    CollisionResult result = CollisionResolver::resolve(mover, mover);

    EXPECT_EQ(result.kind, CollisionKind::VacatedCell);
}

TEST(CollisionResolverTest, EnemyCapturedWhenOccupantIsTheOppositeColor) {
    Piece mover = make_piece(1, PieceColor::White, Position{4, 4});
    Piece occupant = make_piece(2, PieceColor::Black, Position{4, 5});

    CollisionResult result = CollisionResolver::resolve(mover, occupant);

    ASSERT_EQ(result.kind, CollisionKind::EnemyCaptured);
    ASSERT_TRUE(result.captured_piece.has_value());
    EXPECT_EQ(result.captured_piece->id, occupant.id);
    EXPECT_EQ(result.captured_piece->state, PieceState::Captured);
}

TEST(CollisionResolverTest, FriendlyBlockedWhenOccupantIsTheSameColorButADifferentPiece) {
    Piece mover = make_piece(1, PieceColor::White, Position{4, 4});
    Piece ally = make_piece(2, PieceColor::White, Position{4, 5});

    CollisionResult result = CollisionResolver::resolve(mover, ally);

    EXPECT_EQ(result.kind, CollisionKind::FriendlyBlocked);
    EXPECT_FALSE(result.captured_piece.has_value());
}

TEST(CollisionResolverTest, PassesThroughAnEnemyOccupantThatIsAirborne) {
    Piece mover = make_piece(1, PieceColor::White, Position{4, 4});
    Piece jumper = make_piece(2, PieceColor::Black, Position{4, 5});
    jumper.state = PieceState::Airborne;

    CollisionResult result = CollisionResolver::resolve(mover, jumper);

    EXPECT_EQ(result.kind, CollisionKind::PassedThroughAirborne);
    EXPECT_FALSE(result.captured_piece.has_value());
}

TEST(CollisionResolverTest, FriendlyOccupantBlocksEvenWhenAirborne) {
    // Pass-through is only for an *enemy* mid-jump (you cannot capture in the
    // air). A friendly airborne piece must still block -- otherwise the mover
    // would take its cell and the jumper would vanish on landing. See
    // JumpFriendlyBlockTest for the end-to-end consequence.
    Piece mover = make_piece(1, PieceColor::White, Position{4, 4});
    Piece jumper = make_piece(2, PieceColor::White, Position{4, 5});
    jumper.state = PieceState::Airborne;

    CollisionResult result = CollisionResolver::resolve(mover, jumper);

    EXPECT_EQ(result.kind, CollisionKind::FriendlyBlocked);
    EXPECT_FALSE(result.captured_piece.has_value());
}
