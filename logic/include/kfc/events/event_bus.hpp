#pragma once

#include <functional>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace kfc::events {

/// Minimal typed publish/subscribe bus decoupling publishers from subscribers
/// by event type. Handlers run synchronously, in subscription order, on
/// whatever thread calls publish() -- not internally synchronized, so do not
/// publish from multiple threads onto one bus.
class EventBus {
public:
    /// Handlers are held for the bus's lifetime; there is no unsubscribe.
    template <typename EventT>
    void subscribe(std::function<void(const EventT&)> handler) {
        subscribers_[std::type_index(typeid(EventT))].push_back(
            [handler = std::move(handler)](const void* event) { handler(*static_cast<const EventT*>(event)); });
    }

    /// No-op if nothing subscribed to EventT.
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
