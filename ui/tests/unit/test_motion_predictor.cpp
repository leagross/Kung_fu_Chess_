#include <gtest/gtest.h>

#include <chrono>

#include "kfc/graphics/net/motion_predictor.hpp"

using kfc::graphics::net::MotionPredictor;
using namespace kfc::model;
using Clock = std::chrono::steady_clock;

namespace {

Motion make_motion(int piece_id, int duration_ms, int elapsed_ms = 0) {
    Piece piece{PieceId{piece_id}, PieceColor::White, PieceKind::Pawn, Position{0, 0}, PieceState::Idle};
    return Motion{piece, Position{0, 0}, Position{0, 1}, MotionKind::Move, duration_ms, elapsed_ms, 0};
}

}  // namespace

TEST(MotionPredictorTest, UntrackedPieceHasNoMotion) {
    MotionPredictor predictor;

    EXPECT_FALSE(predictor.motion_for(PieceId{1}).has_value());
    EXPECT_FALSE(predictor.is_tracked(PieceId{1}));
}

TEST(MotionPredictorTest, StartTracksThePiece) {
    MotionPredictor predictor;

    predictor.start(make_motion(1, 1000), Clock::now());

    EXPECT_TRUE(predictor.is_tracked(PieceId{1}));
    ASSERT_TRUE(predictor.motion_for(PieceId{1}).has_value());
}

TEST(MotionPredictorTest, TickAdvancesElapsedFromStartedAt) {
    MotionPredictor predictor;
    Clock::time_point start = Clock::now();
    predictor.start(make_motion(1, 1000), start);

    predictor.tick(start + std::chrono::milliseconds(300));

    ASSERT_TRUE(predictor.motion_for(PieceId{1}).has_value());
    EXPECT_EQ(predictor.motion_for(PieceId{1})->elapsed_ms, 300);
}

TEST(MotionPredictorTest, TickClampsElapsedToDuration) {
    MotionPredictor predictor;
    Clock::time_point start = Clock::now();
    predictor.start(make_motion(1, 1000), start);

    predictor.tick(start + std::chrono::milliseconds(5000));

    ASSERT_TRUE(predictor.motion_for(PieceId{1}).has_value());
    EXPECT_EQ(predictor.motion_for(PieceId{1})->elapsed_ms, 1000);
}

TEST(MotionPredictorTest, DiscardStopsTrackingThatPiece) {
    MotionPredictor predictor;
    predictor.start(make_motion(1, 1000), Clock::now());

    predictor.discard(PieceId{1});

    EXPECT_FALSE(predictor.is_tracked(PieceId{1}));
    EXPECT_FALSE(predictor.motion_for(PieceId{1}).has_value());
}

TEST(MotionPredictorTest, DiscardOfAnUntrackedPieceIsANoOp) {
    MotionPredictor predictor;
    EXPECT_NO_THROW(predictor.discard(PieceId{42}));
}

TEST(MotionPredictorTest, TrackingTwoPiecesKeepsThemIndependent) {
    MotionPredictor predictor;
    Clock::time_point start = Clock::now();
    predictor.start(make_motion(1, 1000), start);
    predictor.start(make_motion(2, 2000), start);

    predictor.discard(PieceId{1});

    EXPECT_FALSE(predictor.is_tracked(PieceId{1}));
    EXPECT_TRUE(predictor.is_tracked(PieceId{2}));
}
