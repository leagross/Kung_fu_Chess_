#pragma once

#include <functional>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace kfc::events {

/// A minimal typed publish/subscribe bus -- the CTD SERVER lecture's "בס": one
/// side publishes an event, whoever cares subscribes to it, and the two never
/// reference each other. It is what decouples "something happened in the game"
/// from "the UI reacts": the score panel, the move log, sound, and the
/// start/end animations each subscribe to just the event(s) they care about,
/// and the game (local or networked) publishes without knowing who, if anyone,
/// is listening.
///
/// Events are distinguished by their C++ type: subscribe<GameEnded> only ever
/// hears GameEnded, publish<ArrivalEvent> only reaches ArrivalEvent
/// subscribers. Handlers run synchronously, in subscription order, on whatever
/// thread calls publish() -- deliberately not internally synchronized, matching
/// the single-threaded UI/observer model already used everywhere here (a local
/// Game ticks on one thread; a networked ServerLink drains and dispatches on
/// its render thread). Do not publish from multiple threads onto one bus.
class EventBus {
public:
    /// Registers handler to be called every time an EventT is published.
    /// Handlers are held by value for the bus's lifetime; there is no
    /// unsubscribe (a UI's subscribers live as long as the game view does),
    /// which keeps this deliberately small.
    template <typename EventT>
    void subscribe(std::function<void(const EventT&)> handler) {
        subscribers_[std::type_index(typeid(EventT))].push_back(
            [handler = std::move(handler)](const void* event) { handler(*static_cast<const EventT*>(event)); });
    }

    /// Delivers event to every EventT subscriber, in the order they
    /// subscribed. A no-op if nothing subscribed to EventT.
    template <typename EventT>
    void publish(const EventT& event) const {
        auto it = subscribers_.find(std::type_index(typeid(EventT)));
        if (it == subscribers_.end()) {
            return;
        }
        for (const auto& handler : it->second) {
            handler(&event);
        }
    }

private:
    using ErasedHandler = std::function<void(const void*)>;
    // publish() only reads this (it stays const); subscribe() is the sole
    // mutator, so no mutable is needed.
    std::unordered_map<std::type_index, std::vector<ErasedHandler>> subscribers_;
};

}  // namespace kfc::events
