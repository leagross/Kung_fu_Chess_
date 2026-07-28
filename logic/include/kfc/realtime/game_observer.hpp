#pragma once

#include "../../kfc/realtime/arrival_event.hpp"

namespace kfc::model {

/// Reacts to one piece's arrival, entirely decoupled from the hot
/// move-handling path: neither GameEngine nor RealTimeArbiter ever call
/// this directly. Game does, once per ArrivalEvent, only after
/// RealTimeArbiter::advance_time has already fully resolved it -- an
/// observer never blocks, delays, or otherwise affects whether or how a
/// move happens, only what a UI shows once it already has.
class IGameObserver {
public:
    virtual ~IGameObserver() = default;

    virtual void on_arrival(const ArrivalEvent& event) = 0;
};

}  // namespace kfc::model
