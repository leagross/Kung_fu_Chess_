#include <gtest/gtest.h>

#include <vector>

#include "kfc/audio/sound.hpp"
#include "kfc/audio/sound_board.hpp"
#include "kfc/events/event_bus.hpp"
#include "kfc/events/game_events.hpp"
#include "kfc/model/piece.hpp"
#include "kfc/realtime/arrival_event.hpp"

using namespace kfc::audio;
using namespace kfc::model;
using kfc::events::EventBus;
using kfc::events::GameEnded;
using kfc::events::GameStarted;

namespace {

class RecordingSoundPlayer : public ISoundPlayer {
public:
    void play(Sound sound) override { played.push_back(sound); }
    std::vector<Sound> played;
};

Piece make_piece(int id, PieceColor color, PieceKind kind, Position cell) {
    return Piece{PieceId{id}, color, kind, cell, PieceState::Idle};
}

ArrivalEvent plain_move() {
    ArrivalEvent event;
    event.moved_piece = make_piece(1, PieceColor::White, PieceKind::Rook, Position{2, 0});
    event.source = Position{0, 0};
    event.destination = Position{2, 0};
    event.captured_piece = std::nullopt;
    return event;
}

ArrivalEvent capturing_move() {
    ArrivalEvent event = plain_move();
    event.captured_piece = make_piece(2, PieceColor::Black, PieceKind::Pawn, Position{2, 0});
    return event;
}

}  // namespace

TEST(SoundBoardTest, PlaysMoveForAnArrivalWithoutACapture) {
    EventBus bus;
    RecordingSoundPlayer player;
    SoundBoard board(bus, player);

    bus.publish(plain_move());

    ASSERT_EQ(player.played.size(), 1u);
    EXPECT_EQ(player.played[0], Sound::Move);
}

TEST(SoundBoardTest, PlaysCaptureForAnArrivalThatTookAPiece) {
    EventBus bus;
    RecordingSoundPlayer player;
    SoundBoard board(bus, player);

    bus.publish(capturing_move());

    ASSERT_EQ(player.played.size(), 1u);
    EXPECT_EQ(player.played[0], Sound::Capture);
}

TEST(SoundBoardTest, PlaysStartAndEndCues) {
    EventBus bus;
    RecordingSoundPlayer player;
    SoundBoard board(bus, player);

    bus.publish(GameStarted{});
    bus.publish(GameEnded{PieceColor::White});

    ASSERT_EQ(player.played.size(), 2u);
    EXPECT_EQ(player.played[0], Sound::GameStart);
    EXPECT_EQ(player.played[1], Sound::GameEnd);
}

TEST(SoundBoardTest, MapsAWholeGameSequenceInOrder) {
    EventBus bus;
    RecordingSoundPlayer player;
    SoundBoard board(bus, player);

    bus.publish(GameStarted{});
    bus.publish(plain_move());
    bus.publish(capturing_move());
    bus.publish(GameEnded{std::nullopt});

    EXPECT_EQ(player.played,
              (std::vector<Sound>{Sound::GameStart, Sound::Move, Sound::Capture, Sound::GameEnd}));
}
