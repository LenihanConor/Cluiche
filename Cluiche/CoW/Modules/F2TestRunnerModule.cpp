#include "F2TestRunnerModule.h"
#include "F2StreamIndependence.h"
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaObservation/Log/DiaLog.h>
#include <cstring>
#include <cstdio>
#include <string>

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
    // Messages phase 1 drives and observes. GameSpeedControllerModule registers the same types
    // (same capacity); a duplicate registration is a harmless no-op that returns false.
    bus.RegisterType<SimulationTick, 16>();
    bus.RegisterType<SpeedChangeRequested, 16>();
    bus.RegisterType<ManualAdvanceRequested, 16>();
    bus.RegisterType<ModalOpened, 16>();
    bus.RegisterType<ModalClosed, 16>();
    bus.RegisterProducer<SpeedChangeRequested>(GetInstanceId());
    bus.RegisterProducer<ManualAdvanceRequested>(GetInstanceId());
    bus.RegisterProducer<ModalOpened>(GetInstanceId());
    bus.RegisterProducer<ModalClosed>(GetInstanceId());
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
    mTickSub = Dia::MessageBus::BusSubscriptionHandle();
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
void F2TestRunnerModule::ResetScenarioState()
{
    mTickSub = Dia::MessageBus::BusSubscriptionHandle();
    mPhaseDone = false;
    mScenarioTick = 0;
    mScriptIndex = 0;
    mModel = F2ExpectedClock{};
    mTicksThisUpdate = 0;
    mLastTickDay = 0;
    mC4Transitions = 0;
    mC4Matched = 0;
    mC4TickViolations = 0;
    mC4FirstFailure.clear();
    mCheckPending = false;
    mSaveRequested = false;
    mSaveCountBefore = 0;
    mSaveWaitUpdates = 0;
    mSaveSucceeded = false;
    mHasF500 = false;
    mHasF1000 = false;
    mHasReferenceDraws = false;
    mF500 = F2Fingerprint{};
    mF1000 = F2Fingerprint{};
    mReferenceDraws.fill(0);
}

//-----------------------------------------------------------------------------
void F2TestRunnerModule::DoUpdate(const Dia::SimTime::SimTimeContext& ctx)
{
    mCtxTimeScale = ctx.timeScale;

    if (mRunPending)
    {
        mRunPending = false;
        mRunning = true;
        for (int i = 0; i < kF2CheckCount; ++i)
        {
            mStatus[i] = F2CheckStatus::NotRun;
            mLastResult[i][0] = '\0';
        }
        ResetScenarioState();
        EnterPhase(F2PhaseId::Isolation);
        return;
    }

    if (!mRunning)
        return;

    if (!mPhaseDone)
    {
        RunPhaseBody(mPhase);   // multi-update phase still in progress
        return;
    }

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
    mPhaseDone = false;
    if (phase == F2PhaseId::ReferenceFuture)
        mScriptIndex = 0;   // phase 3 walks the remainder script from its start
    F2PhaseEntered msg;
    msg.phase = phase;
    Publish(msg);
    RunPhaseBody(phase);
}

//-----------------------------------------------------------------------------
void F2TestRunnerModule::RunPhaseBody(F2PhaseId phase)
{
    // Single-update phases are done once their body ran; phases 1-3 clear mPhaseDone until finished.
    mPhaseDone = true;
    switch (phase)
    {
    case F2PhaseId::Isolation:       RunIsolation(); break; // TODO(plan tasks 4-7): C2 Determinism
    case F2PhaseId::Session:         RunSession(); break;
    case F2PhaseId::Save:            RunSave(); break;
    case F2PhaseId::ReferenceFuture: RunReferenceFuture(); break;
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
bool F2TestRunnerModule::IssueAction(const F2ScenarioAction& action)
{
    Cluiche::AppFlow::MessageBusHostModule* host = mBus.Get();
    if (!host)
        return false;
    Dia::MessageBus::Bus& bus = host->GetBus();
    switch (action.kind)
    {
    case F2ActionKind::SetSpeed:
    case F2ActionKind::PlayerResume:
    {
        SpeedChangeRequested m;
        m.tier = action.tier;
        return bus.Broadcast(m);
    }
    case F2ActionKind::PlayerPause:
    {
        SpeedChangeRequested m;
        m.tier = SpeedTier::Paused;
        return bus.Broadcast(m);
    }
    case F2ActionKind::OpenModal:  return bus.Broadcast(ModalOpened{});
    case F2ActionKind::CloseModal: return bus.Broadcast(ModalClosed{});
    case F2ActionKind::ManualStep: return bus.Broadcast(ManualAdvanceRequested{});
    }
    return false;
}

//-----------------------------------------------------------------------------
void F2TestRunnerModule::DrawForTick(uint32_t tick)
{
    RandomStreamModule* random = mRandom.Get();
    if (!random)
        return;
    uint8_t streams[kF2MaxDrawsPerTick] = {};
    const int count = F2DrawsForTick(tick, streams);
    for (int i = 0; i < count; ++i)
    {
        if (Dia::Maths::RandomStream* s = random->GetStreamByName(Dia::Core::StringCRC(RandomStreamModule::kStreamNames[streams[i]])))
            s->RandomInt();
    }
}

//-----------------------------------------------------------------------------
void F2TestRunnerModule::RecordC4Failure(const std::string& what)
{
    if (mC4FirstFailure.empty())
        mC4FirstFailure = what;
}

//-----------------------------------------------------------------------------
// Settled check for the action issued kF2SettleTicks ago: the live controller must match the
// reference model, and a manual step must have advanced exactly the expected number of days.
void F2TestRunnerModule::EvaluatePendingClockCheck()
{
    mCheckPending = false;
    ++mC4Transitions;

    GameSpeedControllerModule* gs = mGameSpeed.Get();
    if (!gs)
    {
        RecordC4Failure("GameSpeedControllerModule unavailable");
        return;
    }
    const GameSpeedController& c = gs->GetController();
    const GameSpeedState& st = c.GetState();

    F2ObservedClock o;
    o.tier = st.speedTier;
    o.source = st.pauseSource;
    o.modalDepth = st.modalDepth;
    o.controllerScale = c.GetTimeScale();
    o.domainScale = mCtxTimeScale;

    std::string mismatch = F2CompareClock(mModel, o, c.GetConfig());
    if (mismatch.empty() && mCheckAction.kind == F2ActionKind::ManualStep)
    {
        const uint64_t expectedDays = mCheckStepAccepted ? 1u : 0u;
        const uint64_t actualDays = st.dayCounter - mCheckDayAtAction;
        if (actualDays != expectedDays)
        {
            char buf[80];
            std::snprintf(buf, sizeof(buf), "manual step advanced %llu days, expected %llu",
                          static_cast<unsigned long long>(actualDays), static_cast<unsigned long long>(expectedDays));
            mismatch = buf;
        }
    }

    if (mismatch.empty())
    {
        ++mC4Matched;
        return;
    }
    char buf[160];
    std::snprintf(buf, sizeof(buf), "tick %u %s: %s", mCheckAction.tick, F2ActionKindName(mCheckAction.kind), mismatch.c_str());
    RecordC4Failure(buf);
    DIA_LOG_ERROR("CoW", "F2 C4 mismatch: %s", buf);
}

//-----------------------------------------------------------------------------
// Phase 1: one scenario tick per call. Issues scripted actions as real bus messages, asserts each one
// after kF2SettleTicks, audits the SimulationTick stream, then draws the tick's RNG pattern.
// Ordering: the bus delivers before this module's update, so a message sent on tick T has reached the
// controller by tick T+1; the controller applies the scale on its own update and the domain on the next.
void F2TestRunnerModule::RunSession()
{
    mPhaseDone = false;
    GameSpeedControllerModule* gs = mGameSpeed.Get();
    Cluiche::AppFlow::MessageBusHostModule* host = mBus.Get();

    if (mScenarioTick == 0 && !mTickSub.IsValid())
    {
        // First call of the phase: subscribe to SimulationTick for the one-per-frame audit.
        mTicksThisUpdate = 0;
        if (gs && host)
        {
            mLastTickDay = gs->GetController().GetState().dayCounter;
            mTickSub = host->GetBus().Subscribe<SimulationTick>(GetInstanceId(),
                [this](const SimulationTick& t) {
                    ++mTicksThisUpdate;
                    if (t.day != mLastTickDay + 1)
                    {
                        ++mC4TickViolations;
                        char buf[96];
                        std::snprintf(buf, sizeof(buf), "SimulationTick day %llu after day %llu (skipped or repeated)",
                                      static_cast<unsigned long long>(t.day), static_cast<unsigned long long>(mLastTickDay));
                        RecordC4Failure(buf);
                    }
                    mLastTickDay = t.day;
                });
        }
        if (!gs || !host || !mTickSub.IsValid())
        {
            CompleteCheck(F2CheckId::ClockProof, false, "GameSpeedController or bus unavailable");
            mPhaseDone = true;   // nothing to drive; later phases still run and report their own failures
            return;
        }
    }

    if (mScenarioTick >= kF2SessionTicks)
    {
        mTickSub = Dia::MessageBus::BusSubscriptionHandle();
        const bool pass = mC4FirstFailure.empty() && mC4TickViolations == 0 && mC4Transitions == F2SessionScript().size();
        char summary[kSummaryLen];
        if (pass)
            std::snprintf(summary, sizeof(summary), "%u / %u transitions match, 0 double ticks over %u ticks",
                          mC4Matched, mC4Transitions, kF2SessionTicks);
        else
            std::snprintf(summary, sizeof(summary), "%u / %u match; %s", mC4Matched, mC4Transitions, mC4FirstFailure.c_str());
        CompleteCheck(F2CheckId::ClockProof, pass, summary);
        mPhaseDone = true;
        return;
    }

    // At most one SimulationTick per sim frame.
    if (mTicksThisUpdate > 1)
    {
        ++mC4TickViolations;
        char buf[64];
        std::snprintf(buf, sizeof(buf), "tick %u: %u SimulationTicks in one sim frame", mScenarioTick, mTicksThisUpdate);
        RecordC4Failure(buf);
    }
    mTicksThisUpdate = 0;

    if (mCheckPending && mScenarioTick >= mCheckDueTick)
        EvaluatePendingClockCheck();

    const std::vector<F2ScenarioAction>& script = F2SessionScript();
    while (mScriptIndex < script.size() && script[mScriptIndex].tick <= mScenarioTick)
    {
        const F2ScenarioAction& action = script[mScriptIndex++];
        mCheckStepAccepted = F2ManualStepAccepted(mModel);
        F2ApplyAction(mModel, action);
        mCheckAction = action;
        mCheckDayAtAction = gs->GetController().GetState().dayCounter;
        mCheckDueTick = mScenarioTick + kF2SettleTicks;
        mCheckPending = true;
        if (!IssueAction(action))
            RecordC4Failure("bus rejected a scripted action");
    }

    DrawForTick(mScenarioTick);
    ++mScenarioTick;
}

//-----------------------------------------------------------------------------
F2Fingerprint F2TestRunnerModule::CaptureFingerprint(bool& ok)
{
    RandomStreamModule* random = mRandom.Get();
    GameSpeedControllerModule* gs = mGameSpeed.Get();
    ok = (random != nullptr && gs != nullptr);
    return ok ? F2Fingerprint::Capture(*random, *gs) : F2Fingerprint{};
}

//-----------------------------------------------------------------------------
// Phase 2: request `cow.save quick` through SaveLoadModule (the API the command uses) and wait for the
// save to complete (it runs at a frame boundary, so one or two updates) before capturing F500. The
// runner draws nothing and the clock is stopped while it waits, so F500 describes the saved state.
// F500 is the FULL fingerprint; since GameSpeedController restores only the day counter and always
// resumes paused, the reload comparison (task 5) uses the restored fields only.
void F2TestRunnerModule::RunSave()
{
    constexpr uint32_t kMaxSaveWaitUpdates = 10;
    mPhaseDone = false;
    SaveLoadModule* save = mSaveLoad.Get();
    if (!save)
    {
        DIA_LOG_ERROR("CoW", "F2: SaveLoadModule unavailable, no save taken");
        mSaveSucceeded = false;
        bool ok = false;
        mF500 = CaptureFingerprint(ok);
        mHasF500 = ok;
        mPhaseDone = true;
        return;
    }

    if (!mSaveRequested)
    {
        mSaveCountBefore = save->GetSaveCount();
        mSaveRequested = save->RequestSave(Dia::Core::StringCRC("quick"));
        mSaveWaitUpdates = 0;
        if (!mSaveRequested)
        {
            DIA_LOG_ERROR("CoW", "F2: quick save request rejected");
            mPhaseDone = true;
        }
        return;
    }

    ++mSaveWaitUpdates;
    const bool completed = save->GetSaveCount() > mSaveCountBefore;
    if (completed || mSaveWaitUpdates >= kMaxSaveWaitUpdates)
    {
        mSaveSucceeded = completed && save->GetLastSaveResult().Ok();
        if (!mSaveSucceeded)
            DIA_LOG_ERROR("CoW", "F2: quick save did not complete successfully after %u updates", mSaveWaitUpdates);
        bool ok = false;
        mF500 = CaptureFingerprint(ok);
        mHasF500 = ok;
        mPhaseDone = true;
    }
}

//-----------------------------------------------------------------------------
// Phase 3: scenario ticks [500, 1000) with the remainder script (no C4 assertions; C4 is phase 1). On
// the update after tick 999 capture F1000 FIRST, then record the next 100 draws from each stream
// (800 values, stream-major). The streams are not rewound afterwards: the Reload phase restores them
// from the save, and the replay must draw in this same order right after reproducing F1000.
void F2TestRunnerModule::RunReferenceFuture()
{
    mPhaseDone = false;
    if (mScenarioTick >= kF2ScenarioTicks)
    {
        bool ok = false;
        mF1000 = CaptureFingerprint(ok);
        mHasF1000 = ok;
        if (RandomStreamModule* random = mRandom.Get())
        {
            for (int s = 0; s < RandomStreamModule::kStreamCount; ++s)
            {
                Dia::Maths::RandomStream* stream = random->GetStreamByName(Dia::Core::StringCRC(RandomStreamModule::kStreamNames[s]));
                for (int n = 0; n < kF2ReferenceDrawsPerStream; ++n)
                    mReferenceDraws[s * kF2ReferenceDrawsPerStream + n] = stream ? stream->RandomInt() : 0;
            }
            mHasReferenceDraws = ok;
        }
        mPhaseDone = true;
        return;
    }

    const std::vector<F2ScenarioAction>& script = F2RemainderScript();
    while (mScriptIndex < script.size() && script[mScriptIndex].tick <= mScenarioTick)
    {
        F2ApplyAction(mModel, script[mScriptIndex]);
        IssueAction(script[mScriptIndex]);
        ++mScriptIndex;
    }
    DrawForTick(mScenarioTick);
    ++mScenarioTick;
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
