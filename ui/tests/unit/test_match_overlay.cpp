#include <gtest/gtest.h>

#include <chrono>

#include "kfc/events/event_bus.hpp"
#include "kfc/events/game_events.hpp"
#include "kfc/graphics/app/match_overlay.hpp"

using kfc::graphics::app::MatchOverlay;
using kfc::graphics::app::Overlay;
using kfc::model::PieceColor;
using Clock = MatchOverlay::Clock;
using namespace std::chrono_literals;

namespace {

// Time is passed in rather than read from a clock, so the intro can be tested
// without waiting 1.5 real seconds for it.
constexpr int kIntroMs = 1500;

}  // namespace

TEST(MatchOverlayTest, NothingIsShownBeforeAnythingHappens) {
    kfc::events::EventBus bus;
    MatchOverlay overlay(bus, kIntroMs);

    EXPECT_EQ(overlay.current(/*searching=*/false, Clock::now()), Overlay::None);
}

TEST(MatchOverlayTest, SearchingIsShownWhileWaitingForAnOpponent) {
    kfc::events::EventBus bus;
    MatchOverlay overlay(bus, kIntroMs);

    EXPECT_EQ(overlay.current(/*searching=*/true, Clock::now()), Overlay::Searching);
}

TEST(MatchOverlayTest, TheIntroShowsOnlyForItsOwnDuration) {
    kfc::events::EventBus bus;
    MatchOverlay overlay(bus, kIntroMs);
    bus.publish(kfc::events::GameStarted{});
    Clock::time_point start = Clock::now();

    EXPECT_EQ(overlay.current(false, start), Overlay::Intro);
    EXPECT_EQ(overlay.current(false, start + std::chrono::milliseconds(kIntroMs - 1)), Overlay::Intro);
    EXPECT_EQ(overlay.current(false, start + std::chrono::milliseconds(kIntroMs + 1)), Overlay::None)
        << "the splash outstayed its welcome";
}

TEST(MatchOverlayTest, TheIntroFadesOutRatherThanVanishing) {
    kfc::events::EventBus bus;
    MatchOverlay overlay(bus, kIntroMs);
    bus.publish(kfc::events::GameStarted{});
    Clock::time_point start = Clock::now();

    EXPECT_NEAR(overlay.intro_opacity(start), 1.0, 0.01);
    EXPECT_NEAR(overlay.intro_opacity(start + std::chrono::milliseconds(kIntroMs / 2)), 0.5, 0.05);
    // Clamped, never negative -- a frame arriving late must not ask for a
    // negative-opacity draw.
    EXPECT_DOUBLE_EQ(overlay.intro_opacity(start + std::chrono::milliseconds(kIntroMs * 10)), 0.0);
}

TEST(MatchOverlayTest, TheEndBannerCarriesTheWinner) {
    kfc::events::EventBus bus;
    MatchOverlay overlay(bus, kIntroMs);

    bus.publish(kfc::events::GameEnded{PieceColor::Black});

    EXPECT_EQ(overlay.current(false, Clock::now()), Overlay::GameOver);
    ASSERT_TRUE(overlay.winner().has_value());
    EXPECT_EQ(*overlay.winner(), PieceColor::Black);
}

TEST(MatchOverlayTest, ADrawEndsTheGameWithNoWinner) {
    kfc::events::EventBus bus;
    MatchOverlay overlay(bus, kIntroMs);

    bus.publish(kfc::events::GameEnded{std::nullopt});

    EXPECT_EQ(overlay.current(false, Clock::now()), Overlay::GameOver);
    EXPECT_FALSE(overlay.winner().has_value());
}

TEST(MatchOverlayTest, ACountdownShowsTheSecondsItWasGiven) {
    kfc::events::EventBus bus;
    MatchOverlay overlay(bus, kIntroMs);

    bus.publish(kfc::events::OpponentCountdown{17});

    EXPECT_EQ(overlay.current(false, Clock::now()), Overlay::Countdown);
    EXPECT_EQ(overlay.countdown_seconds(), 17);

    bus.publish(kfc::events::OpponentCountdown{16});
    EXPECT_EQ(overlay.countdown_seconds(), 16);
}

// --- The priorities, which is the part that is easy to get wrong ---

TEST(MatchOverlayTest, SearchingOutranksEverythingElse) {
    kfc::events::EventBus bus;
    MatchOverlay overlay(bus, kIntroMs);

    bus.publish(kfc::events::GameStarted{});
    bus.publish(kfc::events::OpponentCountdown{5});
    bus.publish(kfc::events::GameEnded{PieceColor::White});

    EXPECT_EQ(overlay.current(/*searching=*/true, Clock::now()), Overlay::Searching);
}

// The countdown asks "will they come back?". Once the game is decided that is
// settled, so both banners must not want the board at once.
TEST(MatchOverlayTest, TheEndBannerSupersedesARunningCountdown) {
    kfc::events::EventBus bus;
    MatchOverlay overlay(bus, kIntroMs);

    bus.publish(kfc::events::OpponentCountdown{9});
    bus.publish(kfc::events::GameEnded{PieceColor::White});

    EXPECT_EQ(overlay.current(false, Clock::now()), Overlay::GameOver);
}

// They genuinely overlap: an opponent who drops in the first second and a half
// of a match. The countdown is the one the player needs to see.
TEST(MatchOverlayTest, ACountdownOutranksTheIntro) {
    kfc::events::EventBus bus;
    MatchOverlay overlay(bus, kIntroMs);

    bus.publish(kfc::events::GameStarted{});
    Clock::time_point start = Clock::now();
    bus.publish(kfc::events::OpponentCountdown{19});

    EXPECT_EQ(overlay.current(false, start), Overlay::Countdown);
}

// The other way a countdown ends. Forgetting it leaves a stale countdown frozen
// on screen for the rest of a game that is being played perfectly normally.
TEST(MatchOverlayTest, AnOpponentComingBackClearsTheCountdown) {
    kfc::events::EventBus bus;
    MatchOverlay overlay(bus, kIntroMs);
    bus.publish(kfc::events::GameStarted{});
    Clock::time_point start = Clock::now();

    bus.publish(kfc::events::OpponentCountdown{12});
    ASSERT_EQ(overlay.current(false, start), Overlay::Countdown);

    bus.publish(kfc::events::OpponentReturned{});

    EXPECT_EQ(overlay.current(false, start + std::chrono::milliseconds(kIntroMs + 1)), Overlay::None)
        << "returning in time left a stale countdown on screen";
}

TEST(MatchOverlayTest, ASecondDisconnectAfterAReturnCountsDownAgain) {
    kfc::events::EventBus bus;
    MatchOverlay overlay(bus, kIntroMs);

    bus.publish(kfc::events::OpponentCountdown{20});
    bus.publish(kfc::events::OpponentReturned{});
    bus.publish(kfc::events::OpponentCountdown{20});

    EXPECT_EQ(overlay.current(false, Clock::now()), Overlay::Countdown);
    EXPECT_EQ(overlay.countdown_seconds(), 20);
}
