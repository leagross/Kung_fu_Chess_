#pragma once

#include <functional>
#include <list>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace kfc::events {

/// Minimal typed publish/subscribe bus decoupling publishers from subscribers
/// by event type. Handlers run synchronously, in subscription order, on
/// whatever thread calls publish() -- not internally synchronized, so do not
/// publish from multiple threads onto one bus.
class EventBus {
private:
    using ErasedHandler = std::function<void(const void*)>;

public:
    /// Opaque handle from subscribe(), needed to unsubscribe() later. Default
    /// constructed = not subscribed to anything (unsubscribe() on it is a no-op).
    class SubscriptionId {
        friend class EventBus;

    public:
        SubscriptionId() = default;

    private:
        std::type_index type_ = typeid(void);
        std::list<ErasedHandler>::iterator it_{};
        bool active_ = false;
    };

    template <typename EventT>
    SubscriptionId subscribe(std::function<void(const EventT&)> handler) {
        auto& handlers = subscribers_[std::type_index(typeid(EventT))];
        handlers.push_back(
            [handler = std::move(handler)](const void* event) { handler(*static_cast<const EventT*>(event)); });
        SubscriptionId id;
        id.type_ = std::type_index(typeid(EventT));
        id.it_ = std::prev(handlers.end());
        id.active_ = true;
        return id;
    }

    /// Safe to call with an already-inactive id (default-constructed, or
    /// already unsubscribed) -- a no-op.
    void unsubscribe(SubscriptionId& id) {
        if (!id.active_) {
            return;
        }
        subscribers_[id.type_].erase(id.it_);
        id.active_ = false;
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
    // std::list, not std::vector: unsubscribe() erases by iterator, which
    // must stay valid across other handlers being added/removed for the
    // same event type.
    std::unordered_map<std::type_index, std::list<ErasedHandler>> subscribers_;
};

}  // namespace kfc::events
