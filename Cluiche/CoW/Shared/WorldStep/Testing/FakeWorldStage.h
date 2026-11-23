#pragma once

#include <stdint.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

#include <DiaDeferredStep/IStepStage.h>
#include <Shared/World/WorldSimNames.h>
#include <Shared/WorldStep/WorldStepStageBase.h>

namespace CoW {
namespace WorldStep {
namespace Testing {

// Fixed-capacity log of call names (string literals; no STL).
struct CallLog
{
    static const uint32_t kCapacity = 64;

    void Add(const char* name)
    {
        if (mCount < kCapacity) { mNames[mCount++] = name; }
    }
    uint32_t Count() const { return mCount; }
    const char* At(uint32_t i) const { return i < mCount ? mNames[i] : ""; }
    void Clear() { mCount = 0; }

private:
    const char* mNames[kCapacity] = {};
    uint32_t mCount = 0;
};

// Plain structs for the later WorldStepStageBase fixture.
struct FakeSim
{
    FakeSim() { ++sLive; }
    ~FakeSim() { --sLive; if (sDestroyLog != nullptr) { sDestroyLog->Add("~FakeSim"); } }
    FakeSim(const FakeSim&) = delete;
    FakeSim& operator=(const FakeSim&) = delete;
    int value = 0;
    static std::atomic<int> sLive;      // live instances, for leak and destruction checks
    static CallLog* sDestroyLog;        // optional: records destruction order (test double only)
};
inline std::atomic<int> FakeSim::sLive{0};
inline CallLog* FakeSim::sDestroyLog = nullptr;
struct FakeEntry { int value = 0; };
struct FakeLatch { int value = 0; };

// Records every IStepStage call by name. Seed for the WorldStepStageBase test fixture.
class FakeWorldStage : public Dia::DeferredStep::IStepStage
{
public:
    bool IsReady() const override
    {
        log.Add("IsReady");
        return readyValue && extraReady && !initFails;
    }
    void Capture(Dia::DeferredStep::StepKey) override { log.Add("Capture"); }
    void Run(Dia::DeferredStep::IParallelExecutor&) override { log.Add("Run"); }
    void Commit() override { log.Add("Commit"); }
    void SerializePending(Dia::SaveGame::SaveContext&) const override { log.Add("SerializePending"); }
    void DeserializePending(Dia::SaveGame::LoadContext&) override { log.Add("DeserializePending"); }
    bool ValidatePending(Dia::SaveGame::LoadContext&) const override
    {
        log.Add("ValidatePending");
        return latchOk;
    }

    mutable CallLog log;
    bool extraReady = true;
    bool latchOk = true;
    bool initFails = false;
    bool readyValue = true;
};

typedef WorldStepStageBase<FakeSim, FakeEntry, FakeLatch> FakeBase;

inline const char* FakeCauseText(FakeBase::CauseCode code)
{
    return code == 1 ? "fake_init_failed" : (code == 2 ? "fake_other" : "fake_none");
}

// Hook set for the fake system. Records every hook call by name.
struct FakeHooks : public FakeBase::Hooks
{
    bool InitialiseSim(FakeSim& sim, const WorldView&, FakeBase::CauseCode& cause) override
    {
        log.Add("InitialiseSim");
        initEntered = true;
        for (int i = 0; blockInit && !releaseInit && i < 2000; ++i)   // bounded: 2 s
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        initExited = true;
        if (initFails) { cause = 1; return false; }
        sim.value = 0;
        return true;
    }
    bool ExtraReady() const override { return extraReady; }
    bool LatchInputs(FakeLatch& latch, Dia::DeferredStep::StepKey key) override
    {
        log.Add("LatchInputs");
        latch.value = static_cast<int>(key);
        return latchOk;
    }
    void ApplyEntry(FakeSim& working, FakeLatch&, const FakeEntry& e) override
    {
        log.Add("ApplyEntry");
        if (appliedCount < kMaxApplied) { applied[appliedCount++] = e.value; }
        working.value += e.value;
    }
    void RunStep(FakeSim& working, const FakeLatch& latch, Dia::DeferredStep::IParallelExecutor&) override
    {
        log.Add("RunStep");
        if (runSleepMs > 0) { std::this_thread::sleep_for(std::chrono::milliseconds(runSleepMs)); }
        if (onRun) { onRun(); }
        working.value = working.value * 2 + latch.value;
    }
    void PublishCommit(const FakeSim& committed, uint32_t stepDay) override
    {
        log.Add("PublishCommit");
        publishedValue = committed.value;
        publishedDay = stepDay;
        ++publishCount;
    }
    void OnStartExtra() override { log.Add("OnStartExtra"); }
    void OnStopExtra() override { log.Add("OnStopExtra"); }
    bool ExtraCountMatches(const Json::Value&, const WorldView&) const override { return extraCountOk; }
    bool RestoreCommitted(FakeSim& sim, const Json::Value& record) override
    {
        log.Add("RestoreCommitted");
        if (restoreFails) { return false; }
        sim.value = record.get("sim_value", 0).asInt();
        return true;
    }

    static const uint32_t kMaxApplied = 256;
    CallLog log;
    bool extraReady = true;
    bool latchOk = true;
    bool initFails = false;
    bool blockInit = false;
    bool extraCountOk = true;
    bool restoreFails = false;
    int runSleepMs = 0;
    std::function<void()> onRun;
    std::atomic<bool> releaseInit{false};
    std::atomic<bool> initEntered{false};
    std::atomic<bool> initExited{false};
    int applied[kMaxApplied] = {};
    uint32_t appliedCount = 0;
    int publishedValue = -1;
    uint32_t publishedDay = 0;
    uint32_t publishCount = 0;
};

// Fake system stage. Spin-up jobs are driven by hand (RunJobNow) unless manualJobs is false.
class FakeStage : public FakeBase
{
public:
    FakeStage(FakeHooks& hooks, CallLog* log)
        : FakeBase(hooks, World::kSuccession, "FakeWorld", &FakeCauseText)
        , mLog(log)
    {
    }

    // Pretend the worker finished the job for (rev, gen).
    void RunJobNow(uint32_t rev, uint32_t gen, const WorldView& world) { RunJob(rev, gen, world); }

    // Announce a world and complete its spin-up by hand, then poll.
    void SpinUpNow(uint32_t rev)
    {
        WorldView w;
        w.revision = rev;
        w.present = true;
        OnWorldGenerationComplete(w);
        RunJobNow(rev, VisitGeneration(), w);
        Update();
    }

    bool manualJobs = true;

protected:
    void LaunchJob(uint32_t rev, uint32_t gen, const WorldView& world) override
    {
        if (!manualJobs) { FakeBase::LaunchJob(rev, gen, world); }
    }
    void UnregisterStage() override { if (mLog != nullptr) { mLog->Add("UnregisterStage"); } }

private:
    CallLog* mLog;
};

}  // namespace Testing
}  // namespace WorldStep
}  // namespace CoW
