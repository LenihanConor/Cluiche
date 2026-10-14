#include "F2TestRunnerModule.h"
#include "F2StreamIndependence.h"
#include "F2DragonStub.h"
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaSaveGame/SlotManager.h>
#include <fstream>
#include <iterator>
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

#ifdef DIA_DEBUG
    mDebugDomain = std::make_unique<F2TestRunnerDebugDomain>(*this);
    mDomainRegistered = false;
#endif

    DIA_LOG_INFO("CoW", "F2TestRunnerModule: ready");
    return StartResult::kReady;
}

//-----------------------------------------------------------------------------
Dia::ApplicationFlow::StopResult F2TestRunnerModule::DoStop()
{
#ifdef DIA_DEBUG
    UnregisterDebugDomain();
    mDebugDomain.reset();
#endif
    mRunPending = false;
    mRunning = false;
    mTickSub = Dia::MessageBus::BusSubscriptionHandle();
    mBusReady = false;
    return Dia::ApplicationFlow::StopResult::kDone;
}

#ifdef DIA_DEBUG
//-----------------------------------------------------------------------------
void F2TestRunnerModule::TryRegisterDebugDomain()
{
    if (mDomainRegistered || !mDebugDomain)
        return;
    if (Cluiche::AppFlow::VisualDebuggerModule* vd = mVisualDebuggerRef.Get())
    {
        vd->RegisterDomain(*mDebugDomain);
        mDomainRegistered = true;
        DIA_LOG_INFO("CoW", "F2TestRunnerModule: TestRunnerPanel debug domain registered with the visual debugger");
    }
}

void F2TestRunnerModule::UnregisterDebugDomain()
{
    if (!mDomainRegistered || !mDebugDomain)
        return;
    if (Cluiche::AppFlow::VisualDebuggerModule* vd = mVisualDebuggerRef.Get())
        vd->UnregisterDomain(*mDebugDomain);
    mDomainRegistered = false;
}
#endif

//-----------------------------------------------------------------------------
bool F2TestRunnerModule::RunScenario()
{
    if (mRunning || mRunPending)
        return false;
    mRunPending = true;
    return true;
}

//-----------------------------------------------------------------------------
bool F2TestRunnerModule::RequestSetSpeed(SpeedTier tier)
{
    if (IsRunning())
        return false;
    F2ScenarioAction a;
    a.kind = F2ActionKind::SetSpeed;
    a.tier = tier;
    return IssueAction(a);
}

bool F2TestRunnerModule::RequestManualStep()
{
    if (IsRunning())
        return false;
    F2ScenarioAction a;
    a.kind = F2ActionKind::ManualStep;
    return IssueAction(a);
}

bool F2TestRunnerModule::RequestModal(bool open)
{
    if (IsRunning())
        return false;
    F2ScenarioAction a;
    a.kind = open ? F2ActionKind::OpenModal : F2ActionKind::CloseModal;
    return IssueAction(a);
}

bool F2TestRunnerModule::GetClockState(GameSpeedState& out) const
{
    const GameSpeedControllerModule* game = mGameSpeed.Get();
    if (!game)
        return false;
    out = game->GetController().GetState();
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
    mLoadRequested = false;
    mLoadCountBefore = 0;
    mLoadWaitUpdates = 0;
    mHasC5a = false;
    mC5aPassed = false;
    mC5aSummary[0] = '\0';
    mFReload = F2Fingerprint{};
    mFReplay1000 = F2Fingerprint{};
    mStartSaveDone = false;
    mStartSaveOk = false;
    mHasFStart = false;
    mFStart = F2Fingerprint{};
    mRerun = false;
    mRerunStage = RerunStage::LoadStart;
    mFRerun500 = F2Fingerprint{};
    mFRerun1000 = F2Fingerprint{};
    mFailStage = FailStage::Quiesce;
    mFailQuiesceUpdates = 0;
    mQuickBytesBefore.clear();
    mFLiveBefore = F2Fingerprint{};
    mMissingLoadFailed = false;
    mHasC5b = false;
    mC5bPassed = false;
    mC5bSummary[0] = '\0';
}

//-----------------------------------------------------------------------------
void F2TestRunnerModule::DoUpdate(const Dia::SimTime::SimTimeContext& ctx)
{
    mCtxTimeScale = ctx.timeScale;

#ifdef DIA_DEBUG
    TryRegisterDebugDomain();
#endif

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
    if (phase == F2PhaseId::Replay)
    {
        // The replay re-runs phase 3 exactly: same ticks, same script, same draw pattern.
        mScenarioTick = kF2SessionTicks;
        mScriptIndex = 0;
    }
    if (phase == F2PhaseId::Rerun)
    {
        mRerun = true;
        mRerunStage = RerunStage::LoadStart;
    }
    if (phase == F2PhaseId::FailedLoad)
    {
        mRerun = false;
        mFailStage = FailStage::Quiesce;
        mFailQuiesceUpdates = 0;
    }
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
    case F2PhaseId::Isolation:       RunIsolation(); break;
    case F2PhaseId::Session:         RunSession(); break;
    case F2PhaseId::Save:            RunSave(); break;
    case F2PhaseId::ReferenceFuture: RunReferenceFuture(); break;
    case F2PhaseId::Reload:          RunReload(); break;
    case F2PhaseId::Replay:          RunReplay(); break;
    case F2PhaseId::Rerun:           RunRerun(); break;
    case F2PhaseId::FailedLoad:      RunFailedLoad(); break;
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

    if (!mRerun && !mStartSaveDone)
    {
        // Tick 0, before any action or draw: the `start` save phase 6 reloads (there is no in-process
        // reset API). The runner draws and acts on nothing while it waits.
        const StepResult r = StepSave("start");
        if (r == StepResult::Pending)
            return;
        bool ok = false;
        mFStart = CaptureFingerprint(ok);
        mHasFStart = ok;
        mStartSaveOk = (r == StepResult::Ok);
        mStartSaveDone = true;
        if (!mStartSaveOk)
            DIA_LOG_ERROR("CoW", "F2: start save did not complete successfully");
        RunBandResolverCheck();   // C6 load path; touches no stream, clock or fingerprint state
        return;
    }

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
            if (mRerun)
                FinishC2(false, "GameSpeedController or bus unavailable");
            else
                CompleteCheck(F2CheckId::ClockProof, false, "GameSpeedController or bus unavailable");
            mPhaseDone = true;   // nothing to drive; later phases still run and report their own failures
            return;
        }
    }

    if (mScenarioTick >= kF2SessionTicks)
    {
        mTickSub = Dia::MessageBus::BusSubscriptionHandle();
        if (mRerun)
        {
            // The rerun repeats the same actions and draws, but C4 is not re-reported. Capture F500''
            // here: the same point as F500 (clock stopped, nothing in flight), before any save.
            bool ok = false;
            mFRerun500 = CaptureFingerprint(ok);
            mRerunStage = RerunStage::Remainder;
            mScriptIndex = 0;
            return;
        }
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
// C6 load path (the hatch path stays deferred until B6 exists). Loads a stub dragon record through the
// band resolver, then proves: live reads never re-resolve, editing bands without a band_schema_version
// bump changes nothing, and (negative control) a version bump + explicit reload is the one thing that
// re-resolves. Self-contained: no RNG draws, no clock or bus traffic, nothing in the fingerprints.
void F2TestRunnerModule::RunBandResolverCheck()
{
    F2DragonStub dragon;
    dragon.SetReResolveOnRead(mBandResolverFault);
    dragon.Load(F2DragonRecord{});

    constexpr F2DragonAttr kAttrs[kF2DragonAttrCount] = {
        F2DragonAttr::Health, F2DragonAttr::Strength, F2DragonAttr::Agility, F2DragonAttr::Fertility };
    float cached[kF2DragonAttrCount];
    for (int a = 0; a < kF2DragonAttrCount; ++a)
        cached[a] = dragon.Read(kAttrs[a]);

    bool stable = true;
    for (int pass = 0; pass < 3; ++pass)
        for (int a = 0; a < kF2DragonAttrCount; ++a)
            stable = stable && (dragon.Read(kAttrs[a]) == cached[a]);

    // Edit the in-memory bands without bumping band_schema_version: nothing may change.
    for (int a = 0; a < kF2DragonAttrCount; ++a)
        dragon.MutableRecord().bands[a] = F2Band::Apex;
    dragon.ReloadIfVersionChanged();   // version unchanged: must be a no-op
    for (int a = 0; a < kF2DragonAttrCount; ++a)
        stable = stable && (dragon.Read(kAttrs[a]) == cached[a]);
    const uint32_t invocations = dragon.GetResolverInvocations();

    // Negative control: the version bump is the only trigger, and it does re-resolve and change floats.
    // Not part of the pass count above; it proves the check can see a re-resolve at all.
    dragon.SetReResolveOnRead(false);
    dragon.MutableRecord().bandSchemaVersion += 1;
    const bool reloaded = dragon.ReloadIfVersionChanged();
    bool changed = false;
    for (int a = 0; a < kF2DragonAttrCount; ++a)
        changed = changed || (dragon.Read(kAttrs[a]) != cached[a]);
    const bool controlOk = reloaded && changed && dragon.GetResolverInvocations() == invocations + 1;

    char summary[kSummaryLen];
    bool pass = false;
    if (invocations != 1)
        std::snprintf(summary, sizeof(summary), "%u resolver invocations, expected 1", invocations);
    else if (!stable)
        std::snprintf(summary, sizeof(summary), "1 resolver invocation, but floats changed without a version bump");
    else if (!controlOk)
        std::snprintf(summary, sizeof(summary), "version bump + reload did not re-resolve (negative control)");
    else
    {
        pass = true;
        std::snprintf(summary, sizeof(summary), "1 resolver invocation, floats stable");
    }
    CompleteCheck(F2CheckId::BandResolver, pass, summary);
    if (!pass)
        DIA_LOG_ERROR("CoW", "F2 C6 failed: %s", summary);
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
    mPhaseDone = false;
    const StepResult r = StepSave("quick");
    if (r == StepResult::Pending)
        return;
    mSaveSucceeded = (r == StepResult::Ok);
    if (!mSaveSucceeded)
        DIA_LOG_ERROR("CoW", "F2: quick save did not complete successfully");
    bool ok = false;
    mF500 = CaptureFingerprint(ok);
    mHasF500 = ok;
    mPhaseDone = true;
}

//-----------------------------------------------------------------------------
F2TestRunnerModule::StepResult F2TestRunnerModule::StepSave(const char* slot)
{
    constexpr uint32_t kMaxSaveWaitUpdates = 10;
    SaveLoadModule* save = mSaveLoad.Get();
    if (!save)
        return StepResult::Rejected;

    if (!mSaveRequested)
    {
        mSaveCountBefore = save->GetSaveCount();
        mSaveWaitUpdates = 0;
        if (!save->RequestSave(Dia::Core::StringCRC(slot)))
            return StepResult::Rejected;
        mSaveRequested = true;
        return StepResult::Pending;
    }

    ++mSaveWaitUpdates;
    const bool completed = save->GetSaveCount() > mSaveCountBefore;
    if (!completed && mSaveWaitUpdates < kMaxSaveWaitUpdates)
        return StepResult::Pending;
    mSaveRequested = false;
    if (!completed)
        return StepResult::Timeout;
    return save->GetLastSaveResult().Ok() ? StepResult::Ok : StepResult::Failed;
}

//-----------------------------------------------------------------------------
// Load requests execute at the next SaveLoadModule update, success or not, so completion is "no longer
// pending"; success is then read from the last result and the completed-load counter.
F2TestRunnerModule::StepResult F2TestRunnerModule::StepLoad(const char* slot)
{
    constexpr uint32_t kMaxLoadWaitUpdates = 10;
    SaveLoadModule* save = mSaveLoad.Get();
    if (!save)
        return StepResult::Rejected;

    if (!mLoadRequested)
    {
        mLoadCountBefore = save->GetLoadCount();
        mLoadWaitUpdates = 0;
        if (!save->RequestLoad(Dia::Core::StringCRC(slot)))
            return StepResult::Rejected;
        mLoadRequested = true;
        return StepResult::Pending;
    }

    ++mLoadWaitUpdates;
    if (save->HasPendingLoad() && mLoadWaitUpdates < kMaxLoadWaitUpdates)
        return StepResult::Pending;
    mLoadRequested = false;
    if (save->HasPendingLoad())
        return StepResult::Timeout;
    const bool ok = save->GetLastLoadResult().Ok() && save->GetLoadCount() > mLoadCountBefore;
    return ok ? StepResult::Ok : StepResult::Failed;
}

//-----------------------------------------------------------------------------
// Phase 3: reference future. See RunRemainder; the values captured here are the reference the replay
// (phase 5) must reproduce. The streams are not rewound afterwards: the Reload phase restores them.
void F2TestRunnerModule::RunReferenceFuture()
{
    mPhaseDone = RunRemainder(RemainderMode::Reference);
}

//-----------------------------------------------------------------------------
// Phase 5: replay. Identical code path to phase 3 (RunRemainder), but the captured F1000' and 800
// draws are compared with the stored reference and resolve C3.
void F2TestRunnerModule::RunReplay()
{
    mPhaseDone = RunRemainder(RemainderMode::Replay);
}

//-----------------------------------------------------------------------------
// Scenario ticks [500, 1000) with the remainder script (no C4 assertions; C4 is phase 1). On the
// update after tick 999 capture F1000 FIRST, then draw the next 100 values from each stream
// (800 values, stream-major, kStreamNames order). Returns true once finished.
bool F2TestRunnerModule::RunRemainder(RemainderMode mode)
{
    if (mScenarioTick >= kF2ScenarioTicks)
    {
        bool ok = false;
        const F2Fingerprint fp = CaptureFingerprint(ok);
        std::array<int, kF2ReferenceDrawCount> draws = {};
        RandomStreamModule* random = mRandom.Get();
        if (random)
        {
            for (int s = 0; s < RandomStreamModule::kStreamCount; ++s)
            {
                Dia::Maths::RandomStream* stream = random->GetStreamByName(Dia::Core::StringCRC(RandomStreamModule::kStreamNames[s]));
                for (int n = 0; n < kF2ReferenceDrawsPerStream; ++n)
                    draws[s * kF2ReferenceDrawsPerStream + n] = stream ? stream->RandomInt() : 0;
            }
        }

        if (mode == RemainderMode::Reference)
        {
            mF1000 = fp;
            mHasF1000 = ok;
            mReferenceDraws = draws;
            mHasReferenceDraws = ok && random;
            return true;
        }

        if (mode == RemainderMode::Rerun)
        {
            mFRerun1000 = fp;
            FinishRerunComparison(ok && random != nullptr, draws);
            return true;
        }

        mFReplay1000 = fp;
        char summary[kSummaryLen];
        if (!ok || !random || !mHasF1000 || !mHasReferenceDraws)
        {
            CompleteCheck(F2CheckId::SaveLoadRng, false, "replay or reference data unavailable");
            return true;
        }
        const F2CompareResult state = F2Fingerprint::Compare(mF1000, fp);
        int matched = 0;
        int firstBad = -1;
        for (int i = 0; i < kF2ReferenceDrawCount; ++i)
        {
            if (draws[i] == mReferenceDraws[i])
                ++matched;
            else if (firstBad < 0)
                firstBad = i;
        }
        const bool pass = state.IsIdentical() && matched == kF2ReferenceDrawCount;
        if (pass)
        {
            std::snprintf(summary, sizeof(summary), "%d / %d draws match, state identical", matched, kF2ReferenceDrawCount);
        }
        else if (!state.IsIdentical())
        {
            std::snprintf(summary, sizeof(summary), "state differs: %s; %d / %d draws match",
                          state.Summary().c_str(), matched, kF2ReferenceDrawCount);
        }
        else
        {
            std::snprintf(summary, sizeof(summary), "%d / %d draws match, first mismatch stream %s draw %d",
                          matched, kF2ReferenceDrawCount, RandomStreamModule::kStreamNames[firstBad / kF2ReferenceDrawsPerStream],
                          firstBad % kF2ReferenceDrawsPerStream);
        }
        CompleteCheck(F2CheckId::SaveLoadRng, pass, summary);
        if (!pass)
            DIA_LOG_ERROR("CoW", "F2 C3 failed: %s", summary);
        return true;
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
    return false;
}

//-----------------------------------------------------------------------------
void F2TestRunnerModule::FinishC5a(bool passed, const char* detail)
{
    mHasC5a = true;
    mC5aPassed = passed;
    std::strncpy(mC5aSummary, detail, kSummaryLen - 1);
    mC5aSummary[kSummaryLen - 1] = '\0';
    const int i = static_cast<int>(F2CheckId::SaveLoadProof);
    if (passed)
    {
        // C5 stays Running until phase 7 resolves part b (the failed loads).
        mStatus[i] = F2CheckStatus::Running;
        std::snprintf(mLastResult[i], kSummaryLen, "(a) %s; (b) pending", mC5aSummary);
    }
    else
    {
        CompleteCheck(F2CheckId::SaveLoadProof, false, (std::string("reload: ") + mC5aSummary).c_str());
        DIA_LOG_ERROR("CoW", "F2 C5a failed: %s", mC5aSummary);
    }
    mPhaseDone = true;
}

//-----------------------------------------------------------------------------
// Phase 4: request `cow.load quick` through SaveLoadModule (the API the command uses), wait for the
// load to complete (frame boundary), then compare RESTORED FIELDS ONLY against F500 (C5a): streams and
// day counter restored, speed state at the paused defaults. C5 stays Running on success and goes
// straight to Fail otherwise.
void F2TestRunnerModule::RunReload()
{
    mPhaseDone = false;

    if (!mSaveLoad.Get() || !mHasF500 || !mSaveSucceeded)
    {
        FinishC5a(false, !mSaveLoad.Get() ? "SaveLoadModule unavailable" : "no valid quick save or F500");
        return;
    }

    const StepResult r = StepLoad("quick");
    if (r == StepResult::Pending)
        return;
    if (r == StepResult::Rejected)
    {
        FinishC5a(false, "quick load request rejected");
        return;
    }
    if (r != StepResult::Ok)
    {
        FinishC5a(false, "quick load did not complete successfully");
        return;
    }
    bool ok = false;
    mFReload = CaptureFingerprint(ok);
    if (!ok)
    {
        FinishC5a(false, "fingerprint unavailable after reload");
        return;
    }
    const F2CompareResult cmp = F2Fingerprint::CompareRestored(mF500, mFReload);
    if (cmp.IsIdentical())
        FinishC5a(true, "8 streams + day counter restored, speed paused");
    else
        FinishC5a(false, cmp.Summary().c_str());
}

//-----------------------------------------------------------------------------
void F2TestRunnerModule::FinishC2(bool passed, const char* summary)
{
    CompleteCheck(F2CheckId::Determinism, passed, summary);
    if (!passed)
        DIA_LOG_ERROR("CoW", "F2 C2 failed: %s", summary);
    mPhaseDone = true;
}

//-----------------------------------------------------------------------------
// Phase 6: reload the tick-0 `start` save (no in-process reset API exists), confirm the loaded state
// equals the tick-0 fingerprint on restored fields, then run the whole script again through the SAME
// code paths as phases 1 and 3 (RunSession / RunRemainder, in rerun mode). Resolves C2.
void F2TestRunnerModule::RunRerun()
{
    mPhaseDone = false;
    switch (mRerunStage)
    {
    case RerunStage::LoadStart:
    {
        if (!mSaveLoad.Get() || !mHasFStart || !mStartSaveOk)
        {
            FinishC2(false, "no valid start save or tick-0 fingerprint");
            return;
        }
        const StepResult r = StepLoad("start");
        if (r == StepResult::Pending)
            return;
        if (r != StepResult::Ok)
        {
            FinishC2(false, "start load did not complete successfully");
            return;
        }
        bool ok = false;
        const F2Fingerprint loaded = CaptureFingerprint(ok);
        if (!ok)
        {
            FinishC2(false, "fingerprint unavailable after start load");
            return;
        }
        const F2CompareResult cmp = F2Fingerprint::CompareRestored(mFStart, loaded);
        if (!cmp.IsIdentical())
        {
            FinishC2(false, ("start state: " + cmp.Summary()).c_str());
            return;
        }
        // Back at tick 0: reset the per-run bookkeeping the session code uses.
        mModel = F2ExpectedClock{};
        mScenarioTick = 0;
        mScriptIndex = 0;
        mTicksThisUpdate = 0;
        mC4Transitions = 0;
        mC4Matched = 0;
        mC4TickViolations = 0;
        mC4FirstFailure.clear();
        mCheckPending = false;
        mTickSub = Dia::MessageBus::BusSubscriptionHandle();
        mRerunStage = RerunStage::Session;
        return;
    }
    case RerunStage::Session:
        RunSession();   // on reaching tick 500 it captures F500'' and switches to the Remainder stage
        return;
    case RerunStage::Remainder:
        mPhaseDone = RunRemainder(RemainderMode::Rerun);
        return;
    }
}

//-----------------------------------------------------------------------------
// C2: F500'' == F500 and F1000'' == F1000 (full compare, captured at the same points as the originals)
// and the 800 draws match the reference. Divergences = differing fields + mismatched draws.
void F2TestRunnerModule::FinishRerunComparison(bool haveLive, const std::array<int, kF2ReferenceDrawCount>& draws)
{
    if (!haveLive || !mHasF500 || !mHasF1000 || !mHasReferenceDraws)
    {
        FinishC2(false, "rerun or reference data unavailable");
        return;
    }
    const F2CompareResult d500 = F2Fingerprint::Compare(mF500, mFRerun500);
    const F2CompareResult d1000 = F2Fingerprint::Compare(mF1000, mFRerun1000);
    int badDraws = 0;
    int firstBad = -1;
    for (int i = 0; i < kF2ReferenceDrawCount; ++i)
    {
        if (draws[i] != mReferenceDraws[i])
        {
            ++badDraws;
            if (firstBad < 0)
                firstBad = i;
        }
    }
    const size_t divergences = d500.differingFields.size() + d1000.differingFields.size() + static_cast<size_t>(badDraws);
    char summary[kSummaryLen];
    if (divergences == 0)
    {
        std::snprintf(summary, sizeof(summary), "0 divergences over %u ticks", kF2ScenarioTicks);
        FinishC2(true, summary);
        return;
    }
    std::string first;
    if (!d500.differingFields.empty())
        first = "F500 " + d500.differingFields.front();
    else if (!d1000.differingFields.empty())
        first = "F1000 " + d1000.differingFields.front();
    else
        first = std::string("stream ") + RandomStreamModule::kStreamNames[firstBad / kF2ReferenceDrawsPerStream]
              + " draw " + std::to_string(firstBad % kF2ReferenceDrawsPerStream);
    std::snprintf(summary, sizeof(summary), "%u divergences, first: %s", static_cast<unsigned>(divergences), first.c_str());
    FinishC2(false, summary);
}

//-----------------------------------------------------------------------------
namespace {
std::string ReadFileBytes(const char* path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}
}

//-----------------------------------------------------------------------------
// Resolves C5 from part a (already stored) and part b (this phase).
void F2TestRunnerModule::FinishC5b(bool passed, const char* detail)
{
    mHasC5b = true;
    mC5bPassed = passed;
    std::strncpy(mC5bSummary, detail, kSummaryLen - 1);
    mC5bSummary[kSummaryLen - 1] = '\0';
    if (!passed)
        DIA_LOG_ERROR("CoW", "F2 C5b failed: %s", mC5bSummary);

    const int i = static_cast<int>(F2CheckId::SaveLoadProof);
    if (mHasC5a && mC5aPassed)
    {
        char summary[kSummaryLen];
        if (passed)
            std::snprintf(summary, sizeof(summary), "(a) restored fields match; (b) failed loads leave state untouched");
        else
            std::snprintf(summary, sizeof(summary), "(a) restored fields match; (b) %s", mC5bSummary);
        CompleteCheck(F2CheckId::SaveLoadProof, passed, summary);
    }
    else if (mStatus[i] == F2CheckStatus::Fail)
    {
        // Already failed in phase 4: keep that result first and append part b, without a second event.
        char summary[kSummaryLen];
        std::snprintf(summary, sizeof(summary), "%s; (b) %s", mLastResult[i], passed ? "failed loads leave state untouched" : mC5bSummary);
        std::strncpy(mLastResult[i], summary, kSummaryLen - 1);
        mLastResult[i][kSummaryLen - 1] = '\0';
    }
    else
    {
        CompleteCheck(F2CheckId::SaveLoadProof, false, "reload (a) did not run");
    }
    mPhaseDone = true;
}

//-----------------------------------------------------------------------------
// Phase 7: loads from a missing slot and from a corrupt slot must each fail (SaveLoadModule
// pre-validates before touching anything) and leave the live state and the `quick` file untouched.
// The clock is paused first (the remainder script ends running) so the live fingerprint is stable.
// Only pre-validation failures are covered; a participant failing mid-load is unsupported by E1.
void F2TestRunnerModule::RunFailedLoad()
{
    constexpr uint32_t kQuiesceUpdates = kF2SettleTicks + 2;
    constexpr const char* kMissingSlot = "missing_slot";
    constexpr const char* kCorruptSlot = "corrupt";
    mPhaseDone = false;

    SaveLoadModule* save = mSaveLoad.Get();
    if (!save || !mSaveSucceeded)
    {
        FinishC5b(false, !save ? "SaveLoadModule unavailable" : "no quick save to protect");
        return;
    }
    Dia::SaveGame::SlotManager slots;
    slots.Init(save->GetConfig());
    char quickPath[256];
    slots.BuildPath(Dia::Core::StringCRC("quick"), quickPath, sizeof(quickPath));

    switch (mFailStage)
    {
    case FailStage::Quiesce:
        if (mFailQuiesceUpdates == 0)
        {
            F2ScenarioAction pause;
            pause.kind = F2ActionKind::PlayerPause;
            IssueAction(pause);
        }
        if (++mFailQuiesceUpdates >= kQuiesceUpdates)
            mFailStage = FailStage::Begin;
        return;

    case FailStage::Begin:
    {
        bool ok = false;
        mFLiveBefore = CaptureFingerprint(ok);
        mQuickBytesBefore = ReadFileBytes(quickPath);
        if (!ok || mQuickBytesBefore.empty())
        {
            FinishC5b(false, "no readable quick file or live fingerprint");
            return;
        }
        char corruptPath[256];
        slots.BuildPath(Dia::Core::StringCRC(kCorruptSlot), corruptPath, sizeof(corruptPath));
        {
            std::ofstream out(corruptPath, std::ios::binary | std::ios::trunc);
            out << "F2 corrupt slot {{ not a save file";
            if (!out.good())
            {
                FinishC5b(false, "could not write the corrupt slot file");
                return;
            }
        }
        mMissingLoadFailed = false;
        mFailStage = FailStage::MissingLoad;
        return;
    }

    case FailStage::MissingLoad:
    {
        const StepResult r = StepLoad(kMissingSlot);
        if (r == StepResult::Pending)
            return;
        mMissingLoadFailed = (r == StepResult::Failed);
        mFailStage = FailStage::CorruptLoad;
        return;
    }

    case FailStage::CorruptLoad:
    {
        const StepResult r = StepLoad(kCorruptSlot);
        if (r == StepResult::Pending)
            return;
        const bool corruptLoadFailed = (r == StepResult::Failed);
        slots.DeleteSlot(Dia::Core::StringCRC(kCorruptSlot));

        bool ok = false;
        const F2Fingerprint after = CaptureFingerprint(ok);
        const F2CompareResult live = F2Fingerprint::Compare(mFLiveBefore, after);
        const bool quickUnchanged = (ReadFileBytes(quickPath) == mQuickBytesBefore);

        char detail[kSummaryLen];
        bool pass = false;
        if (!mMissingLoadFailed)
            std::snprintf(detail, sizeof(detail), "missing slot load did not report failure");
        else if (!corruptLoadFailed)
            std::snprintf(detail, sizeof(detail), "corrupt slot load did not report failure");
        else if (!ok || !live.IsIdentical())
            std::snprintf(detail, sizeof(detail), "failed load changed live state: %s", ok ? live.Summary().c_str() : "unavailable");
        else if (!quickUnchanged)
            std::snprintf(detail, sizeof(detail), "failed load changed the quick save file");
        else
        {
            pass = true;
            std::snprintf(detail, sizeof(detail), "failed loads leave state untouched");
        }
        FinishC5b(pass, detail);
        return;
    }
    }
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
