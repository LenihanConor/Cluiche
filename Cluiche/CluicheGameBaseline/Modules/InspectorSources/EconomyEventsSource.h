#pragma once
#include <DiaDebugServer/IInspectorDataSource.h>
#include <DiaEconomy/IEconomyObserver.h>

namespace Dia { namespace DebugServer { class DebugServer; } }
namespace Dia { namespace Economy    { class EconomySystem; } }

namespace Cluiche { namespace AppFlow {

// Broadcasts economy.events — push-on-event feed that implements IEconomyObserver.
// Maintains a server-side ring buffer of depth 500.  On initial connect (new
// subscriber detected) the full ring is replayed with full_ring=true.  Subsequent
// observer callbacks send a single-event delta with full_ring=false.
//
// Thread safety: assumes single-threaded tick (SimPU).  Observer callbacks fire on
// the same thread; no additional synchronisation is applied in v1.
class EconomyEventsSource
    : public Dia::DebugServer::IInspectorDataSource
    , public Dia::Economy::IEconomyObserver
{
public:
    explicit EconomyEventsSource(Dia::Economy::EconomySystem& system);

    // IInspectorDataSource
    Dia::Core::StringCRC           GetTopic()  const override;
    Dia::DebugServer::SourcePolicy GetPolicy() const override;
    void Activate  (Dia::DebugServer::DebugServer* server)          override;
    void Tick      (float dt, int connectionCount, int subCount)     override;
    void Deactivate()                                                override;

    // IEconomyObserver
    void OnPoolChanged       (const Dia::Economy::PoolChangedEvent&)        override;
    void OnTransactionClamped(const Dia::Economy::TransactionClampedEvent&) override;
    void OnTransferCompleted (const Dia::Economy::TransferCompletedEvent&)  override;

protected:
    virtual void SendFullRing();
    virtual void SendDelta(const Json::Value& ev);

private:
    static constexpr unsigned int kRingDepth = 500;

    void PushToRing(const Json::Value& ev);

    Dia::Economy::EconomySystem&   mSystem;
    Dia::DebugServer::DebugServer* mServer       = nullptr;
    int                            mLastSubCount = -1;
    unsigned int                   mFrame        = 0;

    Json::Value  mRing[kRingDepth];
    unsigned int mRingCount = 0;
    unsigned int mRingHead  = 0;   // index of oldest entry (for ordered replay)
};

}} // namespace Cluiche::AppFlow
