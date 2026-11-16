#include <gtest/gtest.h>
#include <DiaObservation/Profile/TripRecord.h>
#include <DiaObservation/Profile/TripReporter.h>
#include <DiaObservation/Metric/MetricRegistry.h>

#include "DiaCore/Json/external/json/json.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace Dia::Observation::Profile;

namespace
{
	const char* TestResolver(uint32_t crc, char* buf, size_t sz)
	{
		snprintf(buf, sz, "mod%u", crc);
		return buf;
	}

	// Modules crc 1..3 in slots 0..2; module 2 is the heaviest. Entry i unit tick = 10 + i.
	void Fill(UnitTimingTable& t, uint32_t entries)
	{
		for (uint32_t i = 0; i < entries; ++i)
		{
			t.RecordModule(0, 1u, 1.0f);
			t.RecordModule(1, 2u, 5.0f);
			t.RecordModule(2, 3u, 3.0f);
			t.RecordUnitTick(10.0f + static_cast<float>(i));
			t.CommitEntry();
		}
	}

	TripResult MakeTrip(uint32_t first, uint32_t last, float peak = 30.0f, uint32_t suppressed = 0)
	{
		TripResult r;
		r.tripped = true;
		r.firstEntryIndex = first;
		r.lastEntryIndex = last;
		r.peakMs = peak;
		r.suppressedSinceLastTrip = suppressed;
		return r;
	}

	TripRecord Build(const UnitTimingTable& t, uint32_t first, uint32_t last)
	{
		TripRecord r;
		BuildTripRecord(r, "Sim", MakeTrip(first, last, 30.0f, 4), 16.6f, "Debug", t, &TestResolver);
		return r;
	}

	std::string TempDir()
	{
		char base[MAX_PATH];
		GetTempPathA(MAX_PATH, base);
		static int sCounter = 0;
		char dir[MAX_PATH];
		snprintf(dir, sizeof(dir), "%sTripReporterTest_%lu_%llu_%d\\", base, GetCurrentProcessId(),
			static_cast<unsigned long long>(GetTickCount64()), ++sCounter);
		CreateDirectoryA(dir, nullptr);
		return dir;
	}

	std::vector<std::string> ReadLines(const std::string& path)
	{
		std::ifstream f(path, std::ios::binary);
		std::vector<std::string> lines;
		std::string l;
		while (std::getline(f, l)) lines.push_back(l);
		return lines;
	}

	struct ReporterGuard
	{
		ReporterGuard() { TripReporter::Instance().Stop(); TripReporter::Instance().ResetCounters(); }
		~ReporterGuard()
		{
			TripReporter::Instance().SetWriterPausedForTest(false);
			TripReporter::Instance().Stop();
			TripReporter::Instance().ResetCounters();
		}
	};
}

TEST(TripRecord, TopModulesDescendingByMeanAndCappedAtFive)
{
	UnitTimingTable t;
	for (uint32_t i = 0; i < 20; ++i)
	{
		for (uint32_t s = 0; s < 7; ++s)
			t.RecordModule(s, 100u + s, 1.0f + static_cast<float>(s));   // slot 6 heaviest
		t.RecordUnitTick(20.0f);
		t.CommitEntry();
	}
	TripRecord r = Build(t, 5, 15);
	ASSERT_EQ(5u, r.topCount);
	EXPECT_STREQ("mod106", r.top[0].name);
	EXPECT_NEAR(7.0f, r.top[0].ms, 1e-4f);
	for (uint32_t i = 1; i < r.topCount; ++i)
		EXPECT_GE(r.top[i - 1].ms, r.top[i].ms);
	EXPECT_STREQ("mod102", r.top[4].name);
}

TEST(TripRecord, MeanIsOverTripWindowNotWholeRing)
{
	UnitTimingTable t;
	// 20 old entries at 1ms, then 4 recent at 9ms for one module.
	for (uint32_t i = 0; i < 24; ++i)
	{
		t.RecordModule(0, 7u, i < 20 ? 1.0f : 9.0f);
		t.RecordUnitTick(5.0f);
		t.CommitEntry();
	}
	TripRecord r = Build(t, 20, 23);   // range length 4
	ASSERT_EQ(1u, r.topCount);
	EXPECT_NEAR(9.0f, r.top[0].ms, 1e-4f);
}

TEST(TripRecord, WindowLongerThanDataUsesAvailableSamples)
{
	UnitTimingTable t;
	Fill(t, 3);
	TripRecord r = Build(t, 0, 500);   // range far larger than ring
	ASSERT_GE(r.topCount, 3u);
	EXPECT_STREQ("mod2", r.top[0].name);
	EXPECT_NEAR(5.0f, r.top[0].ms, 1e-4f);
}

TEST(TripRecord, RecentHoldsLastSixtyNewestLast)
{
	UnitTimingTable t;
	Fill(t, 100);
	TripRecord r = Build(t, 90, 99);
	ASSERT_EQ(60u, r.recentCount);
	EXPECT_FLOAT_EQ(10.0f + 40.0f, r.recent[0]);
	EXPECT_FLOAT_EQ(10.0f + 99.0f, r.recent[59]);
}

TEST(TripRecord, RecentHasFewerWhenFewerExist)
{
	UnitTimingTable t;
	Fill(t, 7);
	TripRecord r = Build(t, 0, 6);
	ASSERT_EQ(7u, r.recentCount);
	EXPECT_FLOAT_EQ(10.0f, r.recent[0]);
	EXPECT_FLOAT_EQ(16.0f, r.recent[6]);
}

TEST(TripRecord, EmptyTableProducesEmptyRecord)
{
	UnitTimingTable t;
	TripRecord r = Build(t, 0, 0);
	EXPECT_EQ(0u, r.topCount);
	EXPECT_EQ(0u, r.recentCount);
	EXPECT_EQ(0u, r.worstCount);
}

TEST(TripRecord, WorstFrameBreakdownIsPerModuleMsOfHighestUnitTick)
{
	UnitTimingTable t;
	for (uint32_t i = 0; i < 10; ++i)
	{
		const bool spike = (i == 6);
		t.RecordModule(0, 1u, spike ? 20.0f : 1.0f);
		t.RecordModule(1, 2u, spike ? 8.0f : 2.0f);
		t.RecordUnitTick(spike ? 40.0f : 5.0f);
		t.CommitEntry();
	}
	TripRecord r = Build(t, 5, 9);
	EXPECT_EQ(6u, r.worstIndex);
	EXPECT_FLOAT_EQ(40.0f, r.worstUnitMs);
	ASSERT_EQ(2u, r.worstCount);
	EXPECT_STREQ("mod1", r.worst[0].name);
	EXPECT_FLOAT_EQ(20.0f, r.worst[0].ms);
	EXPECT_STREQ("mod2", r.worst[1].name);
	EXPECT_FLOAT_EQ(8.0f, r.worst[1].ms);
}

TEST(TripRecord, CopiesHeaderFieldsAndLeavesSectionsEmpty)
{
	UnitTimingTable t;
	Fill(t, 5);
	TripRecord r = Build(t, 1, 4);
	EXPECT_STREQ("Sim", r.unit);
	EXPECT_STREQ("Debug", r.buildConfig);
	EXPECT_EQ(1u, r.firstFrame);
	EXPECT_EQ(4u, r.lastFrame);
	EXPECT_FLOAT_EQ(16.6f, r.budgetMs);
	EXPECT_FLOAT_EQ(30.0f, r.peakMs);
	EXPECT_EQ(4u, r.suppressedSinceLastTrip);
	EXPECT_EQ(0u, r.sectionCount);
}

TEST(TripRecord, LongUnitNameIsTruncatedNotOverflowed)
{
	UnitTimingTable t;
	TripRecord r;
	std::string longName(200, 'x');
	BuildTripRecord(r, longName.c_str(), MakeTrip(0, 0), 10.0f, "Release", t, &TestResolver);
	EXPECT_EQ(static_cast<size_t>(TripRecord::kUnitLen - 1), strlen(r.unit));
}

TEST(TripRecord, JsonLineParsesBackWithExpectedFields)
{
	UnitTimingTable t;
	Fill(t, 12);
	TripRecord r = Build(t, 4, 11);
	char buf[16384];
	size_t len = 0;
	SerializeTripRecord(r, buf, sizeof(buf), len);
	ASSERT_GT(len, 1u);
	ASSERT_EQ('\n', buf[len - 1]);
	EXPECT_EQ(len - 1, strlen(buf) - 1);
	for (size_t i = 0; i + 1 < len; ++i) ASSERT_NE('\n', buf[i]);   // exactly one line

	Json::Value v;
	Json::CharReaderBuilder rb;
	std::string errs;
	std::istringstream in(std::string(buf, len));
	ASSERT_TRUE(Json::parseFromStream(rb, in, &v, &errs)) << errs;
	EXPECT_EQ("Sim", v["unit"].asString());
	EXPECT_EQ("Debug", v["build_config"].asString());
	EXPECT_EQ(4u, v["first_frame"].asUInt());
	EXPECT_EQ(11u, v["last_frame"].asUInt());
	EXPECT_NEAR(16.6, v["budget_ms"].asDouble(), 1e-3);
	EXPECT_NEAR(30.0, v["peak_ms"].asDouble(), 1e-3);
	EXPECT_EQ(4u, v["suppressed_since_last_trip"].asUInt());
	ASSERT_TRUE(v["top_modules"].isArray());
	EXPECT_EQ("mod2", v["top_modules"][0]["module"].asString());
	EXPECT_NEAR(5.0, v["top_modules"][0]["ms"].asDouble(), 1e-3);
	EXPECT_EQ(12u, v["recent_unit_ms"].size());
	EXPECT_TRUE(v["worst_frame"]["modules"].isArray());
	EXPECT_TRUE(v["sections"].isArray());
}

TEST(TripReporter, SubmitWithNoSessionOnlyCounts)
{
	ReporterGuard g;
	UnitTimingTable t;
	Fill(t, 5);
	TripRecord r = Build(t, 0, 4);
	TripReporter& rep = TripReporter::Instance();
	EXPECT_FALSE(rep.IsStarted());
	EXPECT_FALSE(rep.Submit(r));
	EXPECT_FALSE(rep.Submit(r));
	EXPECT_EQ(2u, rep.GetSubmitted());
	EXPECT_EQ(0u, rep.GetWritten());
	EXPECT_EQ(0u, rep.GetDropped());
}

TEST(TripReporter, WritesExactlyOneLinePerRecordToSessionDir)
{
	ReporterGuard g;
	UnitTimingTable t;
	Fill(t, 5);
	const std::string dir = TempDir();
	TripReporter& rep = TripReporter::Instance();
	ASSERT_TRUE(rep.Start(dir.c_str()));
	for (uint32_t i = 0; i < 5; ++i)
	{
		TripRecord r = Build(t, i, i + 1);
		EXPECT_TRUE(rep.Submit(r));
		// keep under queue capacity regardless of writer speed
		while (rep.GetWritten() + rep.GetDropped() < i + 1) std::this_thread::yield();
	}
	rep.Stop();
	EXPECT_EQ(5u, rep.GetWritten());
	EXPECT_EQ(0u, rep.GetDropped());
	auto lines = ReadLines(dir + "trips.jsonl");
	ASSERT_EQ(5u, lines.size());
	for (const std::string& l : lines)
	{
		Json::Value v;
		Json::CharReaderBuilder rb;
		std::string errs;
		std::istringstream in(l);
		EXPECT_TRUE(Json::parseFromStream(rb, in, &v, &errs)) << errs;
		EXPECT_EQ("Sim", v["unit"].asString());
	}
}

TEST(TripReporter, QueueFullDropsAndCountsNewRecords)
{
	ReporterGuard g;
	UnitTimingTable t;
	Fill(t, 5);
	const std::string dir = TempDir();
	TripReporter& rep = TripReporter::Instance();
	ASSERT_TRUE(rep.Start(dir.c_str()));
	rep.SetWriterPausedForTest(true);
	TripRecord r = Build(t, 0, 4);
	const uint32_t cap = TripReporter::kQueueCapacity;
	for (uint32_t i = 0; i < cap + 3; ++i)
		rep.Submit(r);
	EXPECT_EQ(3u, rep.GetDropped());
	EXPECT_EQ(cap + 3, rep.GetSubmitted());
	rep.SetWriterPausedForTest(false);
	rep.Stop();
	EXPECT_EQ(cap, rep.GetWritten());
	EXPECT_EQ(cap, ReadLines(dir + "trips.jsonl").size());

	Dia::Observation::Metric::Gauge* gd =
		Dia::Observation::Metric::MetricRegistry::Instance().FindGauge(Dia::Core::StringCRC("perf.trips.dropped"));
	ASSERT_NE(nullptr, gd);
	EXPECT_DOUBLE_EQ(3.0, gd->Value());
}

TEST(TripReporter, SubmitDoesNotBlockWhileWriterIsStalled)
{
	ReporterGuard g;
	UnitTimingTable t;
	Fill(t, 5);
	const std::string dir = TempDir();
	TripReporter& rep = TripReporter::Instance();
	ASSERT_TRUE(rep.Start(dir.c_str()));
	rep.SetWriterPausedForTest(true);
	TripRecord r = Build(t, 0, 4);
	auto t0 = std::chrono::steady_clock::now();
	for (int i = 0; i < 1000; ++i) rep.Submit(r);
	auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
	EXPECT_LT(ms, 500.0);   // 1000 submits against a stalled writer return immediately
	EXPECT_EQ(1000u - TripReporter::kQueueCapacity, rep.GetDropped());
}

TEST(TripReporter, StartOnUnwritableDirectoryCountsFailureAndStaysStopped)
{
	ReporterGuard g;
	TripReporter& rep = TripReporter::Instance();
	EXPECT_FALSE(rep.Start("Z:\\definitely\\missing\\dir\\"));
	EXPECT_FALSE(rep.IsStarted());
	EXPECT_EQ(1u, rep.GetWriteFailures());
	UnitTimingTable t;
	TripRecord r = Build(t, 0, 0);
	EXPECT_FALSE(rep.Submit(r));   // no crash
}

TEST(TripReporter, ReportSubmitsAndNeverThrows)
{
	ReporterGuard g;
	UnitTimingTable t;
	Fill(t, 5);
	TripRecord r = Build(t, 0, 4);
	EXPECT_NO_THROW(TripReporter::Instance().Report(r));
	EXPECT_EQ(1u, TripReporter::Instance().GetSubmitted());
}
