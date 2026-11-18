// DeferredStepModule metrics (AC 19-22) and health reporter (AC 23-25).
#include "DeferredStepModuleHarness.h"
#include <DiaObservation/Health/HealthRegistry.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <chrono>
#include <thread>

namespace
{
	using Dia::DeferredStep::StepKey;
	using Dia::Observation::Health::HealthRegistry;
	using Dia::Observation::Health::HealthStatus;
	using Dia::Observation::Metric::MetricEntry;
	using Dia::Observation::Metric::MetricRegistry;
	using Dia::Observation::Metric::MetricSnapshot;

	struct MH_KeySource : Dia::DeferredStep::IStepKeySource
	{
		StepKey mKey = 0;
		StepKey GetCurrentKey() const override { return mKey; }
	};

	const StringCRC kMhChannel("MH_Channel");

	const char* const kAllMetricNames[] =
	{
		"dia.deferredstep.in_flight", "dia.deferredstep.kicks", "dia.deferredstep.kicks_rejected",
		"dia.deferredstep.applies", "dia.deferredstep.stalls", "dia.deferredstep.stall_ms",
		"dia.deferredstep.slack_keys", "dia.deferredstep.cap_utilisation", "dia.deferredstep.inline_fallbacks",
	};

	const MetricEntry* FindEntry(const MetricSnapshot& snap, const char* name)
	{
		for (unsigned i = 0; i < snap.entryCount; ++i)
		{
			if (snap.entries[i].name == StringCRC(name)) { return &snap.entries[i]; }
		}
		return nullptr;
	}

	uint64_t CounterValue(const char* name)
	{
		auto* c = MetricRegistry::Instance().FindCounter(StringCRC(name));
		return c ? c->Value() : 0;
	}
	double GaugeValue(const char* name)
	{
		auto* g = MetricRegistry::Instance().FindGauge(StringCRC(name));
		return g ? g->Value() : -1.0;
	}

	// Registers a slow step on the channel, kicks it with applyKey == key, then updates so Tick must wait.
	void ForceStall(DSM_Probe& probe, MH_KeySource& source, Dia::DeferredStep::Testing::RecordingStep& step, StepKey key)
	{
		step.SetOnRun([]() { std::this_thread::sleep_for(std::chrono::milliseconds(40)); });
		ASSERT_EQ(probe.GetScheduler().Kick(kMhChannel, key, key), Dia::DeferredStep::KickResult::kLaunched);
		source.mKey = key;
		probe.CallUpdate();
	}

	bool SnapshotHas(const char* name)
	{
		MetricSnapshot snap;
		MetricRegistry::Instance().Snapshot(snap);
		return FindEntry(snap, name) != nullptr;
	}

	// First snapshot entry named DeferredStep. Live reporters are listed before retired (frozen) ones, so a live
	// registration is distinguishable from a retired copy by whether it follows a clock change.
	bool FindHealth(HealthStatus& status)
	{
		HealthRegistry::ReporterSnapshot out[64];
		unsigned count = 0;
		HealthRegistry::Instance().Snapshot(out, 64, count);
		for (unsigned i = 0; i < count; ++i)
		{
			if (out[i].name == StringCRC("DeferredStep")) { status = out[i].health.status; return true; }
		}
		return false;
	}
}

TEST(DeferredStepModuleMetrics, AllNineRegisteredWithInfoAfterStart)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	// Lookup by name, not via MetricSnapshot: the snapshot is capped at kMaxMetricEntries and other suites in the
	// same process may have filled the global registry.
	auto& reg = MetricRegistry::Instance();
	for (const char* name : kAllMetricNames)
	{
		SCOPED_TRACE(name);
		EXPECT_TRUE(reg.FindCounter(StringCRC(name)) != nullptr || reg.FindGauge(StringCRC(name)) != nullptr);
		const auto* info = reg.GetInfo(StringCRC(name));
		ASSERT_NE(info, nullptr);
		EXPECT_NE(info->description, nullptr);
		EXPECT_NE(info->unit, nullptr);
	}
	for (const char* name : { "dia.deferredstep.in_flight", "dia.deferredstep.slack_keys", "dia.deferredstep.cap_utilisation" })
	{
		EXPECT_NE(reg.FindGauge(StringCRC(name)), nullptr) << name;
	}
	for (const char* name : { "dia.deferredstep.kicks", "dia.deferredstep.kicks_rejected", "dia.deferredstep.applies",
		"dia.deferredstep.stalls", "dia.deferredstep.stall_ms", "dia.deferredstep.inline_fallbacks" })
	{
		EXPECT_NE(reg.FindCounter(StringCRC(name)), nullptr) << name;
	}
}

TEST(DeferredStepModuleMetrics, RestartCyclesReRegisterWithoutDuplicateWarnings)
{
	Dia::DeferredStep::Testing::LogCapture vscLogs("VSC");
	Harness h;
	ASSERT_TRUE(h.Start());
	for (int cycle = 0; cycle < 3; ++cycle)
	{
		EXPECT_EQ(g_probe->CallStop(), StopResult::kDone);
		auto& reg = MetricRegistry::Instance();
		for (const char* name : kAllMetricNames)
		{
			EXPECT_TRUE(reg.FindCounter(StringCRC(name)) == nullptr && reg.FindGauge(StringCRC(name)) == nullptr) << name;
			EXPECT_FALSE(SnapshotHas(name)) << name;
		}
		EXPECT_EQ(g_probe->CallStart(), StartResult::kReady);
		for (const char* name : kAllMetricNames)
		{
			EXPECT_TRUE(reg.FindCounter(StringCRC(name)) != nullptr || reg.FindGauge(StringCRC(name)) != nullptr) << name;
			EXPECT_TRUE(SnapshotHas(name)) << name;
			EXPECT_EQ(CounterValue(name), 0u) << name;
			EXPECT_GE(GaugeValue(name), -1.0) << name;
		}
	}
	EXPECT_EQ(vscLogs.Warnings(), 0u);
}

TEST(DeferredStepModuleMetrics, AbsentAfterStopAndResumeFromFreshStatsOnRestart)
{
	MH_KeySource source;
	Harness h;
	ASSERT_TRUE(h.Start());
	g_probe->SetKeySource(source);
	Dia::DeferredStep::Testing::RecordingStep step;
	ASSERT_TRUE(g_probe->GetScheduler().Register(kMhChannel, step));
	const uint64_t kicks0 = CounterValue("dia.deferredstep.kicks");
	ASSERT_EQ(g_probe->GetScheduler().Kick(kMhChannel, 1, 2), Dia::DeferredStep::KickResult::kLaunched);
	source.mKey = 2;
	g_probe->CallUpdate();
	EXPECT_EQ(CounterValue("dia.deferredstep.kicks"), kicks0 + 1);
	g_probe->GetScheduler().Unregister(kMhChannel);

	g_probe->CallStop();
	g_probe->CallUpdate();   // no scheduler: no-op
	EXPECT_EQ(MetricRegistry::Instance().FindCounter(StringCRC("dia.deferredstep.kicks")), nullptr);
	EXPECT_FALSE(SnapshotHas("dia.deferredstep.kicks"));

	g_probe->CallStart();    // revived zeroed; fresh scheduler stats, baselines reset, so no negative delta / double count
	g_probe->CallUpdate();
	ASSERT_NE(MetricRegistry::Instance().FindCounter(StringCRC("dia.deferredstep.kicks")), nullptr);
	EXPECT_EQ(CounterValue("dia.deferredstep.kicks"), 0u);
}

TEST(DeferredStepModuleMetrics, ValuesTrackForcedStallWithoutDoubleCounting)
{
	MH_KeySource source;
	Harness h;
	ASSERT_TRUE(h.Start());
	g_probe->SetKeySource(source);
	Dia::DeferredStep::Testing::RecordingStep step;
	ASSERT_TRUE(g_probe->GetScheduler().Register(kMhChannel, step));

	const uint64_t kicks0 = CounterValue("dia.deferredstep.kicks");
	const uint64_t applies0 = CounterValue("dia.deferredstep.applies");
	const uint64_t stalls0 = CounterValue("dia.deferredstep.stalls");
	const uint64_t stallMs0 = CounterValue("dia.deferredstep.stall_ms");

	ForceStall(*g_probe, source, step, 1);
	const auto& stats = g_probe->GetScheduler().GetStats();
	EXPECT_GE(stats.stalls, 1u);
	EXPECT_EQ(CounterValue("dia.deferredstep.kicks") - kicks0, stats.kicks);
	EXPECT_EQ(CounterValue("dia.deferredstep.applies") - applies0, stats.applies);
	EXPECT_EQ(CounterValue("dia.deferredstep.stalls") - stalls0, stats.stalls);
	EXPECT_EQ(CounterValue("dia.deferredstep.stall_ms") - stallMs0, stats.totalStallMicros / 1000u);
	EXPECT_GE(stats.totalStallMicros, 1000u);
	EXPECT_EQ(GaugeValue("dia.deferredstep.slack_keys"), 0.0);   // stalled step: slack 0
	EXPECT_EQ(GaugeValue("dia.deferredstep.in_flight"), 0.0);

	for (int i = 0; i < 5; ++i) { g_probe->CallUpdate(); }       // idle ticks add nothing
	EXPECT_EQ(CounterValue("dia.deferredstep.stalls") - stalls0, stats.stalls);
	EXPECT_EQ(CounterValue("dia.deferredstep.stall_ms") - stallMs0, stats.totalStallMicros / 1000u);
	EXPECT_EQ(CounterValue("dia.deferredstep.applies") - applies0, stats.applies);

	g_probe->GetScheduler().Unregister(kMhChannel);
}

TEST(DeferredStepModuleMetrics, SlackGaugeReportsRemainingKeysOfEarlyCompletion)
{
	MH_KeySource source;
	Harness h;
	ASSERT_TRUE(h.Start());
	g_probe->SetKeySource(source);
	Dia::DeferredStep::Testing::RecordingStep step;
	ASSERT_TRUE(g_probe->GetScheduler().Register(kMhChannel, step));
	ASSERT_EQ(g_probe->GetScheduler().Kick(kMhChannel, 1, 10), Dia::DeferredStep::KickResult::kLaunched);

	// Wait (bounded) for the instant step to complete, then observe it at key 4 (slack 6), apply at key 10.
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	source.mKey = 4;
	g_probe->CallUpdate();
	EXPECT_EQ(GaugeValue("dia.deferredstep.in_flight"), 1.0);
	source.mKey = 10;
	g_probe->CallUpdate();
	EXPECT_EQ(GaugeValue("dia.deferredstep.slack_keys"), 6.0);
	EXPECT_EQ(GaugeValue("dia.deferredstep.in_flight"), 0.0);
	g_probe->GetScheduler().Unregister(kMhChannel);
}

TEST(DeferredStepModuleMetrics, InFlightAndCapUtilisationRefreshEveryTick)
{
	MH_KeySource source;
	Harness h;
	ASSERT_TRUE(h.Start());
	g_probe->SetKeySource(source);
	Dia::DeferredStep::Testing::RecordingStep step;
	std::atomic<bool> release{ false };
	step.SetOnRun([&release]() { while (!release.load()) { std::this_thread::yield(); } });
	ASSERT_TRUE(g_probe->GetScheduler().Register(kMhChannel, step));
	ASSERT_EQ(g_probe->GetScheduler().Kick(kMhChannel, 1, 100), Dia::DeferredStep::KickResult::kLaunched);

	source.mKey = 2;
	g_probe->CallUpdate();
	EXPECT_EQ(GaugeValue("dia.deferredstep.in_flight"), 1.0);
	// Utilisation is sampled at Tick time: it must equal slotsHeld/workerCap for some value the gate held.
	const double util = GaugeValue("dia.deferredstep.cap_utilisation");
	EXPECT_GE(util, 0.0);
	EXPECT_LE(util, 1.0);
	EXPECT_GE(g_probe->ResolvedCap(), 1u);

	release.store(true);
	g_probe->GetScheduler().Unregister(kMhChannel);
	source.mKey = 3;
	g_probe->CallUpdate();
	EXPECT_EQ(GaugeValue("dia.deferredstep.in_flight"), 0.0);
	EXPECT_EQ(GaugeValue("dia.deferredstep.cap_utilisation"), 0.0);   // gate idle
}

TEST(DeferredStepModuleMetrics, InlineLauncherPublishesZeroCapUtilisation)
{
	MH_KeySource source;
	Harness h(false);
	ASSERT_TRUE(h.Start());
	g_probe->SetKeySource(source);
	source.mKey = 1;
	g_probe->CallUpdate();
	EXPECT_EQ(GaugeValue("dia.deferredstep.cap_utilisation"), 0.0);
}

TEST(DeferredStepModuleHealth, RegisteredOnStartAndUnregisteredOnStop)
{
	constexpr uint64_t kSec = 1000000000ull;
	HealthRegistry::Instance().Reset();   // drop retired/stale entries left by earlier tests in this process
	Harness h;
	ASSERT_TRUE(h.Start());
	HealthStatus status = HealthStatus::kOK;
	for (int cycle = 0; cycle < 3; ++cycle)
	{
		const uint64_t base = (1000 + cycle * 1000) * kSec;
		auto& reporter = g_probe->HealthReporter();
		reporter.SetNowForTest(base);
		reporter.NoteStall();

		ASSERT_TRUE(FindHealth(status));                 // live: snapshot calls Report()
		EXPECT_EQ(status, HealthStatus::kDegraded);

		g_probe->CallStop();
		reporter.SetNowForTest(base + 500 * kSec);       // a live registration would now read kOK
		ASSERT_TRUE(FindHealth(status));                 // retired copy, frozen at the last live report
		EXPECT_EQ(status, HealthStatus::kDegraded);

		g_probe->CallStart();
		ASSERT_TRUE(FindHealth(status));                 // live again: the 60 s window has passed
		EXPECT_EQ(status, HealthStatus::kOK);
	}
}

TEST(DeferredStepModuleHealth, OkInitially)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	const auto report = g_probe->HealthReporter().Report();
	EXPECT_EQ(report.status, HealthStatus::kOK);
	EXPECT_EQ(g_probe->HealthReporter().GetReporterName(), StringCRC("DeferredStep"));
}

TEST(DeferredStepModuleHealth, StallDegradesThenRecoversAfterSixtySeconds)
{
	MH_KeySource source;
	Harness h;
	ASSERT_TRUE(h.Start());
	g_probe->SetKeySource(source);
	constexpr uint64_t kSec = 1000000000ull;
	g_probe->HealthReporter().SetNowForTest(1000 * kSec);

	Dia::DeferredStep::Testing::RecordingStep step;
	ASSERT_TRUE(g_probe->GetScheduler().Register(kMhChannel, step));
	ForceStall(*g_probe, source, step, 1);
	ASSERT_GE(g_probe->GetScheduler().GetStats().stalls, 1u);

	auto report = g_probe->HealthReporter().Report();
	EXPECT_EQ(report.status, HealthStatus::kDegraded);
	EXPECT_EQ(report.reason, StringCRC("step_stall"));

	g_probe->HealthReporter().SetNowForTest(1059 * kSec);
	EXPECT_EQ(g_probe->HealthReporter().Report().status, HealthStatus::kDegraded);

	g_probe->HealthReporter().SetNowForTest(1061 * kSec);
	report = g_probe->HealthReporter().Report();
	EXPECT_EQ(report.status, HealthStatus::kOK);
	EXPECT_NE(report.reason, StringCRC("step_stall"));

	g_probe->GetScheduler().Unregister(kMhChannel);
}

TEST(DeferredStepModuleHealth, NeverFailingAndNonStallingStepsStayOk)
{
	MH_KeySource source;
	Harness h;
	ASSERT_TRUE(h.Start());
	g_probe->SetKeySource(source);
	Dia::DeferredStep::Testing::RecordingStep step;
	ASSERT_TRUE(g_probe->GetScheduler().Register(kMhChannel, step));
	ASSERT_EQ(g_probe->GetScheduler().Kick(kMhChannel, 1, 3), Dia::DeferredStep::KickResult::kLaunched);
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	for (StepKey k = 2; k <= 3; ++k) { source.mKey = k; g_probe->CallUpdate(); }
	EXPECT_EQ(g_probe->HealthReporter().Report().status, HealthStatus::kOK);

	// Repeated stalls: status is never kFailing at any point.
	for (StepKey k = 10; k < 14; ++k)
	{
		ForceStall(*g_probe, source, step, k);
		EXPECT_NE(g_probe->HealthReporter().Report().status, HealthStatus::kFailing);
	}
	g_probe->GetScheduler().Unregister(kMhChannel);
}

TEST(DeferredStepModuleHealth, ReportIsSafeFromASecondThreadDuringStalls)
{
	MH_KeySource source;
	Harness h;
	ASSERT_TRUE(h.Start());
	g_probe->SetKeySource(source);
	Dia::DeferredStep::Testing::RecordingStep step;
	ASSERT_TRUE(g_probe->GetScheduler().Register(kMhChannel, step));

	std::atomic<bool> stop{ false };
	std::atomic<unsigned> failing{ 0 };
	std::atomic<unsigned> reads{ 0 };
	const Dia::DeferredStep::DeferredStepHealth& reporter = g_probe->HealthReporter();
	std::thread reader([&]()
	{
		while (!stop.load())
		{
			if (reporter.Report().status == HealthStatus::kFailing) { failing.fetch_add(1); }
			reads.fetch_add(1);
		}
	});
	for (StepKey k = 1; k <= 3; ++k) { ForceStall(*g_probe, source, step, k); }
	stop.store(true);
	reader.join();

	EXPECT_GT(reads.load(), 0u);
	EXPECT_EQ(failing.load(), 0u);
	g_probe->GetScheduler().Unregister(kMhChannel);
}

// ---- Exhaustive pass: per-metric value tracking ----
TEST(DeferredStepModuleMetrics, KicksRejectedCountsEveryNonLaunchedResult)
{
	MH_KeySource source;
	Harness h;
	ASSERT_TRUE(h.Start());
	g_probe->SetKeySource(source);
	auto& sched = g_probe->GetScheduler();
	using Dia::DeferredStep::KickResult;
	std::atomic<bool> release{ false };
	Dia::DeferredStep::Testing::RecordingStep step;
	step.SetOnRun([&release]() { while (!release.load()) { std::this_thread::yield(); } });
	ASSERT_TRUE(sched.Register(kMhChannel, step));

	const uint64_t rejected0 = CounterValue("dia.deferredstep.kicks_rejected");
	const uint64_t kicks0 = CounterValue("dia.deferredstep.kicks");
	const uint64_t applies0 = CounterValue("dia.deferredstep.applies");

	EXPECT_EQ(sched.Kick(StringCRC("MH_NoSuchChannel"), 1, 2), KickResult::kUnknownChannel);
	EXPECT_EQ(sched.Kick(kMhChannel, 5, 3), KickResult::kApplyBeforeKick);
	EXPECT_EQ(sched.Kick(kMhChannel, 10, 12), KickResult::kLaunched);
	EXPECT_EQ(sched.Kick(kMhChannel, 11, 13), KickResult::kAlreadyInFlight);
	g_probe->CallUpdate();    // publishes
	EXPECT_EQ(CounterValue("dia.deferredstep.kicks_rejected") - rejected0, 3u);
	EXPECT_EQ(CounterValue("dia.deferredstep.kicks") - kicks0, 1u);

	release.store(true);
	source.mKey = 12;
	g_probe->CallUpdate();    // applies
	EXPECT_EQ(sched.Kick(kMhChannel, 5, 20), KickResult::kKeyNotMonotonic);
	g_probe->CallUpdate();
	EXPECT_EQ(CounterValue("dia.deferredstep.kicks_rejected") - rejected0, 4u);   // one per non-launched result, none for the accepted kick
	EXPECT_EQ(CounterValue("dia.deferredstep.kicks") - kicks0, 1u);
	EXPECT_EQ(CounterValue("dia.deferredstep.applies") - applies0, 1u);
	sched.Unregister(kMhChannel);
}

TEST(DeferredStepModuleMetrics, CapUtilisationIsSlotsHeldOverWorkerCapWhileStepInFlight)
{
	const struct { const char* json; double expected; } cases[] =
	{
		{ "{\"worker_cap\":1}", 1.0 },
		{ "{\"worker_cap\":2}", 0.5 },
		{ "{\"worker_cap\":4}", 0.25 },
	};
	for (const auto& c : cases)
	{
		SCOPED_TRACE(c.json);
		MH_KeySource source;
		Harness h(true, c.json, 8);
		ASSERT_TRUE(h.Start());
		g_probe->SetKeySource(source);
		std::atomic<bool> release{ false };
		Dia::DeferredStep::Testing::RecordingStep step;
		step.SetOnRun([&release]() { while (!release.load()) { std::this_thread::yield(); } });
		ASSERT_TRUE(g_probe->GetScheduler().Register(kMhChannel, step));
		ASSERT_EQ(g_probe->GetScheduler().Kick(kMhChannel, 1, 100), Dia::DeferredStep::KickResult::kLaunched);
		source.mKey = 2;
		g_probe->CallUpdate();
		EXPECT_EQ(GaugeValue("dia.deferredstep.cap_utilisation"), c.expected);
		release.store(true);
		g_probe->GetScheduler().Unregister(kMhChannel);
	}
}

TEST(DeferredStepModuleMetrics, InlineFallbacksCounterTracksFailedSubmits)
{
	MH_KeySource source;
	Harness h;
	ASSERT_TRUE(h.Start());
	g_probe->SetKeySource(source);
	Dia::DeferredStep::Testing::RecordingStep step;
	ASSERT_TRUE(g_probe->GetScheduler().Register(kMhChannel, step));
	const uint64_t inline0 = CounterValue("dia.deferredstep.inline_fallbacks");

	g_probe->Gate().SetSubmitFailureHookForTest([]() { return true; });
	for (StepKey k = 1; k <= 3; ++k)
	{
		ASSERT_EQ(g_probe->GetScheduler().Kick(kMhChannel, k, k + 1), Dia::DeferredStep::KickResult::kLaunched);
		EXPECT_EQ(step.GetRunThread(), std::this_thread::get_id());   // ran inline on SimPU
		source.mKey = k + 1;
		g_probe->CallUpdate();
		EXPECT_EQ(CounterValue("dia.deferredstep.inline_fallbacks") - inline0, k);
	}
	g_probe->Gate().SetSubmitFailureHookForTest(nullptr);
	for (int i = 0; i < 3; ++i) { g_probe->CallUpdate(); }
	EXPECT_EQ(CounterValue("dia.deferredstep.inline_fallbacks") - inline0, 3u);   // idle ticks add nothing
	g_probe->GetScheduler().Unregister(kMhChannel);
}

TEST(DeferredStepModuleMetrics, InlineLauncherStillTracksKicksAppliesAndNeverFallsBack)
{
	MH_KeySource source;
	Harness h(false);
	ASSERT_TRUE(h.Start());
	g_probe->SetKeySource(source);
	Dia::DeferredStep::Testing::RecordingStep step;
	ASSERT_TRUE(g_probe->GetScheduler().Register(kMhChannel, step));
	const uint64_t kicks0 = CounterValue("dia.deferredstep.kicks");
	const uint64_t applies0 = CounterValue("dia.deferredstep.applies");
	const uint64_t inline0 = CounterValue("dia.deferredstep.inline_fallbacks");
	const uint64_t stalls0 = CounterValue("dia.deferredstep.stalls");
	ASSERT_EQ(g_probe->GetScheduler().Kick(kMhChannel, 1, 2), Dia::DeferredStep::KickResult::kLaunched);
	source.mKey = 2;
	g_probe->CallUpdate();
	EXPECT_EQ(CounterValue("dia.deferredstep.kicks") - kicks0, 1u);
	EXPECT_EQ(CounterValue("dia.deferredstep.applies") - applies0, 1u);
	EXPECT_EQ(CounterValue("dia.deferredstep.inline_fallbacks") - inline0, 0u);
	EXPECT_EQ(CounterValue("dia.deferredstep.stalls") - stalls0, 0u);
	g_probe->GetScheduler().Unregister(kMhChannel);
}

TEST(DeferredStepModuleMetrics, CountersDoNotDoubleCountAcrossRestarts)
{
	// Restart retires then revives the metrics zeroed, so each run counts exactly its own scheduler's totals.
	MH_KeySource source;
	Harness h;
	ASSERT_TRUE(h.Start());
	g_probe->SetKeySource(source);

	for (unsigned cycle = 0; cycle < 3; ++cycle)
	{
		Dia::DeferredStep::Testing::RecordingStep step;
		step.SetOnRun([]() { std::this_thread::sleep_for(std::chrono::milliseconds(5)); });
		ASSERT_TRUE(g_probe->GetScheduler().Register(kMhChannel, step));
		ASSERT_EQ(g_probe->GetScheduler().Kick(kMhChannel, 1, 1), Dia::DeferredStep::KickResult::kLaunched);
		EXPECT_EQ(g_probe->GetScheduler().Kick(StringCRC("MH_NoSuchChannel"), 1, 1), Dia::DeferredStep::KickResult::kUnknownChannel);
		source.mKey = 1;
		g_probe->CallUpdate();
		g_probe->GetScheduler().Unregister(kMhChannel);

		const uint64_t stalls = g_probe->GetScheduler().GetStats().stalls;
		EXPECT_EQ(CounterValue("dia.deferredstep.kicks"), 1u);
		EXPECT_EQ(CounterValue("dia.deferredstep.applies"), 1u);
		EXPECT_EQ(CounterValue("dia.deferredstep.kicks_rejected"), 1u);
		EXPECT_EQ(CounterValue("dia.deferredstep.stalls"), stalls);

		EXPECT_EQ(g_probe->CallStop(), StopResult::kDone);
		EXPECT_EQ(g_probe->CallStart(), StartResult::kReady);
		g_probe->CallUpdate();     // fresh stats after restart add nothing
		EXPECT_EQ(CounterValue("dia.deferredstep.kicks"), 0u);
		EXPECT_EQ(CounterValue("dia.deferredstep.applies"), 0u);
		EXPECT_EQ(CounterValue("dia.deferredstep.kicks_rejected"), 0u);
		EXPECT_EQ(CounterValue("dia.deferredstep.stalls"), 0u);
	}
}
