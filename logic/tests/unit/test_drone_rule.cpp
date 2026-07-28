#include <gtest/gtest.h>

#include "kfc/model/board.hpp"
#include "kfc/realtime/jump_cooldown_policy.hpp"
#include "kfc/realtime/motion_factory.hpp"
#include "kfc/realtime/standard_cooldown_policy.hpp"
#include "kfc/rules/drone_rule.hpp"

using namespace kfc::model;

namespace {

Piece make_piece(PieceKind kind, int id, PieceColor color, Position cell) {
    return Piece{PieceId{id}, color, kind, cell, PieceState::Idle};
}

bool contains(const std::vector<Position>& destinations, Position target) {
    for (const Position& destination : destinations) {
        if (destination == target) {
            return true;
        }
    }
    return false;
}

}  // namespace

TEST(DroneRuleTest, CanMoveOneOrTwoCellsAlongACardinalDirection) {
    Board board(8, 8);
    Position start{4, 4};
    Piece drone = make_piece(PieceKind::Drone, 1, PieceColor::White, start);
    board.add_piece(drone);
    DroneRule rule;

    std::vector<Position> destinations = rule.legal_destinations(board, drone);

    EXPECT_TRUE(contains(destinations, Position{3, 4}));
    EXPECT_TRUE(contains(destinations, Position{2, 4}));
    EXPECT_TRUE(contains(destinations, Position{4, 6}));
}

TEST(DroneRuleTest, CannotMoveDiagonally) {
    Board board(8, 8);
    Position start{4, 4};
    Piece drone = make_piece(PieceKind::Drone, 1, PieceColor::White, start);
    board.add_piece(drone);
    DroneRule rule;

    std::vector<Position> destinations = rule.legal_destinations(board, drone);

    EXPECT_FALSE(contains(destinations, Position{6, 6}));
    EXPECT_FALSE(contains(destinations, Position{5, 5}));
}

TEST(DroneRuleTest, CannotMoveThreeCellsInAStraightLine) {
    Board board(8, 8);
    Position start{4, 4};
    Piece drone = make_piece(PieceKind::Drone, 1, PieceColor::White, start);
    board.add_piece(drone);
    DroneRule rule;

    std::vector<Position> destinations = rule.legal_destinations(board, drone);

    EXPECT_FALSE(contains(destinations, Position{1, 4}));
    EXPECT_FALSE(contains(destinations, Position{4, 7}));
}

TEST(DroneRuleTest, JumpsOverABlockerToReachTwoCellsAway) {
    Board board(8, 8);
    Position start{4, 4};
    Position blocker{3, 4};
    Piece drone = make_piece(PieceKind::Drone, 1, PieceColor::White, start);
    board.add_piece(drone);
    board.add_piece(make_piece(PieceKind::Pawn, 2, PieceColor::Black, blocker));
    DroneRule rule;

    std::vector<Position> destinations = rule.legal_destinations(board, drone);

    EXPECT_TRUE(contains(destinations, Position{2, 4}));
}

TEST(DroneRuleTest, CannotLandOnAFriendlyPiece) {
    Board board(8, 8);
    Position start{4, 4};
    Position friendly{4, 5};
    Piece drone = make_piece(PieceKind::Drone, 1, PieceColor::White, start);
    board.add_piece(drone);
    board.add_piece(make_piece(PieceKind::Pawn, 2, PieceColor::White, friendly));
    DroneRule rule;

    std::vector<Position> destinations = rule.legal_destinations(board, drone);

    EXPECT_FALSE(contains(destinations, friendly));
}

TEST(DroneRuleTest, CapturesAnEnemyPieceAtRange) {
    Board board(8, 8);
    Position start{4, 4};
    Position enemy{4, 6};
    Piece drone = make_piece(PieceKind::Drone, 1, PieceColor::White, start);
    board.add_piece(drone);
    board.add_piece(make_piece(PieceKind::Pawn, 2, PieceColor::Black, enemy));
    DroneRule rule;

    std::vector<Position> destinations = rule.legal_destinations(board, drone);

    EXPECT_TRUE(contains(destinations, enemy));
}

TEST(MotionFactoryDroneTest, CreateMoveIsSlowerForADroneThanForAnEquivalentMove) {
    StandardCooldownPolicy standard_policy;
    JumpCooldownPolicy jump_policy;
    MotionFactory factory(standard_policy, jump_policy);
    Position start{4, 4};
    Position destination{4, 5};
    Piece drone = make_piece(PieceKind::Drone, 1, PieceColor::White, start);
    Piece rook = make_piece(PieceKind::Rook, 2, PieceColor::White, start);

    Motion drone_motion = factory.create_move(drone, start, destination);
    Motion rook_motion = factory.create_move(rook, start, destination);

    EXPECT_GT(drone_motion.duration_ms, rook_motion.duration_ms);
}
