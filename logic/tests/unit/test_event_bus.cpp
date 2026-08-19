#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "kfc/events/event_bus.hpp"
#include "kfc/events/game_events.hpp"
#include "kfc/model/piece.hpp"

using namespace kfc::events;
using kfc::model::PieceColor;

namespace {
struct Ping {
    int value;
};
struct Pong {
    std::string text;
};
}  // namespace

TEST(EventBusTest, DeliversAPublishedEventToItsSubscriber) {
    EventBus bus;
    int seen = 0;
    bus.subscribe<Ping>([&](const Ping& p) { seen = p.value; });

    bus.publish(Ping{42});

    EXPECT_EQ(seen, 42);
}

TEST(EventBusTest, DoesNotDeliverToSubscribersOfADifferentType) {
    EventBus bus;
    bool pong_called = false;
    bus.subscribe<Pong>([&](const Pong&) { pong_called = true; });

    bus.publish(Ping{1});

    EXPECT_FALSE(pong_called);
}

TEST(EventBusTest, DeliversToEverySubscriberInSubscriptionOrder) {
    EventBus bus;
    std::vector<int> order;
    bus.subscribe<Ping>([&](const Ping&) { order.push_back(1); });
    bus.subscribe<Ping>([&](const Ping&) { order.push_back(2); });
    bus.subscribe<Ping>([&](const Ping&) { order.push_back(3); });

    bus.publish(Ping{0});

    EXPECT_EQ(order, (std::vector<int>{1, 2, 3}));
}

TEST(EventBusTest, PublishingWithNoSubscribersIsANoOp) {
    EventBus bus;
    EXPECT_NO_THROW(bus.publish(Ping{7}));
}

TEST(EventBusTest, DifferentEventTypesAreRoutedIndependently) {
    EventBus bus;
    int pings = 0;
    std::string last_pong;
    bus.subscribe<Ping>([&](const Ping&) { ++pings; });
    bus.subscribe<Pong>([&](const Pong& p) { last_pong = p.text; });

    bus.publish(Ping{0});
    bus.publish(Pong{"hi"});
    bus.publish(Ping{0});

    EXPECT_EQ(pings, 2);
    EXPECT_EQ(last_pong, "hi");
}

TEST(EventBusTest, CarriesGameEndedWinnerPayload) {
    EventBus bus;
    std::optional<PieceColor> reported = std::nullopt;
    bool fired = false;
    bus.subscribe<GameEnded>([&](const GameEnded& e) {
        reported = e.winner;
        fired = true;
    });

    bus.publish(GameEnded{PieceColor::Black});

    ASSERT_TRUE(fired);
    ASSERT_TRUE(reported.has_value());
    EXPECT_EQ(*reported, PieceColor::Black);
}

TEST(EventBusTest, UnsubscribeStopsFurtherDeliveryToThatHandler) {
    EventBus bus;
    int count = 0;
    auto id = bus.subscribe<Ping>([&](const Ping&) { ++count; });

    bus.publish(Ping{0});
    bus.unsubscribe(id);
    bus.publish(Ping{0});

    EXPECT_EQ(count, 1);
}

TEST(EventBusTest, UnsubscribeLeavesOtherSubscribersOfTheSameTypeIntact) {
    EventBus bus;
    std::vector<int> order;
    auto first = bus.subscribe<Ping>([&](const Ping&) { order.push_back(1); });
    bus.subscribe<Ping>([&](const Ping&) { order.push_back(2); });

    bus.unsubscribe(first);
    bus.publish(Ping{0});

    EXPECT_EQ(order, (std::vector<int>{2}));
}

TEST(EventBusTest, UnsubscribeIsANoOpOnAnAlreadyUnsubscribedOrDefaultId) {
    EventBus bus;
    EventBus::SubscriptionId never_subscribed;
    EXPECT_NO_THROW(bus.unsubscribe(never_subscribed));

    int count = 0;
    auto id = bus.subscribe<Ping>([&](const Ping&) { ++count; });
    bus.unsubscribe(id);
    EXPECT_NO_THROW(bus.unsubscribe(id));
}

TEST(EventBusTest, GameStartedReachesItsSubscriber) {
    EventBus bus;
    int starts = 0;
    bus.subscribe<GameStarted>([&](const GameStarted&) { ++starts; });

    bus.publish(GameStarted{});

    EXPECT_EQ(starts, 1);
}
