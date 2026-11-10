#pragma once
////////////////////////////////////////////////////////////////////////////////
// SimQueryHostModule.h
//
// SimPU side of Sim-Hosted Queries (docs/specs/.../diadebugserver/sim-hosted-queries.md).
//
// Owns a Dia::DebugServer::SimQueryHost and adapts the two manifest-declared
// EventStreams to it:
//   SimQueryRequest  (Main -> Sim)  read  -> ISimRequestSource
//   SimQueryReply    (Sim -> Main)  write -> ISimReplySink
// Handlers run on the sim thread inside DoUpdate, so they can read SimPU-owned
// state with no locking.
//
// Sim modules register names through RegisterQuery / RegisterCommand.  Besides
// adding the handler to the host, this sends a registration message (a reply
// with the reserved requestId 0, see Types/SimRegistrationMessage.h) so the
// Main-side DebugServerHostModule starts routing that name to this module.
//
// Consumers on the SimPU resolve this module with ModuleRef<SimQueryHostModule>.
////////////////////////////////////////////////////////////////////////////////

#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaDebugServer/SimQueryHost.h>
#include <DiaStreams/Event.h>
#include <DiaStreams/EventStreamReader.h>
#include <DiaStreams/EventStreamWriter.h>
#include <DiaStreams/SendResult.h>

namespace Dia { namespace Observation { namespace Metric { class Counter; } } }

namespace Cluiche { namespace AppFlow {

class SimQueryHostModule
    : public Dia::ApplicationFlow::SimModule
    , private Dia::DebugServer::ISimRequestSource
    , private Dia::DebugServer::ISimReplySink
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Runs sim-hosted debug queries requested by the Main-side DebugServer";

    explicit SimQueryHostModule(const Dia::Core::StringCRC& instanceId);
    ~SimQueryHostModule() override;

    Dia::DebugServer::SimQueryHost& GetHost() { return mHost; }

    // Registers the handler on the host and announces the name to the Main-side DebugServer.
    // Returns false if the host rejected it (duplicate / full) or the name is not announceable.
    // A non-empty description is mandatory (see SimHelpInfo); the info is copied and stored on the host.
    bool RegisterQuery(const char* name, const Dia::DebugServer::SimHelpInfo& help, Dia::DebugServer::SimQueryHost::QueryHandler handler);
    bool RegisterCommand(const char* name, const Dia::DebugServer::SimHelpInfo& help, Dia::DebugServer::SimQueryHost::QueryHandler handler);

    // Removes the handler and tells the Main side to forget the name.
    void UnregisterQuery(const char* name);

    // Frames between periodic digests (SimPU frames), replay messages per frame, overflow log rate limit.
    static constexpr uint32_t kDigestIntervalFrames      = 30;
    static constexpr uint32_t kReplayPerFrame            = 16;
    static constexpr uint32_t kOverflowLogIntervalFrames = 30;

    // Announce-path writes that the stream did not deliver cleanly (dropped / rejected).
    uint32_t AnnounceOverflowCount() const { return mAnnounceOverflow; }

protected:
    // Single point of contact with the reply stream; tests override it to capture / simulate overflow.
    virtual Dia::ApplicationFlow::SendResult SendReply(const Dia::DebugServer::SimReplyEvent& reply);

    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    // ISimRequestSource
    bool TryPop(Dia::DebugServer::SimRequestEvent& out) override;
    // ISimReplySink
    bool PostReply(const Dia::DebugServer::SimReplyEvent& reply) override;

    bool RegisterInternal(const char* name, Dia::DebugServer::SimKind kind, const Dia::DebugServer::SimHelpInfo& help,
                          Dia::DebugServer::SimQueryHost::QueryHandler handler);
    // False only if the message could not be built (Register whose help does not fit the event); nothing is sent then.
    bool Announce(bool registerOp, const char* name, Dia::DebugServer::SimKind kind);

    // Sends an announce-path message (register/unregister/digest/resync_begin/replay): records overflow,
    // marks the frame as having announced.  Does not retry; digest + resync repair any loss.
    Dia::ApplicationFlow::SendResult PostAnnounce(const Dia::DebugServer::SimReplyEvent& msg);
    void RequestResync();
    void DrainReplay();
    void SendDigest();

    // Small batch: each event is ~16 KB, so keep the stack/member footprint modest.
    static constexpr unsigned int kBatch        = 2;
    static constexpr unsigned int kMaxAnnounced = 64;
    static constexpr unsigned int kNameBytes    = 96;

    struct AnnouncedName
    {
        char                      name[kNameBytes];
        Dia::DebugServer::SimKind kind;
    };

    Dia::DebugServer::SimQueryHost mHost;
    Dia::ApplicationFlow::EventStreamReader<Dia::DebugServer::SimRequestEvent> mRequests{this, "SimQueryRequest"};
    Dia::ApplicationFlow::EventStreamWriter<Dia::DebugServer::SimReplyEvent>   mReplies{this, "SimQueryReply"};

    Dia::Core::Containers::DynamicArrayC<Dia::ApplicationFlow::Event<Dia::DebugServer::SimRequestEvent>, kBatch> mBatch;
    unsigned int mBatchIndex = 0;

    // Names announced to Main; re-announced to Unregister on stop.
    Dia::Core::Containers::DynamicArrayC<AnnouncedName, kMaxAnnounced> mAnnounced;

    // Per-host announcement sequence: +1 per register/unregister message (wraps).
    uint32_t mAnnounceSeq = 0;

    uint32_t mFramesSinceDigest = 0;
    bool     mAnnouncedThisFrame = false;   // an announce/replay message was sent since the last DoUpdate decision

    // Pending resync replay: resync_begin first, then mAnnounced[mReplayIndex..].  A register/unregister
    // or a second resync request restarts it, so no copy of the table is kept.
    bool     mReplayPending   = false;
    bool     mReplayBeginSent = false;
    unsigned int mReplayIndex = 0;

    uint32_t mAnnounceOverflow = 0;
    Dia::Observation::Metric::Counter* mMetricOverflow = nullptr;   // owned by MetricRegistry; Counter is thread-safe
    uint32_t mFrame = 0;
    uint32_t mLastOverflowLogFrame = 0;
    bool     mOverflowLogged = false;
};

} } // namespace Cluiche::AppFlow
