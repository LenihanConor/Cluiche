#pragma once
////////////////////////////////////////////////////////////////////////////////
// InspectorDataSourceBase.h
//
// Strategy base classes for IInspectorDataSource. Each base handles the
// "when to fire" logic; derived classes implement "what to send."
//
// Four strategies:
//   ChangeDetectedSourceBase — broadcasts when CollectAndHash() produces a
//     new hash, or when subscriber count changes (forces initial send).
//
//   PeriodicSourceBase — calls AccumulateSample() every tick, broadcasts
//     BuildPayload() every periodSec, then calls Reset().
//
//   EventDrivenSourceBase — derived class calls Push(payload) when an event
//     occurs; base handles optional rate limiting.
//
//   OnDemandSourceBase — registers a query handler on Activate; derived class
//     implements HandleQuery(args) for pull-based responses.
////////////////////////////////////////////////////////////////////////////////
#include "DiaDebugServer/IInspectorDataSource.h"
#include "DiaDebugServer/DebugServer.h"

namespace Dia { namespace DebugServer {

//-----------------------------------------------------------------------------
// ChangeDetectedSourceBase
//-----------------------------------------------------------------------------
class ChangeDetectedSourceBase : public IInspectorDataSource
{
public:
    void Activate(DebugServer* server) override
    {
        mServer = server;
        mLastHash = 0;
        mLastSubCount = -1;
    }

    void Tick(float /*deltaTime*/, int connectionCount, int subscriptionCount) override
    {
        if (!mServer || connectionCount == 0) return;

        if (subscriptionCount != mLastSubCount)
        {
            mForceResend  = true;
            mLastSubCount = subscriptionCount;
        }

        Json::Value payload;
        unsigned int hash = CollectAndHash(payload);
        if (!mForceResend && hash == mLastHash) return;
        mForceResend = false;
        mLastHash    = hash;

        Broadcast(payload);
    }

    void Deactivate() override { mServer = nullptr; }

protected:
    // Return a hash of current state and populate payload. Called every tick
    // when connections > 0. Payload is only sent when hash differs.
    virtual unsigned int CollectAndHash(Json::Value& payload) = 0;

    void Broadcast(const Json::Value& payload)
    {
        if (mServer)
            mServer->NotifySubscribers(GetTopic(), payload);
    }

    static unsigned int HashCombine(unsigned int hash, unsigned int value)
    {
        return hash ^ (value + 0x9e3779b9 + (hash << 6) + (hash >> 2));
    }

private:
    DebugServer*  mServer       = nullptr;
    unsigned int  mLastHash     = 0;
    int           mLastSubCount = -1;
    bool          mForceResend  = false;
};

//-----------------------------------------------------------------------------
// PeriodicSourceBase
//-----------------------------------------------------------------------------
class PeriodicSourceBase : public IInspectorDataSource
{
public:
    void Activate(DebugServer* server) override
    {
        mServer = server;
        mAccSec = 0.0f;
        Reset();
    }

    void Tick(float deltaTime, int connectionCount, int /*subscriptionCount*/) override
    {
        if (!mServer) return;

        if (connectionCount > 0)
            AccumulateSample(deltaTime);

        mAccSec += deltaTime;
        if (mAccSec >= GetPolicy().periodSec)
        {
            mAccSec = 0.0f;
            if (connectionCount > 0)
            {
                Json::Value payload = BuildPayload();
                mServer->NotifySubscribers(GetTopic(), payload);
            }
            Reset();
        }
    }

    void Deactivate() override { mServer = nullptr; }

protected:
    virtual void       AccumulateSample(float deltaTime) = 0;
    virtual Json::Value BuildPayload() = 0;
    virtual void       Reset() = 0;

private:
    DebugServer* mServer = nullptr;
    float        mAccSec = 0.0f;
};

//-----------------------------------------------------------------------------
// EventDrivenSourceBase
//-----------------------------------------------------------------------------
class EventDrivenSourceBase : public IInspectorDataSource
{
public:
    void Activate(DebugServer* server) override
    {
        mServer = server;
        mWindowEventsCount = 0;
        mWindowAccSec = 0.0f;
    }

    void Tick(float deltaTime, int /*connectionCount*/, int /*subscriptionCount*/) override
    {
        if (!mServer) return;
        const int maxPerWindow = GetPolicy().maxEventsPerWindow;
        if (maxPerWindow > 0)
        {
            mWindowAccSec += deltaTime;
            if (mWindowAccSec >= GetPolicy().rateLimitWindowSec)
            {
                mWindowAccSec      = 0.0f;
                mWindowEventsCount = 0;
            }
        }
    }

    void Deactivate() override { mServer = nullptr; }

protected:
    DebugServer* GetServer() const { return mServer; }

    void Push(const Json::Value& payload)
    {
        if (!mServer) return;
        const int maxPerWindow = GetPolicy().maxEventsPerWindow;
        if (maxPerWindow > 0 && mWindowEventsCount >= maxPerWindow) return;
        ++mWindowEventsCount;
        mServer->NotifySubscribers(GetTopic(), payload);
    }

private:
    DebugServer* mServer            = nullptr;
    int          mWindowEventsCount = 0;
    float        mWindowAccSec      = 0.0f;
};

//-----------------------------------------------------------------------------
// OnDemandSourceBase
//-----------------------------------------------------------------------------
class OnDemandSourceBase : public IInspectorDataSource
{
public:
    SourcePolicy GetPolicy() const override
    {
        return { SourceStrategy::kOnDemand, 0.0f, 0, 0.0f };
    }

    void Activate(DebugServer* server) override
    {
        mServer = server;
        mServer->GetQueryRegistry().Register(GetTopic(),
            [this](const Json::Value& args) { return HandleQuery(args); });
    }

    void Tick(float /*dt*/, int /*cc*/, int /*sc*/) override {}

    void Deactivate() override { mServer = nullptr; }

protected:
    virtual Json::Value HandleQuery(const Json::Value& args) = 0;

private:
    DebugServer* mServer = nullptr;
};

}} // namespace Dia::DebugServer
