#pragma once
////////////////////////////////////////////////////////////////////////////////
// Filename: PerfPauseCommands.h
//
// Perf live control (task 5): Main-hosted DiaAPI commands perf.pause, perf.resume
// and perf.resume_all.  Pauses are owned per DebugServer client: a client's pauses
// are released when it disconnects (DebugServer::AddDisconnectObserver).
//
// Debug only (_DEBUG): in Release the whole class is compiled out, so the commands
// are not registered and no PerfPause symbols are referenced.  Do NOT gate on
// DIA_DEBUG (DiaVisualDebugger defines it in Release).
//
// Main thread only.  Does not own the controller or the server; both must outlive it.
////////////////////////////////////////////////////////////////////////////////
#ifdef _DEBUG

#include <DiaApplicationFlow/PerfPause.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaDebugServer/DebugServer.h>

#include <cstdint>

namespace Cluiche { namespace AppFlow {

class PerfPauseCommands
{
public:
    PerfPauseCommands(Dia::ApplicationFlow::PerfPauseController& controller, Dia::DebugServer::DebugServer& server);
    ~PerfPauseCommands();

    PerfPauseCommands(const PerfPauseCommands&) = delete;
    PerfPauseCommands& operator=(const PerfPauseCommands&) = delete;

    // Registers the three DiaAPI commands and the disconnect observer.  Idempotent.
    void Register();
    // Removes them again; pauses stay until released by their owner.  Idempotent.
    void Unregister();

    // Owner token for a DebugServer connection.  Never equals PerfPauseController::kLocalOwner (0):
    // connId + 1 for connId >= 0; a negative connId (no DebugServer client, e.g. in-process caller) maps to kLocalOwner.
    static uint32_t OwnerForConn(int connId);

    // Handlers; connId is the calling client (negative = local).  Failures return {"error": reason}.
    Json::Value HandlePause(int connId, const Json::Value& args);
    Json::Value HandleResume(int connId, const Json::Value& args);
    Json::Value HandleResumeAll(int connId);

private:
    static void OnDisconnect(void* context, int connId);

    Dia::ApplicationFlow::PerfPauseController& mController;
    Dia::DebugServer::DebugServer&             mServer;
    bool                                       mRegistered;
};

}} // namespace Cluiche::AppFlow

#endif // _DEBUG
