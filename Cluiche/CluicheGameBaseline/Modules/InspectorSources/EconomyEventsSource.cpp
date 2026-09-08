#include "Modules/InspectorSources/EconomyEventsSource.h"
#include <DiaEconomy/EconomySystem.h>
#include <DiaEconomy/EconomyInstance.h>
#include <DiaDebugServer/DebugServer.h>
#include <DiaCore/Json/external/json/json.h>

namespace Cluiche { namespace AppFlow {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

EconomyEventsSource::EconomyEventsSource(Dia::Economy::EconomySystem& system)
    : mSystem(system)
{
}

// ---------------------------------------------------------------------------
// IInspectorDataSource
// ---------------------------------------------------------------------------

Dia::Core::StringCRC EconomyEventsSource::GetTopic() const
{
    static const Dia::Core::StringCRC kTopic("economy.events");
    return kTopic;
}

Dia::DebugServer::SourcePolicy EconomyEventsSource::GetPolicy() const
{
    return { Dia::DebugServer::SourceStrategy::kEventDriven, 0.0f, 0, 0.0f };
}

void EconomyEventsSource::Activate(Dia::DebugServer::DebugServer* server)
{
    mServer = server;
    mSystem.GetObserverSubject().Subscribe(this);
}

void EconomyEventsSource::Tick(float /*dt*/, int connectionCount, int subCount)
{
    ++mFrame;

    // Detect a new subscriber: subCount rose and at least one client is connected.
    // Send the full ring so the new client has history immediately.
    if (subCount > mLastSubCount && connectionCount > 0)
    {
        SendFullRing();
    }
    mLastSubCount = subCount;
}

void EconomyEventsSource::Deactivate()
{
    mSystem.GetObserverSubject().Unsubscribe(this);
    mServer       = nullptr;
    mLastSubCount = -1;
}

// ---------------------------------------------------------------------------
// IEconomyObserver
// ---------------------------------------------------------------------------

void EconomyEventsSource::OnPoolChanged(const Dia::Economy::PoolChangedEvent& ev)
{
    if (ev.delta == 0.0f)
        return;

    Json::Value entry;
    entry["frame"]    = mFrame;
    entry["resource"] = ev.resourceName.AsChar();
    entry["instance"] = ev.instanceName.AsChar();
    entry["amount"]   = ev.delta;
    entry["source"]   = "";

    if (ev.delta > 0.0f)
        entry["type"] = "Earn";
    else
        entry["type"] = "Spend";

    SendDelta(entry);
}

void EconomyEventsSource::OnTransactionClamped(const Dia::Economy::TransactionClampedEvent& ev)
{
    Json::Value entry;
    entry["frame"]     = mFrame;
    entry["type"]      = "Clamped";
    entry["instance"]  = ev.instanceName.AsChar();
    entry["resource"]  = ev.resourceName.AsChar();
    entry["attempted"] = ev.requestedAmount;
    entry["actual"]    = ev.actualAmount;

    SendDelta(entry);
}

void EconomyEventsSource::OnTransferCompleted(const Dia::Economy::TransferCompletedEvent& ev)
{
    Json::Value entry;
    entry["frame"]       = mFrame;
    entry["type"]        = "Transfer";
    entry["instance"]    = ev.fromInstanceName.AsChar();
    entry["resource"]    = ev.resourceName.AsChar();
    entry["amount"]      = ev.amount;
    entry["destination"] = ev.toInstanceName.AsChar();

    SendDelta(entry);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void EconomyEventsSource::PushToRing(const Json::Value& ev)
{
    if (mRingCount < kRingDepth)
    {
        mRing[(mRingHead + mRingCount) % kRingDepth] = ev;
        ++mRingCount;
    }
    else
    {
        // Ring is full: overwrite oldest slot and advance head.
        mRing[mRingHead] = ev;
        mRingHead = (mRingHead + 1) % kRingDepth;
    }
}

void EconomyEventsSource::SendFullRing()
{
    Json::Value events(Json::arrayValue);
    for (unsigned int i = 0; i < mRingCount; ++i)
        events.append(mRing[(mRingHead + i) % kRingDepth]);

    Json::Value payload;
    payload["full_ring"] = true;
    payload["events"]    = events;

    mServer->NotifySubscribers(GetTopic(), payload);
}

void EconomyEventsSource::SendDelta(const Json::Value& ev)
{
    PushToRing(ev);

    if (!mServer)
        return;

    Json::Value events(Json::arrayValue);
    events.append(ev);

    Json::Value payload;
    payload["full_ring"] = false;
    payload["events"]    = events;

    mServer->NotifySubscribers(GetTopic(), payload);
}

}} // namespace Cluiche::AppFlow
