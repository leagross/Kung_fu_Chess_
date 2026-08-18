#include "kfc/server/session_registry.hpp"

#include <utility>

namespace kfc::server {

SessionRegistry::Lease::Lease(SessionRegistry& registry, std::string username)
    : registry_(&registry), username_(std::move(username)) {}

SessionRegistry::Lease::~Lease() {
    if (registry_ != nullptr) {
        registry_->release(username_);
    }
}

SessionRegistry::Lease::Lease(Lease&& other) noexcept
    : registry_(other.registry_), username_(std::move(other.username_)) {
    // Cleared, so the moved-from lease's destructor releases nothing -- both
    // releasing would free a name its new owner is still using.
    other.registry_ = nullptr;
}

SessionRegistry::Lease& SessionRegistry::Lease::operator=(Lease&& other) noexcept {
    if (this != &other) {
        if (registry_ != nullptr) {
            registry_->release(username_);  // give up whatever we held first
        }
        registry_ = other.registry_;
        username_ = std::move(other.username_);
        other.registry_ = nullptr;
    }
    return *this;
}

std::optional<SessionRegistry::Lease> SessionRegistry::claim(const std::string& username) {
    std::lock_guard<std::mutex> guard(mutex_);
    auto [it, inserted] = live_.insert(username);
    if (!inserted) {
        return std::nullopt;
    }
    return Lease(*this, username);
}

void SessionRegistry::release(const std::string& username) {
    std::lock_guard<std::mutex> guard(mutex_);
    live_.erase(username);
}

std::size_t SessionRegistry::live_count() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return live_.size();
}

}  // namespace kfc::server
