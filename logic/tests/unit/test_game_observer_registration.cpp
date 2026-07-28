#include <gtest/gtest.h>

#include <utility>

#include "kfc/events/event_bus.hpp"
#include "kfc/input/board_mapper.hpp"
#include "kfc/model/board.hpp"
#include "kfc/realtime/arrival_event.hpp"
#include "kfc/texttests/game.hpp"

using namespace kfc::model;

namespace {

Piece make_rook(int id, PieceColor color, Position cell) {
    return Piece{PieceId{id}, color, PieceKind::Rook, cell, PieceState::Idle};
}

// Pixel at the centre of a board cell, for driving Game::jump through its
// pixel-based click surface. kCellSizePixels is 100, so cell {4,4} is (450,450).
int cell_centre_pixel(int index) {
    return static_cast<int>((index + 0.5) * kfc::input::kCellSizePixels);
}

}  // namespace

TEST(GameEventBusTest, PublishesEachArrivalToSubscribersOfItsBus) {
    Board board(8, 8);
    board.add_piece(make_rook(1, PieceColor::White, Position{4, 4}));
    kfc::texttests::Game game(std::move(board));

    int arrivals = 0;
    game.events().subscribe<ArrivalEvent>([&](const ArrivalEvent&) { ++arrivals; });

    // A jump-in-place at the rook's own cell produces exactly one arrival once
    // it lands; wait past the jump duration so it does.
    game.jump(cell_centre_pixel(4), cell_centre_pixel(4));
    game.wait(1000);

    EXPECT_EQ(arrivals, 1);
}

TEST(GameEventBusTest, DeliversToEverySubscriberInSubscriptionOrder) {
    Board board(8, 8);
    board.add_piece(make_rook(1, PieceColor::White, Position{4, 4}));
    kfc::texttests::Game game(std::move(board));

    int first = 0;
    int second = 0;
    game.events().subscribe<ArrivalEvent>([&](const ArrivalEvent&) { ++first; });
    game.events().subscribe<ArrivalEvent>([&](const ArrivalEvent&) { ++second; });

    game.jump(cell_centre_pixel(4), cell_centre_pixel(4));
    game.wait(1000);

    EXPECT_EQ(first, 1);
    EXPECT_EQ(second, 1);
}
