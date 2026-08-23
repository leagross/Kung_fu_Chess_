#pragma once

#include <functional>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace kfc::events {

/// Typed publish/subscribe bus; handlers run synchronously, not thread-safe.
class EventBus {
public:
    /// Handlers are held for the bus's lifetime; there is no unsubscribe.
    template <typename EventT>
    void subscribe(std::function<void(const EventT&)> handler) {
        subscribers_[std::type_index(typeid(EventT))].push_back(
            [handler = std::move(handler)](const void* event) { handler(*static_cast<const EventT*>(event)); });
    }

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
    std::unordered_map<std::type_index, std::vector<ErasedHandler>> subscribers_;
};

}  // namespace kfc::events
