#pragma once

#include "DiaInput/EventData.h"
#include "DiaInput/InputSourceManager.h"
#include "DiaInput/Messages/input_messages.h"
#include <DiaMessageBus/IFlushAdapter.h>

namespace Dia::MessageBus { class Bus; }

namespace Dia::Input {

// -----------------------------------------------------------------------
// InputBusAdapter
//
// Owns a reference to a live InputSourceManager and drains it once per bus
// tick via Flush(), broadcasting each event as its corresponding generated
// Messages::* type (KeyDownEvent, KeyUpEvent, MouseButtonEvent,
// MouseMovedEvent). Window/joystick/gamepad events are not yet modeled on
// the bus and are ignored.
//
// InputSourceManager has no internal queue of its own —
// Update(EventData&) is caller-driven and, per InputSourceManager.h,
// "appends... it is not cleared first". This adapter owns its own scratch
// EventData buffer (cleared before each Update() call) so it never
// interferes with any other buffer(s) an existing caller drives.
//
// IMPORTANT — InputSourceManager::Update() is DESTRUCTIVE: it polls every
// registered IInputSource, and each source's Poll() drains that source's
// internal queue (see InputSourceManager::Update in InputSourceManager.cpp
// — it collects via Poll() into a temp buffer, which sources will not
// reproduce on a second call this frame). Two different InputSourceManager
// callers competing for the SAME instance would therefore split events
// between them unpredictably, not duplicate them. This adapter must
// therefore own (or be pointed at) an InputSourceManager that no other
// consumer is already draining that frame.
//
// Threading — Dia::MessageBus::Bus is sim-thread-only (SD-MBX2-008), so
// this adapter's Flush() must run on the same thread as the Bus. If your
// game drives input on a different thread than the Bus (e.g.
// CluicheGameBaseline's current split, where KernelModule polls
// InputSourceManager on the Main PU and forwards events to the Sim PU via
// InputStreamModule + MainToSimEvent), do not point this adapter at that
// Main-PU InputSourceManager instance — construct a second,
// sim-thread-owned InputSourceManager for this adapter, or forward events
// into the Bus from the sim-side receiver instead.
// -----------------------------------------------------------------------
class InputBusAdapter : public Dia::MessageBus::IFlushAdapter {
public:
    // Registers the generated input message types with bus. Caller (the
    // input-owning module) keeps both inputSourceManager and bus alive for
    // this adapter's lifetime.
    InputBusAdapter(InputSourceManager& inputSourceManager, Dia::MessageBus::Bus& bus);

    InputBusAdapter(const InputBusAdapter&)            = delete;
    InputBusAdapter& operator=(const InputBusAdapter&) = delete;

    // Dia::MessageBus::IFlushAdapter — calls mInputSourceManager.Update(),
    // then Broadcasts each drained event by kind.
    void Flush(Dia::MessageBus::Bus& bus) override;

private:
    InputSourceManager& mInputSourceManager;
    EventData           mScratch;
};

} // namespace Dia::Input
