#include "Modules/SimQueryHostModule.h"

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Metric/MetricRegistry.h>

#include "Types/SimRegistrationMessage.h"

#include <cstring>
#include <memory>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC SimQueryHostModule::kTypeId("SimQueryHostModule");

SimQueryHostModule::SimQueryHostModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{
    mHost.SetRequestSource(this);
    mHost.SetReplySink(this);
    mHost.SetResyncHandler([this]() { RequestResync(); });
}

SimQueryHostModule::~SimQueryHostModule()
{
    mHost.SetResyncHandler(nullptr);
    mHost.SetRequestSource(nullptr);
    mHost.SetReplySink(nullptr);
}

Dia::ApplicationFlow::StartResult SimQueryHostModule::DoStart()
{
    mMetricOverflow = Dia::Observation::Metric::MetricRegistry::Instance().RegisterCounter(
        Dia::Core::StringCRC("dia.debugserver.sim.announce_overflow"),
        Dia::Observation::Metric::MetricInfo{ "Sim announce overflow",
            "Announce-path messages the SimQueryReply stream dropped or rejected (digest and resync repair them)", "count" });
    return Dia::ApplicationFlow::StartResult::kReady;
}

void SimQueryHostModule::DoUpdate(const Dia::SimTime::SimTimeContext& /*ctx*/)
{
    DIA_TRACE_ZONE("SimQueryHostModule.Update", Dia::Observation::Trace::Category::kDiaApplicationFlow);
    ++mFrame;
    ++mFramesSinceDigest;
    mHost.Update();   // may queue a resync via the resync handler
    DrainReplay();

    // Periodic digest, only on a quiet frame (no announce/replay message since the last decision).
    if (mFramesSinceDigest >= kDigestIntervalFrames && !mAnnouncedThisFrame)
        SendDigest();
    mAnnouncedThisFrame = false;
}

void SimQueryHostModule::SendDigest()
{
    uint32_t hash = 0;
    for (unsigned int i = 0; i < mAnnounced.Size(); ++i)
        hash += Dia::DebugServer::SimNameSetEntryHash(mAnnounced[i].name, mAnnounced[i].kind);

    std::unique_ptr<Dia::DebugServer::SimReplyEvent> msg(new Dia::DebugServer::SimReplyEvent);
    std::memset(msg.get(), 0, sizeof(Dia::DebugServer::SimReplyEvent));
    if (BuildSimDigestReply(mAnnounceSeq, static_cast<uint32_t>(mAnnounced.Size()), hash, *msg))
        PostAnnounce(*msg);
    mFramesSinceDigest = 0;
}

void SimQueryHostModule::RequestResync()
{
    mReplayPending   = true;
    mReplayBeginSent = false;
    mReplayIndex     = 0;
}

void SimQueryHostModule::DrainReplay()
{
    if (!mReplayPending)
        return;

    std::unique_ptr<Dia::DebugServer::SimReplyEvent> msg(new Dia::DebugServer::SimReplyEvent);
    uint32_t budget = kReplayPerFrame;

    if (!mReplayBeginSent && budget > 0)
    {
        std::memset(msg.get(), 0, sizeof(Dia::DebugServer::SimReplyEvent));
        if (BuildSimResyncBeginReply(mAnnounceSeq, static_cast<uint32_t>(mAnnounced.Size()), *msg))
            PostAnnounce(*msg);
        mReplayBeginSent = true;
        --budget;
    }
    while (budget > 0 && mReplayIndex < mAnnounced.Size())
    {
        const AnnouncedName& e = mAnnounced[mReplayIndex++];
        std::memset(msg.get(), 0, sizeof(Dia::DebugServer::SimReplyEvent));
        if (BuildSimReplayReply(e.name, e.kind, *msg))
            PostAnnounce(*msg);
        --budget;
    }
    if (mReplayBeginSent && mReplayIndex >= mAnnounced.Size())
    {
        mReplayPending = false;
        DIA_LOG_INFO("DebugServer", "SimQueryHostModule: resync replay complete (%u names)",
                     static_cast<unsigned int>(mAnnounced.Size()));
    }
}

Dia::ApplicationFlow::SendResult SimQueryHostModule::PostAnnounce(const Dia::DebugServer::SimReplyEvent& msg)
{
    mAnnouncedThisFrame = true;
    const Dia::ApplicationFlow::SendResult result = SendReply(msg);
    if (result != Dia::ApplicationFlow::SendResult::kDelivered)
    {
        ++mAnnounceOverflow;
        if (mMetricOverflow != nullptr)
            mMetricOverflow->Inc();
        if (!mOverflowLogged || (mFrame - mLastOverflowLogFrame) >= kOverflowLogIntervalFrames)
        {
            mOverflowLogged = true;
            mLastOverflowLogFrame = mFrame;
            DIA_LOG_WARNING("DebugServer", "SimQueryHostModule: announce overflow, a message was dropped or rejected (total %u); digest/resync will repair",
                            mAnnounceOverflow);
        }
    }
    return result;
}

Dia::ApplicationFlow::StopResult SimQueryHostModule::DoStop()
{
    // Tell Main to stop routing the names this module announced.
    for (unsigned int i = 0; i < mAnnounced.Size(); ++i)
    {
        Announce(false, mAnnounced[i].name, mAnnounced[i].kind);
        mHost.UnregisterQuery(Dia::Core::StringCRC(mAnnounced[i].name));
    }
    mAnnounced.RemoveAll();
    mBatch.RemoveAll();
    mBatchIndex = 0;
    mMetricOverflow = nullptr;
    return Dia::ApplicationFlow::StopResult::kDone;
}

void SimQueryHostModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mRequests.Connect(app);
    mReplies.Connect(app);
}

bool SimQueryHostModule::TryPop(Dia::DebugServer::SimRequestEvent& out)
{
    if (mBatchIndex >= mBatch.Size())
    {
        mBatch.RemoveAll();
        mBatchIndex = 0;
        mRequests.Consume(mBatch);
        if (mBatch.Size() == 0)
            return false;
    }
    out = mBatch[mBatchIndex++].payload;
    return true;
}

Dia::ApplicationFlow::SendResult SimQueryHostModule::SendReply(const Dia::DebugServer::SimReplyEvent& reply)
{
    return mReplies.Send(reply);
}

bool SimQueryHostModule::PostReply(const Dia::DebugServer::SimReplyEvent& reply)
{
    return Dia::ApplicationFlow::IsSuccess(SendReply(reply));
}

bool SimQueryHostModule::RegisterQuery(const char* name, const Dia::DebugServer::SimHelpInfo& help, Dia::DebugServer::SimQueryHost::QueryHandler handler)
{
    return RegisterInternal(name, Dia::DebugServer::SimKind::Query, help, handler);
}

bool SimQueryHostModule::RegisterCommand(const char* name, const Dia::DebugServer::SimHelpInfo& help, Dia::DebugServer::SimQueryHost::QueryHandler handler)
{
    return RegisterInternal(name, Dia::DebugServer::SimKind::Command, help, handler);
}

bool SimQueryHostModule::RegisterInternal(const char* name, Dia::DebugServer::SimKind kind,
                                          const Dia::DebugServer::SimHelpInfo& help,
                                          Dia::DebugServer::SimQueryHost::QueryHandler handler)
{
    if (name == nullptr || name[0] == '\0' || std::strlen(name) >= kNameBytes || mAnnounced.IsFull())
    {
        DIA_LOG_ERROR("DebugServer", "SimQueryHostModule: cannot register '%s' (empty, too long, or table full)",
                      name != nullptr ? name : "(null)");
        return false;
    }

    const Dia::Core::StringCRC crc(name);
    const bool ok = (kind == Dia::DebugServer::SimKind::Command)
        ? mHost.RegisterCommand(crc, help, handler)
        : mHost.RegisterQuery(crc, help, handler);
    if (!ok)
        return false;

    AnnouncedName entry;
    std::memcpy(entry.name, name, std::strlen(name) + 1);
    entry.kind = kind;
    mAnnounced.Add(entry);

    Announce(true, name, kind);
    DIA_LOG_INFO("DebugServer", "SimQueryHostModule: registered sim-hosted %s '%s'",
                 kind == Dia::DebugServer::SimKind::Command ? "command" : "query", name);
    return true;
}

void SimQueryHostModule::UnregisterQuery(const char* name)
{
    if (name == nullptr)
        return;
    for (unsigned int i = 0; i < mAnnounced.Size(); ++i)
    {
        if (std::strcmp(mAnnounced[i].name, name) == 0)
        {
            Announce(false, name, mAnnounced[i].kind);
            mAnnounced.RemoveAt(i);
            break;
        }
    }
    mHost.UnregisterQuery(Dia::Core::StringCRC(name));
}

void SimQueryHostModule::Announce(bool registerOp, const char* name, Dia::DebugServer::SimKind kind)
{
    // Heap: the reply event is ~16 KB.
    std::unique_ptr<Dia::DebugServer::SimReplyEvent> msg(new Dia::DebugServer::SimReplyEvent);
    std::memset(msg.get(), 0, sizeof(Dia::DebugServer::SimReplyEvent));
    if (!BuildSimRegistrationReply(registerOp ? SimRegistrationOp::Register : SimRegistrationOp::Unregister,
                                   name, kind, ++mAnnounceSeq, *msg))
    {
        DIA_LOG_ERROR("DebugServer", "SimQueryHostModule: could not build registration message for '%s'", name);
        return;
    }
    // A register/unregister changes the set: restart any queued replay so it carries the new seq.
    if (mReplayPending)
        RequestResync();
    if (!Dia::ApplicationFlow::IsSuccess(PostAnnounce(*msg)))
        DIA_LOG_WARNING("DebugServer", "SimQueryHostModule: registration message for '%s' was rejected by the stream", name);
}

} } // namespace Cluiche::AppFlow

namespace { using SimQueryHostModule_ = Cluiche::AppFlow::SimQueryHostModule; }
DIA_MODULE(SimQueryHostModule_);
DIA_DESCRIBE(SimQueryHostModule_::kTypeId, "Runs debug queries and commands that touch SimPU-owned state, requested over EventStreams by the Main-side DebugServer.");
