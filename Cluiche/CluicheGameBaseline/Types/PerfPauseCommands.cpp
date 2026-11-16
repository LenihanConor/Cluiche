#ifdef _DEBUG

#include "Types/PerfPauseCommands.h"

#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaObservation/Log/DiaLog.h>

#include <string>

namespace Cluiche { namespace AppFlow {

using Dia::ApplicationFlow::PerfPauseController;
using Dia::ApplicationFlow::PerfPauseKind;
using Dia::ApplicationFlow::PerfPauseResult;
using Dia::Core::StringCRC;

namespace
{
    const StringCRC kPauseName("perf.pause");
    const StringCRC kResumeName("perf.resume");
    const StringCRC kResumeAllName("perf.resume_all");

    Json::Value Fail(const char* reason)
    {
        Json::Value v(Json::objectValue);
        v["error"] = reason;
        return v;
    }

    // Domain ids are not validated: the pause controller cannot know which debug domains exist (they are
    // registered on SimPU), and pausing an unknown domain is a harmless no-op. Only a non-empty id is required.
    bool ParseKindAndId(const Json::Value& args, PerfPauseKind& kind, StringCRC& id, Json::Value& error)
    {
        if (!args.isObject() || !args["kind"].isString())
        {
            error = Fail("missing or non-string 'kind' (expected module or domain)");
            return false;
        }
        const std::string k = args["kind"].asString();
        if (k == "module")      kind = PerfPauseKind::kModule;
        else if (k == "domain") kind = PerfPauseKind::kDomain;
        else
        {
            error = Fail("invalid 'kind' (expected module or domain)");
            return false;
        }
        if (!args["id"].isString() || args["id"].asString().empty())
        {
            error = Fail("missing or empty 'id'");
            return false;
        }
        id = StringCRC(args["id"].asCString());
        return true;
    }

    const Dia::API::ValueParam kKindIdParams[] = {
        { "kind", "enum",   true, "What to address: a module instance or a debug domain", nullptr, false, 0.0f, 0.0f, "module|domain" },
        { "id",   "string", true, "Module instance id, or debug domain id", nullptr, false, 0.0f, 0.0f, nullptr },
    };
}

PerfPauseCommands::PerfPauseCommands(PerfPauseController& controller, Dia::DebugServer::DebugServer& server)
    : mController(controller)
    , mServer(server)
    , mRegistered(false)
{
}

PerfPauseCommands::~PerfPauseCommands()
{
    Unregister();
}

uint32_t PerfPauseCommands::OwnerForConn(int connId)
{
    return connId < 0 ? PerfPauseController::kLocalOwner : static_cast<uint32_t>(connId) + 1u;
}

void PerfPauseCommands::OnDisconnect(void* context, int connId)
{
    PerfPauseCommands* self = static_cast<PerfPauseCommands*>(context);
    self->mController.ReleaseOwner(OwnerForConn(connId));
    DIA_LOG_INFO("PerfPause", "Released pauses of disconnected client connId=%d", connId);
}

void PerfPauseCommands::Register()
{
    if (mRegistered) return;
    mRegistered = true;

    mServer.AddDisconnectObserver(&PerfPauseCommands::OnDisconnect, this);

    {
        Dia::API::CommandInfoJson c;
        c.name        = kPauseName;
        c.description = "Pause a module (skips its DoUpdate) or a debug domain (excluded from snapshots); released when this client disconnects";
        c.category    = StringCRC("perf");
        c.owner       = "CluicheGameBaseline";
        c.params      = kKindIdParams;
        c.paramCount  = 2;
        c.callback    = [this](const Json::Value& args) { return HandlePause(mServer.GetCurrentCommandConnId(), args); };
        Dia::API::RegisterCommandJson(c);
    }
    {
        Dia::API::CommandInfoJson c;
        c.name        = kResumeName;
        c.description = "Resume a module or debug domain this client paused";
        c.category    = StringCRC("perf");
        c.owner       = "CluicheGameBaseline";
        c.params      = kKindIdParams;
        c.paramCount  = 2;
        c.callback    = [this](const Json::Value& args) { return HandleResume(mServer.GetCurrentCommandConnId(), args); };
        Dia::API::RegisterCommandJson(c);
    }
    {
        Dia::API::CommandInfoJson c;
        c.name        = kResumeAllName;
        c.description = "Resume everything this client paused";
        c.category    = StringCRC("perf");
        c.owner       = "CluicheGameBaseline";
        c.noArgs      = true;
        c.callback    = [this](const Json::Value&) { return HandleResumeAll(mServer.GetCurrentCommandConnId()); };
        Dia::API::RegisterCommandJson(c);
    }
}

void PerfPauseCommands::Unregister()
{
    if (!mRegistered) return;
    mRegistered = false;
    mServer.RemoveDisconnectObserver(&PerfPauseCommands::OnDisconnect, this);
    Dia::API::UnregisterCommandJson(kPauseName);
    Dia::API::UnregisterCommandJson(kResumeName);
    Dia::API::UnregisterCommandJson(kResumeAllName);
}

Json::Value PerfPauseCommands::HandlePause(int connId, const Json::Value& args)
{
    PerfPauseKind kind = PerfPauseKind::kModule;
    StringCRC id;
    Json::Value error;
    if (!ParseKindAndId(args, kind, id, error)) return error;

    const PerfPauseResult r = mController.Pause(OwnerForConn(connId), kind, id);
    if (!r.ok)
    {
        DIA_LOG_WARNING("PerfPause", "perf.pause '%s' refused: %s", args["id"].asCString(), r.reason ? r.reason : "unknown");
        return Fail(r.reason ? r.reason : "pause refused");
    }
    DIA_LOG_INFO("PerfPause", "perf.pause %s '%s' by connId=%d", args["kind"].asCString(), args["id"].asCString(), connId);
    Json::Value out(Json::objectValue);
    out["kind"]   = args["kind"];
    out["id"]     = args["id"];
    out["paused"] = true;
    return out;
}

Json::Value PerfPauseCommands::HandleResume(int connId, const Json::Value& args)
{
    PerfPauseKind kind = PerfPauseKind::kModule;
    StringCRC id;
    Json::Value error;
    if (!ParseKindAndId(args, kind, id, error)) return error;

    const PerfPauseResult r = mController.Resume(OwnerForConn(connId), kind, id);
    if (!r.ok) return Fail(r.reason ? r.reason : "resume refused");
    DIA_LOG_INFO("PerfPause", "perf.resume %s '%s' by connId=%d", args["kind"].asCString(), args["id"].asCString(), connId);
    Json::Value out(Json::objectValue);
    out["kind"]   = args["kind"];
    out["id"]     = args["id"];
    out["paused"] = mController.IsPaused(kind, id);   // true if another client still holds it
    return out;
}

Json::Value PerfPauseCommands::HandleResumeAll(int connId)
{
    mController.ResumeAll(OwnerForConn(connId));
    DIA_LOG_INFO("PerfPause", "perf.resume_all by connId=%d", connId);
    return Json::Value(Json::objectValue);
}

}} // namespace Cluiche::AppFlow

#endif // _DEBUG
