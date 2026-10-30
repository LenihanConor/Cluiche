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
    bool RegisterQuery(const char* name, Dia::DebugServer::SimQueryHost::QueryHandler handler);
    bool RegisterCommand(const char* name, Dia::DebugServer::SimQueryHost::QueryHandler handler);

    // Removes the handler and tells the Main side to forget the name.
    void UnregisterQuery(const char* name);

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    // ISimRequestSource
    bool TryPop(Dia::DebugServer::SimRequestEvent& out) override;
    // ISimReplySink
    bool PostReply(const Dia::DebugServer::SimReplyEvent& reply) override;

    bool RegisterInternal(const char* name, Dia::DebugServer::SimKind kind,
                          Dia::DebugServer::SimQueryHost::QueryHandler handler);
    void Announce(bool registerOp, const char* name, Dia::DebugServer::SimKind kind);

    // Small batch: each event is ~16 KB, so keep the stack/member footprint modest.
    static constexpr unsigned int kBatch        = 2;
    static constexpr unsigned int kMaxAnnounced = 32;
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
};

} } // namespace Cluiche::AppFlow
