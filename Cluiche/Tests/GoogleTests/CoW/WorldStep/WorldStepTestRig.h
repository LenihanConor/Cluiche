#pragma once
// Shared rig for the World Step module tests (Feature 2). Test-only, not shipped.
//
// A real DeferredStepScheduler over DiaDeferredStep's ManualStepLauncher (thread-free: a step runs only when the test
// completes it or the scheduler stalls on it), a standalone SimQueryHostModule that swallows its announcements, and the
// real WorldStepModule driven without a ProcessingUnit through its test seam. One Rig::Step(day) replays what one sim
// step does in production, in production order:
//   1. the bus handler delivers the SimulationTick       (WorldStepModule::OnSimulationTick, records the key)
//   2. DeferredStepModule::DoUpdate                      (scheduler.Tick(GetCurrentKey()), applies due steps)
//   3. WorldStepModule::DoUpdate                         (the cadence: resync, readiness, retry, kick)
#include <gtest/gtest.h>

#include <Modules/WorldStep/WorldStepModule.h>
#include <Modules/SimQueryHostModule.h>
#include <DiaDeferredStep/Testing/FakeStepStage.h>

#include <DiaCore/Core/Assert.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaDeferredStep/DeferredStepScheduler.h>
#include <DiaDeferredStep/SerialExecutor.h>
#include <DiaDeferredStep/Testing/LogCapture.h>
#include <DiaDeferredStep/Testing/ManualStepLauncher.h>
#include <DiaObservation/Metric/MetricRegistry.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace WsTest
{
    using Dia::Core::StringCRC;
    using Dia::DeferredStep::Testing::CallLog;
    using Dia::DeferredStep::Testing::CallPhase;
    using Dia::DeferredStep::Testing::CallRecord;
    using Dia::DeferredStep::Testing::FakeStepStage;
    using Dia::DeferredStep::StepKey;

    constexpr const char* kDefaultConfig = "{\"order\":[\"weather\",\"ecology\"],\"apply_after_steps\":7,\"deferred\":true}";

    inline std::string Config(const char* orderCsv, unsigned k, bool deferred)
    {
        std::string order;
        std::string id;
        for (const char* p = orderCsv;; ++p)
        {
            if (*p == ',' || *p == '\0')
            {
                if (!id.empty())
                {
                    if (!order.empty()) order += ",";
                    order += "\"" + id + "\"";
                }
                id.clear();
                if (*p == '\0') break;
            }
            else
            {
                id += *p;
            }
        }
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%u", k);
        return "{\"order\":[" + order + "],\"apply_after_steps\":" + buf + ",\"deferred\":" + (deferred ? "true" : "false") + "}";
    }

    // Counts asserts instead of breaking into the debugger (Debug only), so paths that assert on purpose can be tested.
#ifdef DEBUG
    inline int gAssertCount = 0;
    inline void CountingAssert(const char*, const char*, int, const char*, ...) { ++gAssertCount; }
    struct AssertCapture
    {
        AssertCapture() { mPrev = Dia::Core::g_pAssertFunc; Dia::Core::g_pAssertFunc = CountingAssert; gAssertCount = 0; }
        ~AssertCapture() { Dia::Core::g_pAssertFunc = mPrev; }
        int Count() const { return gAssertCount; }
    private:
        Dia::Core::DIA_ASSERT_FUNC mPrev;
    };
#else
    struct AssertCapture { int Count() const { return 0; } };
#endif

    inline CoW::SimulationTick MakeTick(uint64_t day)
    {
        CoW::SimulationTick t;
        t.day = day;
        t.band = CoW::ResolveBand(day);
        return t;
    }

    // SimQueryHostModule without streams: announcements are accepted and dropped. Registration and Has() still work.
    class TestQueryHost : public Cluiche::AppFlow::SimQueryHostModule
    {
    public:
        TestQueryHost() : SimQueryHostModule(StringCRC("WsTestQueryHost")) {}
    protected:
        Dia::ApplicationFlow::SendResult SendReply(const Dia::DebugServer::SimReplyEvent&) override
        {
            return Dia::ApplicationFlow::SendResult::kDelivered;
        }
    };

    // Exposes the protected lifecycle so the module runs without a ProcessingUnit.
    class Probe : public CoW::WorldStepModule
    {
    public:
        explicit Probe(const StringCRC& id) : CoW::WorldStepModule(id) {}
        void CallConfigure(const char* json) { OnConfigure(json); }
        Dia::ApplicationFlow::StartResult CallStart() { return DoStart(); }
        Dia::ApplicationFlow::StopResult CallStop() { return DoStop(); }
        void CallUpdate()
        {
            Dia::SimTime::SimTimeContext ctx{ Dia::Core::TimeAbsolute::Zero(), Dia::Core::TimeRelative::Zero(), 0, 1.0f, false };
            DoUpdate(ctx);
        }
    };

    // Module + scheduler + launcher + query host, no stages.
    struct Harness
    {
        Dia::DeferredStep::Testing::ManualStepLauncher launcher;
        Dia::DeferredStep::DeferredStepScheduler scheduler{ launcher };
        TestQueryHost host;
        Probe module{ StringCRC("WorldStepModule") };
        bool autoComplete = true;   // complete a launched step right after the kick (an early finish); false = it runs only on a stall
        bool started = false;

        explicit Harness(const std::string& config = kDefaultConfig, bool start = true)
        {
            module.CallConfigure(config.c_str());
            CoW::WorldStepModule::TestHosts hosts;
            hosts.scheduler = &scheduler;
            hosts.queryHost = &host;
            module.UseTestHosts(hosts);
            if (start)
                Start();
        }

        bool Start()
        {
            started = (module.CallStart() == Dia::ApplicationFlow::StartResult::kReady);
            return started;
        }

        void Stop()
        {
            if (started)
                module.CallStop();
            started = false;
        }

        // One sim step in production order.
        void StepTick(const CoW::SimulationTick& tick)
        {
            module.OnSimulationTick(tick);
            scheduler.Tick(module.GetCurrentKey());
            module.CallUpdate();
            if (autoComplete)
                launcher.CompleteAll();
        }
        void StepDay(uint64_t day) { StepTick(MakeTick(day)); }
        void StepDays(uint64_t first, uint64_t last) { for (uint64_t d = first; d <= last; ++d) StepDay(d); }

        ~Harness() { Stop(); }
    };

    // Harness + the two Feature 1 fake stages, with a harvested call history (the fake's own log holds only 128 records).
    struct Rig : Harness
    {
        CallLog log;
        FakeStepStage weather{ StringCRC("weather"), log };
        FakeStepStage ecology{ StringCRC("ecology"), log };
        std::vector<CallRecord> history;
        std::vector<uint64_t> kickDays;     // days on which the module launched a step
        std::vector<uint64_t> commitDays;   // days on which the weather stage committed

        explicit Rig(const std::string& config = kDefaultConfig, bool registerStages = true, bool start = true)
            : Harness(config, start)
        {
            if (started && registerStages)
                RegisterBoth();
        }

        void RegisterBoth()
        {
            EXPECT_TRUE(module.RegisterStage(StringCRC("weather"), weather));
            EXPECT_TRUE(module.RegisterStage(StringCRC("ecology"), ecology));
        }

        void UnregisterBoth()
        {
            module.UnregisterStage(StringCRC("weather"));
            module.UnregisterStage(StringCRC("ecology"));
        }

        void Harvest()
        {
            for (unsigned i = 0; i < log.Size(); ++i)
                history.push_back(log[i]);
            log.RemoveAll();
        }

        void Step(uint64_t day)
        {
            const uint64_t kicksBefore = module.KickCount() + module.SyncStepCount();
            const size_t commitsBefore = Count(CallPhase::Commit, "weather");
            StepDay(day);
            Harvest();
            if (module.KickCount() + module.SyncStepCount() > kicksBefore) kickDays.push_back(day);
            if (Count(CallPhase::Commit, "weather") > commitsBefore) commitDays.push_back(day);
        }
        void Steps(uint64_t first, uint64_t last) { for (uint64_t d = first; d <= last; ++d) Step(d); }

        size_t Count(CallPhase phase, const char* stage = nullptr) const
        {
            size_t n = 0;
            for (const CallRecord& r : history)
            {
                if (r.phase == phase && (stage == nullptr || r.stageId == StringCRC(stage)))
                    ++n;
            }
            return n;
        }

        std::vector<StepKey> CaptureKeys(const char* stage) const
        {
            std::vector<StepKey> keys;
            for (const CallRecord& r : history)
            {
                if (r.phase == CallPhase::Capture && r.stageId == StringCRC(stage))
                    keys.push_back(r.key);
            }
            return keys;
        }

        ~Rig()
        {
            if (started)
                UnregisterBoth();
        }
    };

    inline double GaugeValue(const char* name)
    {
        auto* g = Dia::Observation::Metric::MetricRegistry::Instance().FindGauge(StringCRC(name));
        return g != nullptr ? g->Value() : -1.0;
    }
    inline int64_t CounterValue(const char* name)
    {
        auto* c = Dia::Observation::Metric::MetricRegistry::Instance().FindCounter(StringCRC(name));
        return c != nullptr ? static_cast<int64_t>(c->Value()) : -1;
    }

    // A real-shaped stage: Capture copies the committed state (plus a sum of an upstream stage's committed state),
    // Run calls ParallelFor several times over independent cells, Commit publishes. No hidden inputs, so a deferred
    // and a synchronous run of the same stages must be bit-identical after the same number of applied steps.
    class OracleStage final : public Dia::DeferredStep::IStepStage
    {
    public:
        OracleStage(uint32_t cells, uint32_t passes, uint32_t salt, const OracleStage* upstream = nullptr)
            : mCommitted(cells, 1u), mWorking(cells, 0u), mPasses(passes), mSalt(salt), mUpstream(upstream) {}

        bool IsReady() const override { return mReady; }
        void SetReady(bool ready) { mReady = ready; }

        void Capture(StepKey key) override
        {
            mWorking = mCommitted;
            mKey = static_cast<uint32_t>(key);
            mUpstreamSum = 0;
            if (mUpstream != nullptr)
            {
                for (uint32_t v : mUpstream->mCommitted)
                    mUpstreamSum += v;
            }
        }

        void Run(Dia::DeferredStep::IParallelExecutor& executor) override
        {
            const uint32_t count = static_cast<uint32_t>(mWorking.size());
            for (uint32_t pass = 0; pass < mPasses; ++pass)
            {
                executor.ParallelFor(count, 8, [this, pass](uint32_t begin, uint32_t end)
                {
                    for (uint32_t i = begin; i < end; ++i)
                        mWorking[i] = Mix(mWorking[i], i, pass);
                });
            }
        }

        void Commit() override { mCommitted = mWorking; ++mCommits; }

        void SerializePending(Dia::SaveGame::SaveContext&) const override {}
        void DeserializePending(Dia::SaveGame::LoadContext&) override {}
        bool ValidatePending(Dia::SaveGame::LoadContext&) const override { return true; }

        const std::vector<uint32_t>& Committed() const { return mCommitted; }
        uint32_t Commits() const { return mCommits; }

    private:
        uint32_t Mix(uint32_t x, uint32_t i, uint32_t pass) const
        {
            x = x * 2654435761u ^ ((i + 1u) * 40503u) ^ (mKey * 97u) ^ (pass * 131u) ^ mSalt ^ mUpstreamSum;
            return x ^ (x >> 13);
        }

        std::vector<uint32_t> mCommitted;
        std::vector<uint32_t> mWorking;
        uint32_t mPasses;
        uint32_t mSalt;
        const OracleStage* mUpstream;
        bool mReady = true;
        uint32_t mKey = 0;
        uint32_t mUpstreamSum = 0;
        uint32_t mCommits = 0;
    };
}
