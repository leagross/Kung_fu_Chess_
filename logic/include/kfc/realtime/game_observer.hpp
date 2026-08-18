#pragma once

#include "../../kfc/realtime/arrival_event.hpp"

namespace kfc::model {

/// Reacts to one piece's arrival, decoupled from the hot move-handling path:
/// Game calls this once per ArrivalEvent only after RealTimeArbiter has
/// already fully resolved it, so an observer can never affect a move itself.
class IGameObserver {
public:
    virtual ~IGameObserver() = default;

    virtual void on_arrival(const ArrivalEvent& event) = 0;
};

}  // namespace kfc::model
