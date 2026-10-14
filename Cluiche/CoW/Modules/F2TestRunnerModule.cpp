#include "F2TestRunnerModule.h"
#include "F2StreamIndependence.h"
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaObservation/Log/DiaLog.h>
#include <cstring>

namespace CoW {

const Dia::Core::StringCRC F2TestRunnerModule::kTypeId("F2TestRunnerModule");

//-----------------------------------------------------------------------------
F2TestRunnerModule::F2TestRunnerModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{
}

//-----------------------------------------------------------------------------
Dia::ApplicationFlow::StartResult F2TestRunnerModule::DoStart()
{
    using Dia::ApplicationFlow::StartResult;

    Cluiche::AppFlow::MessageBusHostModule* host = mBus.Get();
    if (!host)
        return StartResult::kLoading;

    Dia::MessageBus::Bus& bus = host->GetBus();
    bus.RegisterType<F2PhaseEntered, 16>();
    bus.RegisterType<F2CheckCompleted, 16>();
    bus.RegisterType<F2ScenarioCompleted, 4>();
    bus.RegisterProducer<F2PhaseEntered>(GetInstanceId());
    bus.RegisterProducer<F2CheckCompleted>(GetInstanceId());
    bus.RegisterProducer<F2ScenarioCompleted>(GetInstanceId());
    mBusReady = true;

    DIA_LOG_INFO("CoW", "F2TestRunnerModule: ready");
    return StartResult::kReady;
}

//-----------------------------------------------------------------------------
Dia::ApplicationFlow::StopResult F2TestRunnerModule::DoStop()
{
    mRunPending = false;
    mRunning = false;
    mBusReady = false;
    return Dia::ApplicationFlow::StopResult::kDone;
}

//-----------------------------------------------------------------------------
bool F2TestRunnerModule::RunScenario()
{
    if (mRunning || mRunPending)
        return false;
    mRunPending = true;
    return true;
}

//-----------------------------------------------------------------------------
F2CheckStatus F2TestRunnerModule::GetCheckStatus(F2CheckId check) const
{
    const int i = static_cast<int>(check);
    return (i >= 0 && i < kF2CheckCount) ? mStatus[i] : F2CheckStatus::NotRun;
}

//-----------------------------------------------------------------------------
const char* F2TestRunnerModule::GetLastResult(F2CheckId check) const
{
    const int i = static_cast<int>(check);
    return (i >= 0 && i < kF2CheckCount) ? mLastResult[i] : "";
}

//-----------------------------------------------------------------------------
void F2TestRunnerModule::DoUpdate(const Dia::SimTime::SimTimeContext&)
{
    if (mRunPending)
    {
        mRunPending = false;
        mRunning = true;
        for (int i = 0; i < kF2CheckCount; ++i)
        {
            mStatus[i] = F2CheckStatus::NotRun;
            mLastResult[i][0] = '\0';
        }
        EnterPhase(F2PhaseId::Isolation);
        return;
    }

    if (!mRunning)
        return;

    const int next = static_cast<int>(mPhase) + 1;
    if (next < kF2PhaseCount)
    {
        EnterPhase(static_cast<F2PhaseId>(next));
        return;
    }

    uint32_t pass = 0, fail = 0;
    for (int i = 0; i < kF2CheckCount; ++i)
    {
        if (mStatus[i] == F2CheckStatus::Pass) ++pass;
        else if (mStatus[i] == F2CheckStatus::Fail) ++fail;
    }
    mRunning = false;
    mPhase = F2PhaseId::Idle;
    F2ScenarioCompleted done;
    done.passCount = pass;
    done.failCount = fail;
    Publish(done);
    DIA_LOG_INFO("CoW", "F2TestRunnerModule: scenario complete (%u pass, %u fail)", pass, fail);
}

//-----------------------------------------------------------------------------
void F2TestRunnerModule::EnterPhase(F2PhaseId phase)
{
    mPhase = phase;
    F2PhaseEntered msg;
    msg.phase = phase;
    Publish(msg);
    RunPhaseBody(phase);
}

//-----------------------------------------------------------------------------
void F2TestRunnerModule::RunPhaseBody(F2PhaseId phase)
{
    switch (phase)
    {
    case F2PhaseId::Isolation:       RunIsolation(); break; // TODO(plan tasks 4-7): C2 Determinism
    case F2PhaseId::Session:         break; // TODO(plan tasks 3-7): drive session (RNG draws, game days)
    case F2PhaseId::Save:            break; // TODO(plan tasks 3-7): capture save
    case F2PhaseId::ReferenceFuture: break; // TODO(plan tasks 3-7): record reference future
    case F2PhaseId::Reload:          break; // TODO(plan tasks 3-7): load save, C3 SaveLoadRng
    case F2PhaseId::Replay:          break; // TODO(plan tasks 3-7): replay, C4 ClockProof
    case F2PhaseId::Rerun:           break; // TODO(plan tasks 3-7): C5 SaveLoadProof
    case F2PhaseId::FailedLoad:      break; // TODO(plan tasks 3-7): failed-load path, C6 BandResolver
    default:                         break;
    }
}

//-----------------------------------------------------------------------------
// Phase 0: C1 stream independence. Runs on fresh streams built from the live seeds, so live state is untouched.
void F2TestRunnerModule::RunIsolation()
{
    RandomStreamModule* random = mRandom.Get();
    if (!random)
    {
        CompleteCheck(F2CheckId::StreamIndependence, false, "RandomStreamModule unavailable");
        return;
    }
    Dia::Maths::RandomStream* streams[kF2IndependenceStreamCount];
    for (int i = 0; i < kF2IndependenceStreamCount; ++i)
        streams[i] = random->GetStreamByName(Dia::Core::StringCRC(RandomStreamModule::kStreamNames[i]));

    const F2StreamIndependenceResult r = CheckStreamIndependence(streams);
    CompleteCheck(F2CheckId::StreamIndependence, r.passed, r.summary.c_str());
    if (!r.passed)
        DIA_LOG_ERROR("CoW", "F2 C1 failed: %s", r.summary.c_str());
}

//-----------------------------------------------------------------------------
void F2TestRunnerModule::CompleteCheck(F2CheckId check, bool passed, const char* summary)
{
    const int i = static_cast<int>(check);
    if (i < 0 || i >= kF2CheckCount)
        return;
    mStatus[i] = passed ? F2CheckStatus::Pass : F2CheckStatus::Fail;
    std::strncpy(mLastResult[i], summary ? summary : "", kSummaryLen - 1);
    mLastResult[i][kSummaryLen - 1] = '\0';

    F2CheckCompleted msg;
    msg.check = check;
    msg.passed = passed;
    std::strncpy(msg.summary, mLastResult[i], sizeof(msg.summary) - 1);
    Publish(msg);
}

//-----------------------------------------------------------------------------
template <class T>
void F2TestRunnerModule::Publish(const T& msg)
{
    Cluiche::AppFlow::MessageBusHostModule* host = mBus.Get();
    if (host && mBusReady)
        host->GetBus().Broadcast(msg);
}

} // namespace CoW

namespace { using F2TestRunnerModule_ = CoW::F2TestRunnerModule; }
DIA_MODULE(F2TestRunnerModule_);
DIA_DESCRIBE(F2TestRunnerModule_::kTypeId, "F-2 Core Runtime test runner: Save, Reload, Replay scenario with checks C1-C6.");
