#include <gtest/gtest.h>

#include "kfc/realtime/real_time_arbiter.hpp"

using namespace kfc::model;

namespace {

Piece make_rook(int id, PieceColor color, Position cell) {
    return Piece{PieceId{id}, color, PieceKind::Rook, cell, PieceState::Idle};
}

Piece make_pawn(int id, PieceColor color, Position cell) {
    return Piece{PieceId{id}, color, PieceKind::Pawn, cell, PieceState::Idle};
}

Motion make_motion(const Piece& piece, Position source, Position destination, int duration_ms,
                    int cooldown_ms = 0) {
    return Motion{piece, source, destination, MotionKind::Move, duration_ms, 0, cooldown_ms};
}

}  // namespace

TEST(RealTimeArbiterTest, PieceIsNotBusyInitially) {
    Board board(8, 8);
    RealTimeArbiter arbiter(board);

    EXPECT_FALSE(arbiter.is_piece_busy(PieceId{1}));
}

TEST(RealTimeArbiterTest, PieceIsBusyRightAfterStartingAMotion) {
    Board board(8, 8);
    Position source{4, 4};
    Position destination{4, 5};
    Piece rook = make_rook(1, PieceColor::White, source);
    board.add_piece(rook);
    RealTimeArbiter arbiter(board);

    arbiter.start_motion(make_motion(rook, source, destination, 1000));

    EXPECT_TRUE(arbiter.is_piece_busy(rook.id));
}

TEST(RealTimeArbiterTest, AnUnrelatedPieceIsNotBusyWhileAnotherOnesMotionIsActive) {
    Board board(8, 8);
    Position source{4, 4};
    Position destination{4, 5};
    Piece rook = make_rook(1, PieceColor::White, source);
    board.add_piece(rook);
    RealTimeArbiter arbiter(board);
    arbiter.start_motion(make_motion(rook, source, destination, 1000));

    EXPECT_FALSE(arbiter.is_piece_busy(PieceId{99}));
}

TEST(RealTimeArbiterTest, DoesNotArriveOneMillisecondBeforeDuration) {
    Board board(8, 8);
    Position source{4, 4};
    Position destination{4, 5};
    Piece rook = make_rook(1, PieceColor::White, source);
    board.add_piece(rook);
    RealTimeArbiter arbiter(board);
    arbiter.start_motion(make_motion(rook, source, destination, 1000));

    ArrivalEvents events = arbiter.advance_time(999);

    EXPECT_TRUE(events.empty());
    EXPECT_TRUE(arbiter.is_piece_busy(rook.id));
    EXPECT_TRUE(board.piece_at(source).has_value());
    EXPECT_FALSE(board.piece_at(destination).has_value());
}

TEST(RealTimeArbiterTest, ArrivesAfterExactlyItsDuration) {
    Board board(8, 8);
    Position source{4, 4};
    Position destination{4, 5};
    Piece rook = make_rook(1, PieceColor::White, source);
    board.add_piece(rook);
    RealTimeArbiter arbiter(board);
    arbiter.start_motion(make_motion(rook, source, destination, 1000));

    ArrivalEvents events = arbiter.advance_time(1000);

    ASSERT_EQ(events.size(), 1u);
    EXPECT_FALSE(board.piece_at(source).has_value());
    EXPECT_TRUE(board.piece_at(destination).has_value());
}

TEST(RealTimeArbiterTest, PieceIsNoLongerBusyAfterArrivalWithNoCooldown) {
    Board board(8, 8);
    Position source{4, 4};
    Position destination{4, 5};
    Piece rook = make_rook(1, PieceColor::White, source);
    board.add_piece(rook);
    RealTimeArbiter arbiter(board);
    arbiter.start_motion(make_motion(rook, source, destination, 1000, /*cooldown_ms=*/0));

    arbiter.advance_time(1000);

    EXPECT_FALSE(arbiter.is_piece_busy(rook.id));
}

TEST(RealTimeArbiterTest, MotionForReturnsNulloptWhenThePieceIsNotBusy) {
    Board board(8, 8);
    RealTimeArbiter arbiter(board);

    EXPECT_FALSE(arbiter.motion_for(PieceId{1}).has_value());
}

TEST(RealTimeArbiterTest, MotionForReturnsTheInFlightMotion) {
    Board board(8, 8);
    Position source{4, 4};
    Position destination{4, 5};
    Piece rook = make_rook(1, PieceColor::White, source);
    board.add_piece(rook);
    RealTimeArbiter arbiter(board);
    arbiter.start_motion(make_motion(rook, source, destination, 1000));

    std::optional<Motion> motion = arbiter.motion_for(rook.id);

    ASSERT_TRUE(motion.has_value());
    EXPECT_EQ(motion->source, source);
    EXPECT_EQ(motion->destination, destination);
}

// This is exactly the gap PieceAnimator (frontend) used to fall into: a
// zero-cooldown piece becomes "busy" only very briefly, if at all, after
// its Motion resolves -- motion_for must report the Motion gone the moment
// advance_time resolves it, regardless of is_piece_busy's own value.
TEST(RealTimeArbiterTest, MotionForReturnsNulloptOnceTheMotionHasArrivedEvenIfStillInCooldown) {
    Board board(8, 8);
    Position source{4, 4};
    Position destination{4, 5};
    Piece rook = make_rook(1, PieceColor::White, source);
    board.add_piece(rook);
    RealTimeArbiter arbiter(board);
    arbiter.start_motion(make_motion(rook, source, destination, 1000, /*cooldown_ms=*/300));

    arbiter.advance_time(1000);

    EXPECT_TRUE(arbiter.is_piece_busy(rook.id));
    EXPECT_FALSE(arbiter.motion_for(rook.id).has_value());
}

TEST(RealTimeArbiterTest, PieceStaysBusyDuringItsCooldownAfterArrival) {
    Board board(8, 8);
    Position source{4, 4};
    Position destination{4, 5};
    Piece rook = make_rook(1, PieceColor::White, source);
    board.add_piece(rook);
    RealTimeArbiter arbiter(board);
    arbiter.start_motion(make_motion(rook, source, destination, 1000, /*cooldown_ms=*/300));

    arbiter.advance_time(1000);

    EXPECT_TRUE(arbiter.is_piece_busy(rook.id));
}

TEST(RealTimeArbiterTest, PieceIsFreeAgainOnceItsCooldownFullyElapses) {
    Board board(8, 8);
    Position source{4, 4};
    Position destination{4, 5};
    Piece rook = make_rook(1, PieceColor::White, source);
    board.add_piece(rook);
    RealTimeArbiter arbiter(board);
    arbiter.start_motion(make_motion(rook, source, destination, 1000, /*cooldown_ms=*/300));
    arbiter.advance_time(1000);

    arbiter.advance_time(300);

    EXPECT_FALSE(arbiter.is_piece_busy(rook.id));
}

TEST(RealTimeArbiterTest, PartialWaitsAccumulateToTheSameResultAsOneFullWait) {
    Board board(8, 8);
    Position source{4, 4};
    Position destination{4, 5};
    Piece rook = make_rook(1, PieceColor::White, source);
    board.add_piece(rook);
    RealTimeArbiter arbiter(board);
    arbiter.start_motion(make_motion(rook, source, destination, 1000));

    ArrivalEvents first = arbiter.advance_time(500);
    ArrivalEvents second = arbiter.advance_time(500);

    EXPECT_TRUE(first.empty());
    EXPECT_EQ(second.size(), 1u);
    EXPECT_TRUE(board.piece_at(destination).has_value());
}

TEST(RealTimeArbiterTest, ArrivalEventReportsNoCaptureOnAnEmptyDestination) {
    Board board(8, 8);
    Position source{4, 4};
    Position destination{4, 5};
    Piece rook = make_rook(1, PieceColor::White, source);
    board.add_piece(rook);
    RealTimeArbiter arbiter(board);
    arbiter.start_motion(make_motion(rook, source, destination, 1000));

    ArrivalEvents events = arbiter.advance_time(1000);

    ASSERT_EQ(events.size(), 1u);
    EXPECT_FALSE(events[0].captured_piece.has_value());
}

TEST(RealTimeArbiterTest, ArrivalEventReportsTheCapturedPieceWhenDestinationWasOccupied) {
    Board board(8, 8);
    Position source{4, 4};
    Position destination{4, 5};
    Piece rook = make_rook(1, PieceColor::White, source);
    board.add_piece(rook);
    board.add_piece(make_rook(2, PieceColor::Black, destination));
    RealTimeArbiter arbiter(board);
    arbiter.start_motion(make_motion(rook, source, destination, 1000));

    ArrivalEvents events = arbiter.advance_time(1000);

    ASSERT_EQ(events.size(), 1u);
    ASSERT_TRUE(events[0].captured_piece.has_value());
    EXPECT_EQ(events[0].captured_piece->id, PieceId{2});
}

TEST(RealTimeArbiterTest, TwoDifferentPiecesCanHaveActiveMotionsAtTheSameTime) {
    Board board(8, 8);
    Position sourceA{4, 4};
    Position destinationA{4, 5};
    Position sourceB{0, 0};
    Position destinationB{0, 1};
    Piece rookA = make_rook(1, PieceColor::White, sourceA);
    Piece rookB = make_rook(2, PieceColor::White, sourceB);
    board.add_piece(rookA);
    board.add_piece(rookB);
    RealTimeArbiter arbiter(board);

    arbiter.start_motion(make_motion(rookA, sourceA, destinationA, 1000));
    arbiter.start_motion(make_motion(rookB, sourceB, destinationB, 1000));

    EXPECT_TRUE(arbiter.is_piece_busy(rookA.id));
    EXPECT_TRUE(arbiter.is_piece_busy(rookB.id));
}

TEST(RealTimeArbiterTest, AWhitePawnArrivingOnRowZeroIsPromotedToAQueen) {
    Board board(8, 8);
    Position source{1, 4};
    Position destination{0, 4};
    Piece pawn = make_pawn(1, PieceColor::White, source);
    board.add_piece(pawn);
    RealTimeArbiter arbiter(board);
    arbiter.start_motion(make_motion(pawn, source, destination, 1000));

    arbiter.advance_time(1000);

    std::optional<Piece> arrived = board.piece_at(destination);
    ASSERT_TRUE(arrived.has_value());
    EXPECT_EQ(arrived->kind, PieceKind::Queen);
    EXPECT_EQ(arrived->color, PieceColor::White);
}

TEST(RealTimeArbiterTest, ABlackPawnArrivingOnTheLastRowIsPromotedToAQueen) {
    Board board(8, 8);
    Position source{6, 4};
    Position destination{7, 4};
    Piece pawn = make_pawn(1, PieceColor::Black, source);
    board.add_piece(pawn);
    RealTimeArbiter arbiter(board);
    arbiter.start_motion(make_motion(pawn, source, destination, 1000));

    arbiter.advance_time(1000);

    std::optional<Piece> arrived = board.piece_at(destination);
    ASSERT_TRUE(arrived.has_value());
    EXPECT_EQ(arrived->kind, PieceKind::Queen);
}

TEST(RealTimeArbiterTest, WhicheverMotionStartedFirstWinsAHeadOnCollisionAndTheOtherIsNotResurrected) {
    Board board(4, 1);
    Position whiteStart{0, 0};
    Position blackStart{0, 3};
    Piece whiteRook = make_rook(1, PieceColor::White, whiteStart);
    Piece blackRook = make_rook(2, PieceColor::Black, blackStart);
    board.add_piece(whiteRook);
    board.add_piece(blackRook);
    RealTimeArbiter arbiter(board);
    // White's motion is started first (pushed first), matching
    // "enemy_collision_white_started_first": both rooks cross the same
    // 3-cell distance and arrive in the same advance_time call.
    arbiter.start_motion(make_motion(whiteRook, whiteStart, blackStart, 3000));
    arbiter.start_motion(make_motion(blackRook, blackStart, whiteStart, 3000));

    arbiter.advance_time(3000);

    EXPECT_FALSE(board.piece_at(whiteStart).has_value());
    std::optional<Piece> survivor = board.piece_at(blackStart);
    ASSERT_TRUE(survivor.has_value());
    EXPECT_EQ(survivor->id, PieceId{1});  // white -- the one that started first -- wins
}

TEST(RealTimeArbiterTest, APawnArrivingShortOfTheLastRowIsNotPromoted) {
    Board board(8, 8);
    Position source{2, 4};
    Position destination{1, 4};
    Piece pawn = make_pawn(1, PieceColor::White, source);
    board.add_piece(pawn);
    RealTimeArbiter arbiter(board);
    arbiter.start_motion(make_motion(pawn, source, destination, 1000));

    arbiter.advance_time(1000);

    std::optional<Piece> arrived = board.piece_at(destination);
    ASSERT_TRUE(arrived.has_value());
    EXPECT_EQ(arrived->kind, PieceKind::Pawn);
}

TEST(RealTimeArbiterTest, AnOrdinaryMoveMarksThePieceAsHavingMoved) {
    Board board(8, 8);
    Position source{4, 4};
    Position destination{4, 5};
    Piece rook = make_rook(1, PieceColor::White, source);
    board.add_piece(rook);
    RealTimeArbiter arbiter(board);
    arbiter.start_motion(make_motion(rook, source, destination, 1000));

    arbiter.advance_time(1000);

    std::optional<Piece> arrived = board.piece_at(destination);
    ASSERT_TRUE(arrived.has_value());
    EXPECT_TRUE(arrived->has_moved);
}

TEST(RealTimeArbiterTest, AJumpInPlaceDoesNotMarkThePieceAsHavingMoved) {
    Board board(8, 8);
    Position cell{4, 4};
    Piece rook = make_rook(1, PieceColor::White, cell);
    board.add_piece(rook);
    RealTimeArbiter arbiter(board);
    Motion jump{rook, cell, cell, MotionKind::JumpInPlace, 300, 0, 0};
    arbiter.start_motion(jump);

    arbiter.advance_time(300);

    std::optional<Piece> landed = board.piece_at(cell);
    ASSERT_TRUE(landed.has_value());
    EXPECT_FALSE(landed->has_moved);
}

TEST(RealTimeArbiterTest, StartingAMotionMarksThePieceAsMoving) {
    Board board(8, 8);
    Position source{4, 4};
    Position destination{4, 5};
    Piece rook = make_rook(1, PieceColor::White, source);
    board.add_piece(rook);
    RealTimeArbiter arbiter(board);

    arbiter.start_motion(make_motion(rook, source, destination, 1000));

    std::optional<Piece> atSource = board.piece_at(source);
    ASSERT_TRUE(atSource.has_value());
    EXPECT_EQ(atSource->state, PieceState::Moving);
}

TEST(RealTimeArbiterTest, ArrivalReturnsTheMoverToIdle) {
    Board board(8, 8);
    Position source{4, 4};
    Position destination{4, 5};
    Piece rook = make_rook(1, PieceColor::White, source);
    board.add_piece(rook);
    RealTimeArbiter arbiter(board);
    arbiter.start_motion(make_motion(rook, source, destination, 1000));

    arbiter.advance_time(1000);

    std::optional<Piece> arrived = board.piece_at(destination);
    ASSERT_TRUE(arrived.has_value());
    EXPECT_EQ(arrived->state, PieceState::Idle);
}

TEST(RealTimeArbiterTest, EventsWithinOneCoarseCallAreOrderedByActualArrivalTimeNotStartOrder) {
    Board board(8, 8);
    Position sourceA{4, 4};
    Position destinationA{4, 5};
    Position sourceB{0, 0};
    Position destinationB{0, 1};
    Piece rookA = make_rook(1, PieceColor::White, sourceA);
    Piece rookB = make_rook(2, PieceColor::White, sourceB);
    board.add_piece(rookA);
    board.add_piece(rookB);
    RealTimeArbiter arbiter(board);
    // A starts first (duration 1000) and is already 100ms into its motion
    // by the time B starts (duration 500) -- B is the later-starting
    // motion but arrives chronologically first (simulated time 600 vs
    // A's 1000). A single coarse advance_time call then sweeps past both
    // arrivals at once; the returned events must still reflect the order
    // they actually arrived in, not the order their motions were started.
    arbiter.start_motion(make_motion(rookA, sourceA, destinationA, 1000));
    arbiter.advance_time(100);
    arbiter.start_motion(make_motion(rookB, sourceB, destinationB, 500));

    ArrivalEvents events = arbiter.advance_time(1000);

    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].moved_piece.id, rookB.id);
    EXPECT_EQ(events[1].moved_piece.id, rookA.id);
    EXPECT_EQ(events[0].arrived_at_ms, 600);  // B: started at 100, duration 500
    EXPECT_EQ(events[1].arrived_at_ms, 1000);  // A: started at 0, duration 1000
}

TEST(RealTimeArbiterTest, ACooldownThatStartsPartwayThroughACoarseCallOnlyCreditsTheRemainderOfThatCall) {
    Board board(8, 8);
    Position source{4, 4};
    Position destination{4, 5};
    Piece rook = make_rook(1, PieceColor::White, source);
    board.add_piece(rook);
    RealTimeArbiter arbiter(board);
    // Arrives 300ms into this single 1000ms call (duration 300) with a
    // 1000ms cooldown -- only the remaining 700ms of this same call can
    // count against that cooldown, leaving 300ms still outstanding
    // afterward, not the full 1000ms.
    arbiter.start_motion(make_motion(rook, source, destination, 300, /*cooldown_ms=*/1000));

    arbiter.advance_time(1000);
    arbiter.advance_time(299);
    EXPECT_TRUE(arbiter.is_piece_busy(rook.id));

    arbiter.advance_time(1);
    EXPECT_FALSE(arbiter.is_piece_busy(rook.id));
}

TEST(RealTimeArbiterTest, ACapturedPieceThatWasStillMidFlightNeverArrivesAndStopsBeingBusy) {
    Board board(8, 8);
    Position xSource{4, 4};
    Position xDestination{4, 7};
    Position ySource{4, 5};
    Piece x = make_rook(1, PieceColor::White, xSource);
    Piece y = make_rook(2, PieceColor::Black, ySource);
    board.add_piece(x);
    board.add_piece(y);
    RealTimeArbiter arbiter(board);
    // X starts a long move (1000ms); while X is still mid-flight (Board
    // still shows it sitting at xSource until its motion resolves), Y
    // captures it there with a much shorter motion.
    arbiter.start_motion(make_motion(x, xSource, xDestination, 1000));
    arbiter.start_motion(make_motion(y, ySource, xSource, 200));

    ArrivalEvents events = arbiter.advance_time(200);

    ASSERT_EQ(events.size(), 1u);
    ASSERT_TRUE(events[0].captured_piece.has_value());
    EXPECT_EQ(events[0].captured_piece->id, x.id);

    // X's own motion was due to arrive at simulated time 1000 -- advance
    // far past that. A captured piece must never resurrect and land
    // wherever its stale snapshot was heading.
    ArrivalEvents laterEvents = arbiter.advance_time(1000);

    EXPECT_TRUE(laterEvents.empty());
    EXPECT_FALSE(board.piece_at(xDestination).has_value());
    EXPECT_FALSE(arbiter.is_piece_busy(x.id));
}

TEST(RealTimeArbiterTest, OneCoarseAdvanceProducesTheSameOutcomeAsManyFineGrainedOnes) {
    // Runs the exact same two-piece scenario twice, chunked completely
    // differently (one giant advance_time call vs. many small, oddly-sized
    // ones summing to the same total) -- the caller's step size must never
    // change the result, in either the events produced or the final board.
    auto run = [](int step_ms) {
        Board board(8, 8);
        Position sourceA{4, 4};
        Position destinationA{4, 5};
        Position sourceB{0, 0};
        Position destinationB{0, 1};
        Piece rookA = make_rook(1, PieceColor::White, sourceA);
        Piece rookB = make_rook(2, PieceColor::White, sourceB);
        board.add_piece(rookA);
        board.add_piece(rookB);
        RealTimeArbiter arbiter(board);
        arbiter.start_motion(make_motion(rookA, sourceA, destinationA, 1000, /*cooldown_ms=*/300));
        arbiter.start_motion(make_motion(rookB, sourceB, destinationB, 500, /*cooldown_ms=*/100));

        ArrivalEvents all_events;
        int total_ms = 1500;
        int elapsed = 0;
        while (elapsed < total_ms) {
            int this_step = std::min(step_ms, total_ms - elapsed);
            ArrivalEvents events = arbiter.advance_time(this_step);
            all_events.insert(all_events.end(), events.begin(), events.end());
            elapsed += this_step;
        }
        return std::make_pair(all_events,
                               std::make_pair(board.piece_at(destinationA), board.piece_at(destinationB)));
    };

    auto [coarseEvents, coarseFinal] = run(1500);  // a single call covering everything
    auto [fineEvents, fineFinal] = run(37);         // many odd-sized small calls

    ASSERT_EQ(coarseEvents.size(), fineEvents.size());
    for (size_t i = 0; i < coarseEvents.size(); ++i) {
        EXPECT_EQ(coarseEvents[i].moved_piece.id, fineEvents[i].moved_piece.id);
        EXPECT_EQ(coarseEvents[i].arrived_at_ms, fineEvents[i].arrived_at_ms);
    }
    ASSERT_TRUE(coarseFinal.first.has_value());
    ASSERT_TRUE(fineFinal.first.has_value());
    EXPECT_EQ(coarseFinal.first->id, fineFinal.first->id);
    ASSERT_TRUE(coarseFinal.second.has_value());
    ASSERT_TRUE(fineFinal.second.has_value());
    EXPECT_EQ(coarseFinal.second->id, fineFinal.second->id);
}

TEST(RealTimeArbiterTest, CooldownRemainingMsIsZeroWhenThePieceIsNotResting) {
    Board board(8, 8);
    RealTimeArbiter arbiter(board);

    EXPECT_EQ(arbiter.cooldown_remaining_ms(PieceId{1}), 0);
}

TEST(RealTimeArbiterTest, CooldownRemainingMsCountsDownExactlyAsIsPieceBusyDoes) {
    Board board(8, 8);
    Position source{4, 4};
    Position destination{4, 5};
    Piece rook = make_rook(1, PieceColor::White, source);
    board.add_piece(rook);
    RealTimeArbiter arbiter(board);
    arbiter.start_motion(make_motion(rook, source, destination, 1000, /*cooldown_ms=*/300));

    arbiter.advance_time(1000);
    EXPECT_EQ(arbiter.cooldown_remaining_ms(rook.id), 300);

    arbiter.advance_time(200);
    EXPECT_EQ(arbiter.cooldown_remaining_ms(rook.id), 100);

    arbiter.advance_time(100);
    EXPECT_EQ(arbiter.cooldown_remaining_ms(rook.id), 0);
    EXPECT_FALSE(arbiter.is_piece_busy(rook.id));
}

TEST(RealTimeArbiterTest, TwoFriendlyPiecesRacingForTheSameCellBlockTheSecondArrivalInsteadOfCapturingIt) {
    Board board(4, 1);
    Position firstStart{0, 0};
    Position secondStart{0, 3};
    Position sharedDestination{0, 1};
    Piece first = make_rook(1, PieceColor::White, firstStart);
    Piece second = make_rook(2, PieceColor::White, secondStart);
    board.add_piece(first);
    board.add_piece(second);
    RealTimeArbiter arbiter(board);
    // Both are white. "first" is a 1-cell hop (arrives quickly); "second" is
    // a longer 2-cell trip to the same destination, so it arrives later to
    // find its own ally already standing there.
    arbiter.start_motion(make_motion(first, firstStart, sharedDestination, 500));
    arbiter.start_motion(make_motion(second, secondStart, sharedDestination, 1000));

    ArrivalEvents firstWave = arbiter.advance_time(500);
    ASSERT_EQ(firstWave.size(), 1u);
    EXPECT_FALSE(firstWave[0].captured_piece.has_value());

    ArrivalEvents secondWave = arbiter.advance_time(500);

    ASSERT_EQ(secondWave.size(), 1u);
    EXPECT_FALSE(secondWave[0].captured_piece.has_value());  // no ally "captured"
    std::optional<Piece> atDestination = board.piece_at(sharedDestination);
    ASSERT_TRUE(atDestination.has_value());
    EXPECT_EQ(atDestination->id, first.id);  // first ally still holds the cell
    std::optional<Piece> blocked = board.piece_at(secondStart);
    ASSERT_TRUE(blocked.has_value());
    EXPECT_EQ(blocked->id, second.id);  // second stayed at its own source
    EXPECT_EQ(blocked->state, PieceState::Idle);
}

TEST(RealTimeArbiterTest, ACapturedPiecesReportedSnapshotIsMarkedCaptured) {
    Board board(8, 8);
    Position source{4, 4};
    Position destination{4, 5};
    Piece rook = make_rook(1, PieceColor::White, source);
    Piece victim = make_rook(2, PieceColor::Black, destination);
    board.add_piece(rook);
    board.add_piece(victim);
    RealTimeArbiter arbiter(board);
    arbiter.start_motion(make_motion(rook, source, destination, 1000));

    ArrivalEvents events = arbiter.advance_time(1000);

    ASSERT_EQ(events.size(), 1u);
    ASSERT_TRUE(events[0].captured_piece.has_value());
    EXPECT_EQ(events[0].captured_piece->state, PieceState::Captured);
}
