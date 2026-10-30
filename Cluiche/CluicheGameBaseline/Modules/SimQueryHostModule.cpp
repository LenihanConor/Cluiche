#include "Modules/SimQueryHostModule.h"

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>

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
}

SimQueryHostModule::~SimQueryHostModule()
{
    mHost.SetRequestSource(nullptr);
    mHost.SetReplySink(nullptr);
}

Dia::ApplicationFlow::StartResult SimQueryHostModule::DoStart()
{
    return Dia::ApplicationFlow::StartResult::kReady;
}

void SimQueryHostModule::DoUpdate(const Dia::SimTime::SimTimeContext& /*ctx*/)
{
    DIA_TRACE_ZONE("SimQueryHostModule.Update", Dia::Observation::Trace::Category::kDiaApplicationFlow);
    mHost.Update();
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

bool SimQueryHostModule::PostReply(const Dia::DebugServer::SimReplyEvent& reply)
{
    return Dia::ApplicationFlow::IsSuccess(mReplies.Send(reply));
}

bool SimQueryHostModule::RegisterQuery(const char* name, Dia::DebugServer::SimQueryHost::QueryHandler handler)
{
    return RegisterInternal(name, Dia::DebugServer::SimKind::Query, handler);
}

bool SimQueryHostModule::RegisterCommand(const char* name, Dia::DebugServer::SimQueryHost::QueryHandler handler)
{
    return RegisterInternal(name, Dia::DebugServer::SimKind::Command, handler);
}

bool SimQueryHostModule::RegisterInternal(const char* name, Dia::DebugServer::SimKind kind,
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
        ? mHost.RegisterCommand(crc, handler)
        : mHost.RegisterQuery(crc, handler);
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
                                   name, kind, *msg))
    {
        DIA_LOG_ERROR("DebugServer", "SimQueryHostModule: could not build registration message for '%s'", name);
        return;
    }
    if (!PostReply(*msg))
        DIA_LOG_WARNING("DebugServer", "SimQueryHostModule: registration message for '%s' was rejected by the stream", name);
}

} } // namespace Cluiche::AppFlow

namespace { using SimQueryHostModule_ = Cluiche::AppFlow::SimQueryHostModule; }
DIA_MODULE(SimQueryHostModule_);
DIA_DESCRIBE(SimQueryHostModule_::kTypeId, "Runs debug queries and commands that touch SimPU-owned state, requested over EventStreams by the Main-side DebugServer.");
