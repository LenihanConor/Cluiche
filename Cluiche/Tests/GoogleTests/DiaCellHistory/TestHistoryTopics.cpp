#include "HistoryTestRig.h"
#include <DiaCellHistory/HistoryExporter.h>
#include <DiaCellHistory/HistoryQueries.h>
#include <DiaCellHistory/HistoryTopics.h>
#include <DiaCellHistory/TopicObservation.h>
#include <DiaCellHistory/Testing/ExportDecoder.h>
#include <DiaCellHistory/Testing/FakeSnapshotStore.h>
#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/MetricRegistry.h>

#include <json/json.h>

#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#ifdef DIA_DEBUG

using namespace Dia::CellHistory;
using namespace Dia::CellHistory::Testing;
using namespace HistoryRig;
using Dia::CellLayers::Testing::LogCapture;
using Dia::Core::StringCRC;
using Dia::Observation::Metric::MetricRegistry;

namespace
{
	const char* const kAggTopic = "cells.history.aggregates";
	const char* const kEvTopic = "cells.history.events";

	const char* kEventEntry = "{\"key\":\"cow.drought\",\"kind\":\"event\",\"type\":\"string\",\"label\":\"Drought\",\"description\":\"d\","
		"\"payload\":[{\"name\":\"weeks\",\"type\":\"int\",\"description\":\"d\"}]}";

	uint64_t C(const char* n) { auto* c = MetricRegistry::Instance().FindCounter(StringCRC(n)); return c ? c->Value() : 0; }

	size_t Size(const Json::Value& v)
	{
		Json::StreamWriterBuilder b;
		b["indentation"] = "";
		return Json::writeString(b, v).size();
	}

	bool JsonEq(const Json::Value& a, const Json::Value& b)
	{
		Json::StreamWriterBuilder w;
		w["indentation"] = "";
		return Json::writeString(w, a) == Json::writeString(w, b);
	}

	struct FakePub : ITopicPublisher
	{
		struct Msg { StringCRC topic; Json::Value payload; };
		std::vector<Msg> msgs;
		uint32_t subs = 0;
		int rejectNext = 0;			// reject this many upcoming messages
		uint32_t attempts = 0;
		bool Publish(StringCRC topic, const Json::Value& payload) override
		{
			++attempts;
			if (rejectNext > 0) { --rejectNext; return false; }
			msgs.push_back({ topic, payload });
			return true;
		}
		uint32_t SubscriberCount() const override { return subs; }
		std::vector<Json::Value> Of(const char* topic) const
		{
			std::vector<Json::Value> out;
			for (const Msg& m : msgs) { if (m.topic == StringCRC(topic)) { out.push_back(m.payload); } }
			return out;
		}
		size_t Count(const char* topic) const { return Of(topic).size(); }
	};

	struct StepClock
	{
		double t = 0.0;
		static double Read(void* user) { return static_cast<StepClock*>(user)->t; }
	};

	struct TFix
	{
		Rig r;
		FakePub pub;
		StepClock clock;
		std::unique_ptr<HistoryTopics> topics;
		uint32_t next = 0;			// next sample variation index
		uint32_t eventSeq = 0;

		void Setup(const RecorderBudgets* budgets = nullptr)
		{
			r.grid.AddFloatLayer("temp", 40.0f);
			r.grid.AddCategoryLayer("biome", 1);
			r.grid.AddCategoryLayer("region", 0);
			ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100), EnumEntry("biome", 3), EnumEntry("region", 3, "{\"group\":true}"), kEventEntry }));
			r.Start(budgets);
			topics = std::make_unique<HistoryTopics>(*r.rec, pub);
			topics->SetClock(&StepClock::Read, &clock);
		}
		void Vary(uint32_t i)
		{
			for (uint32_t c = 0; c < 12; ++c)
			{
				r.grid.Set("temp", c, float((i * 7 + c * 3) % 101) + 0.37f);
				r.grid.Set("biome", c, float((c + i) % 3));
				r.grid.Set("region", c, float((c / 4 + i) % 3));
			}
			r.grid.BumpVersion("temp");
			r.grid.BumpVersion("biome");
			r.grid.BumpVersion("region");
		}
		void Emit(uint32_t n)
		{
			for (uint32_t j = 0; j < n; ++j)
			{
				HistoryEvent e;
				e.type = StringCRC("cow.drought");
				const std::string payload = "{\"weeks\":" + std::to_string(eventSeq) + "}";
				const std::string text = "e" + std::to_string(eventSeq);
				e.payloadJson = payload.c_str();
				e.text = text.c_str();
				if (eventSeq % 2 == 0) { e.group.mLayer = StringCRC("region"); e.group.mValue = eventSeq % 3; }
				if (eventSeq % 3 == 0) { e.cell = eventSeq % 12; }
				r.rec->Emit(e);
				++eventSeq;
			}
		}
		// n samples, each preceded by `events` events.
		void Step(uint32_t n, uint32_t events = 0)
		{
			for (uint32_t i = 0; i < n; ++i)
			{
				Emit(events);
				Vary(next++);
				r.Take();
			}
		}
		void TickAt(double t) { clock.t = t; topics->Tick(); }
	};

	// Per-key accumulation of decoded rows from query pages or topic messages, from sample 0.
	struct Series
	{
		std::map<std::string, std::vector<uint32_t>> floats;	// "layer|group|which" -> float bits per sample
		std::map<std::string, std::vector<uint16_t>> cats;		// "layer|group" -> sample-major counts
		std::map<std::string, std::vector<uint16_t>> sizes;		// group -> per sample
		std::map<std::string, uint32_t> enumCount;				// layer -> enum count
		uint32_t samples = 0;

		void Add(const Json::Value& msg)
		{
			const uint32_t n = msg["count"].asUInt();
			for (const std::string& g : msg["groupSizes"].getMemberNames())
			{
				std::vector<uint16_t> v;
				ASSERT_TRUE(DecodedExport::DecodeU16(msg["groupSizes"][g], n, v));
				sizes[g].insert(sizes[g].end(), v.begin(), v.end());
			}
			for (const std::string& layer : msg["rows"].getMemberNames())
			{
				const Json::Value& l = msg["rows"][layer];
				for (const std::string& g : l["groups"].getMemberNames())
				{
					if (l["kind"].asString() == "float")
					{
						for (const char* which : { "min", "mean", "max" })
						{
							std::vector<float> v;
							ASSERT_TRUE(DecodedExport::DecodeF32(l["groups"][g][which], n, v));
							std::vector<uint32_t>& dst = floats[layer + "|" + g + "|" + which];
							for (float f : v) { uint32_t u; std::memcpy(&u, &f, 4); dst.push_back(u); }
						}
					}
					else
					{
						const uint32_t e = l["values"].size();
						enumCount[layer] = e;
						std::vector<uint16_t> v;
						ASSERT_TRUE(DecodedExport::DecodeU16(l["groups"][g], n * e, v));
						std::vector<uint16_t>& dst = cats[layer + "|" + g];
						dst.insert(dst.end(), v.begin(), v.end());
					}
				}
			}
			samples += n;
		}

		// Everything equals the file's aggregates, byte for byte (a sample before a layer's `first` is skipped).
		void ExpectEqualsFile(const DecodedExport& dec) const
		{
			EXPECT_EQ(samples, dec.SampleCount());
			ASSERT_EQ(dec.AggregateFirst(), 0u);
			for (const std::string& g : dec.root["groupSizes"].getMemberNames())
			{
				std::vector<uint16_t> fileSizes;
				ASSERT_TRUE(dec.GroupSizes(g.c_str(), fileSizes));
				ASSERT_TRUE(sizes.count(g) != 0) << g;
				EXPECT_EQ(sizes.at(g), fileSizes) << g;
			}
			for (const std::string& layer : dec.root["aggregates"].getMemberNames())
			{
				const Json::Value& l = dec.root["aggregates"][layer];
				for (const std::string& g : l["groups"].getMemberNames())
				{
					if (l["kind"].asString() == "float")
					{
						for (const char* which : { "min", "mean", "max" })
						{
							std::vector<float> fileSeries;
							uint32_t first = 0;
							ASSERT_TRUE(dec.FloatSeries(layer.c_str(), g.c_str(), which, fileSeries, first));
							const auto it = floats.find(layer + "|" + g + "|" + which);
							ASSERT_TRUE(it != floats.end());
							ASSERT_EQ(it->second.size(), size_t(dec.SampleCount()));
							for (uint32_t s = first; s < dec.SampleCount(); ++s)
							{
								uint32_t u; std::memcpy(&u, &fileSeries[s - first], 4);
								ASSERT_EQ(it->second[s], u) << layer << "/" << g << "/" << which << " sample " << s;
							}
						}
					}
					else
					{
						std::vector<uint16_t> fileSeries;
						uint32_t first = 0, e = 0;
						ASSERT_TRUE(dec.CategorySeries(layer.c_str(), g.c_str(), fileSeries, first, e));
						const auto it = cats.find(layer + "|" + g);
						ASSERT_TRUE(it != cats.end());
						ASSERT_EQ(it->second.size(), size_t(dec.SampleCount()) * e);
						for (size_t i = size_t(first) * e; i < it->second.size(); ++i) { ASSERT_EQ(it->second[i], fileSeries[i - size_t(first) * e]) << layer << "/" << g << " idx " << i; }
					}
				}
			}
		}
	};

	std::vector<Json::Value> AllEvents(const std::vector<Json::Value>& msgs)
	{
		std::vector<Json::Value> out;
		for (const Json::Value& m : msgs) { for (const Json::Value& e : m["events"]) { out.push_back(e); } }
		return out;
	}
}

// ---- aggregates: rate, new rows, structure --------------------------------------------------------------------------------

TEST(HistoryTopicsAggregates, RateLimitedToTheInterval)
{
	TFix f; f.Setup();
	f.pub.subs = 1;
	f.Step(1);
	f.TickAt(10.0);
	ASSERT_EQ(f.pub.Count(kAggTopic), 1u);	// the first publication is not delayed
	f.Step(1);
	f.TickAt(10.5);
	EXPECT_EQ(f.pub.Count(kAggTopic), 1u);	// before the interval
	f.TickAt(10.99);
	EXPECT_EQ(f.pub.Count(kAggTopic), 1u);
	f.TickAt(11.0);
	EXPECT_EQ(f.pub.Count(kAggTopic), 2u);	// at the interval
	f.Step(1);
	f.TickAt(11.0);
	EXPECT_EQ(f.pub.Count(kAggTopic), 2u);	// never twice within one interval
	f.TickAt(11.5);
	EXPECT_EQ(f.pub.Count(kAggTopic), 2u);
	f.TickAt(12.0);
	EXPECT_EQ(f.pub.Count(kAggTopic), 3u);
}

TEST(HistoryTopicsAggregates, IntervalIsConfigurable)
{
	TFix f; f.Setup();
	f.topics->SetMinAggregateInterval(0.25);
	EXPECT_DOUBLE_EQ(f.topics->MinAggregateInterval(), 0.25);
	f.pub.subs = 1;
	f.Step(1); f.TickAt(1.0);
	f.Step(1); f.TickAt(1.2);
	EXPECT_EQ(f.pub.Count(kAggTopic), 1u);
	f.TickAt(1.25);
	EXPECT_EQ(f.pub.Count(kAggTopic), 2u);
}

TEST(HistoryTopicsAggregates, OnlyNewRowsEachMessageStartsWhereTheLastEnded)
{
	TFix f; f.Setup();
	f.pub.subs = 1;
	f.Step(3); f.TickAt(0.0);
	f.Step(2); f.TickAt(2.0);
	f.Step(4); f.TickAt(4.0);
	const std::vector<Json::Value> m = f.pub.Of(kAggTopic);
	ASSERT_EQ(m.size(), 3u);
	EXPECT_EQ(m[0]["fromSample"].asUInt(), 0u);	EXPECT_EQ(m[0]["count"].asUInt(), 3u);
	EXPECT_EQ(m[1]["fromSample"].asUInt(), 3u);	EXPECT_EQ(m[1]["count"].asUInt(), 2u);
	EXPECT_EQ(m[2]["fromSample"].asUInt(), 5u);	EXPECT_EQ(m[2]["count"].asUInt(), 4u);
	EXPECT_EQ(m[1]["samples"].size(), 2u);
	EXPECT_EQ(m[1]["samples"][0u]["label"].asString(), f.r.rec->Header(3).mLabel.AsCStr());
	EXPECT_EQ(m[2]["samples"][3u]["tick"].asUInt64(), f.r.rec->Header(8).mTick);
	EXPECT_EQ(m[0]["structure"].asString().size(), 16u);
}

TEST(HistoryTopicsAggregates, NothingWithoutNewSamples)
{
	TFix f; f.Setup();
	f.pub.subs = 1;
	f.Step(2); f.TickAt(0.0);
	ASSERT_EQ(f.pub.Count(kAggTopic), 1u);
	f.TickAt(5.0);
	f.TickAt(10.0);
	EXPECT_EQ(f.pub.Count(kAggTopic), 1u);
	EXPECT_EQ(f.pub.Count(kEvTopic), 0u);
	f.Step(1); f.TickAt(11.0);
	EXPECT_EQ(f.pub.Count(kAggTopic), 2u);
}

TEST(HistoryTopicsAggregates, FirstPublicationStartsAtTheCurrentSampleCount)
{
	TFix f; f.Setup();
	f.Step(5);
	f.topics = std::make_unique<HistoryTopics>(*f.r.rec, f.pub);	// built mid-run
	f.topics->SetClock(&StepClock::Read, &f.clock);
	f.pub.subs = 1;
	f.TickAt(0.0);
	EXPECT_EQ(f.pub.msgs.size(), 0u);	// no replay: catch-up is the queries' job
	f.Step(2); f.TickAt(1.0);
	ASSERT_EQ(f.pub.Count(kAggTopic), 1u);
	EXPECT_EQ(f.pub.Of(kAggTopic)[0]["fromSample"].asUInt(), 5u);
	EXPECT_EQ(f.pub.Of(kAggTopic)[0]["count"].asUInt(), 2u);
}

TEST(HistoryTopicsAggregates, StructureChangeIsFlaggedOnceAndCarried)
{
	TFix f; f.Setup();
	f.pub.subs = 1;
	const uint64_t before = f.r.rec->StructureHash();	// the layout resolves with the first sample
	f.Step(1);
	const uint64_t after = f.r.rec->StructureHash();
	ASSERT_NE(before, after);
	f.TickAt(0.0);
	ASSERT_EQ(f.pub.Count(kAggTopic), 1u);
	char hex[24];
	snprintf(hex, sizeof(hex), "%016llx", (unsigned long long)after);
	const Json::Value first = f.pub.Of(kAggTopic)[0];
	EXPECT_EQ(first["structure"].asString(), hex);
	EXPECT_TRUE(first["structureChanged"].asBool());
	f.Step(1); f.TickAt(1.0);
	const Json::Value second = f.pub.Of(kAggTopic)[1];
	EXPECT_EQ(second["structure"].asString(), hex);
	EXPECT_FALSE(second.isMember("structureChanged"));	// flagged once, then the topics continue on the new layout
	EXPECT_EQ(second["fromSample"].asUInt(), 1u);
}

TEST(HistoryTopicsAggregates, StructureChangeAtTheFirstSampleIsFlaggedOnTheNextPublication)
{
	TFix f; f.Setup();
	f.pub.subs = 1;
	f.TickAt(0.0);						// subscribed, nothing recorded yet
	f.Step(1);							// layout resolves: structure changes
	f.TickAt(0.1);
	ASSERT_EQ(f.pub.Count(kAggTopic), 1u);
	EXPECT_TRUE(f.pub.Of(kAggTopic)[0]["structureChanged"].asBool());
}

TEST(HistoryTopicsAggregates, BacklogIsSplitIntoMessagesWithinTheCap)
{
	TFix f; f.Setup();
	f.topics->SetMaxMessageBytes(1200);
	f.pub.subs = 1;
	f.Step(20);					// a long pause: 20 samples pile up before the first Tick
	f.TickAt(0.0);
	const size_t afterFirst = f.pub.Count(kAggTopic);
	EXPECT_GE(afterFirst, 2u);
	EXPECT_LE(afterFirst, size_t(HistoryTopics::kMaxAggregateMessagesPerTick));
	for (int i = 1; i < 30; ++i) { f.TickAt(double(i)); }		// the rest follows on later intervals
	const std::vector<Json::Value> m = f.pub.Of(kAggTopic);
	uint32_t expect = 0;
	for (const Json::Value& msg : m)
	{
		EXPECT_LE(Size(msg), 1200u);
		EXPECT_EQ(msg["fromSample"].asUInt(), expect);
		EXPECT_GE(msg["count"].asUInt(), 1u);
		expect += msg["count"].asUInt();
	}
	EXPECT_EQ(expect, 20u);
	EXPECT_GT(m.size(), 2u);
}

TEST(HistoryTopicsAggregates, ALargeBacklogFitsOneMessageUnderTheDefaultCap)
{
	TFix f; f.Setup();
	f.pub.subs = 1;
	f.Step(150);
	f.TickAt(0.0);
	ASSERT_EQ(f.pub.Count(kAggTopic), 1u);
	EXPECT_EQ(f.pub.Of(kAggTopic)[0]["count"].asUInt(), 150u);
	EXPECT_LE(Size(f.pub.Of(kAggTopic)[0]), size_t(HistoryTopics::kDefaultMaxMessageBytes));
}

TEST(HistoryTopicsAggregates, TopicRowsEqualTheQueryRowsForTheSameRange)
{
	TFix f; f.Setup();
	f.pub.subs = 1;
	f.Step(3); f.TickAt(0.0);
	HistoryQueries q(*f.r.rec);
	Json::Value args(Json::objectValue);
	args["from"] = 0; args["to"] = 3;
	const Json::Value query = q.Aggregates(args);
	ASSERT_FALSE(query.isMember("error"));
	ASSERT_EQ(f.pub.Count(kAggTopic), 1u);
	const Json::Value topic = f.pub.Of(kAggTopic)[0];
	EXPECT_TRUE(JsonEq(topic["samples"], query["samples"]));
	EXPECT_TRUE(JsonEq(topic["groupSizes"], query["groupSizes"]));
	EXPECT_TRUE(JsonEq(topic["rows"], query["rows"]));
}

TEST(HistoryTopicsAggregates, ConcatenatedTopicRowsEqualTheFile)
{
	TFix f; f.Setup();
	f.pub.subs = 1;
	TempFolder tmp;
	double t = 0.0;
	for (int round = 0; round < 6; ++round) { f.Step(4); f.TickAt(t += 1.0); }
	f.Step(1); f.TickAt(t += 1.0);
	Series s;
	for (const Json::Value& m : f.pub.Of(kAggTopic)) { s.Add(m); }
	ExportOptions opt; opt.overwrite = true;
	ExportResult res;
	const std::string path = tmp.File("run.cell_history.json");
	ASSERT_TRUE(HistoryExporter::WriteFile(*f.r.rec, opt, path.c_str(), res));
	DecodedExport dec;
	ASSERT_TRUE(dec.Load(path.c_str()));
	s.ExpectEqualsFile(dec);
}

// ---- no subscribers --------------------------------------------------------------------------------------------------------

TEST(HistoryTopicsNoSubscribers, DoNoWorkAndAdvanceTheCursorsToTheHead)
{
	TFix f; f.Setup();
	f.pub.subs = 0;
	f.Step(4, 2);
	f.TickAt(0.0); f.TickAt(5.0);
	EXPECT_EQ(f.pub.msgs.size(), 0u);
	EXPECT_EQ(f.pub.attempts, 0u);
	f.pub.subs = 1;
	f.TickAt(10.0);
	EXPECT_EQ(f.pub.msgs.size(), 0u);	// the backlog is not delivered to a late subscriber
	f.Step(1, 1); f.TickAt(11.0);
	ASSERT_EQ(f.pub.Count(kAggTopic), 1u);
	EXPECT_EQ(f.pub.Of(kAggTopic)[0]["fromSample"].asUInt(), 4u);
	const std::vector<Json::Value> ev = AllEvents(f.pub.Of(kEvTopic));
	ASSERT_EQ(ev.size(), 1u);
	EXPECT_EQ(ev[0]["sample"].asUInt(), 4u);
	EXPECT_EQ(ev[0]["text"].asString(), "e8");
}

TEST(HistoryTopicsNoSubscribers, SubscriberLeavingStopsPublishingAndSkipsTheGap)
{
	TFix f; f.Setup();
	f.pub.subs = 1;
	f.Step(1, 1); f.TickAt(0.0);
	ASSERT_EQ(f.pub.msgs.size(), 2u);
	f.pub.subs = 0;
	f.Step(3, 1); f.TickAt(5.0);
	EXPECT_EQ(f.pub.msgs.size(), 2u);
	f.pub.subs = 1;
	f.Step(1, 1); f.TickAt(10.0);
	ASSERT_EQ(f.pub.Count(kAggTopic), 2u);
	EXPECT_EQ(f.pub.Of(kAggTopic)[1]["fromSample"].asUInt(), 4u);
}

// ---- events ----------------------------------------------------------------------------------------------------------------

TEST(HistoryTopicsEvents, EachEventIsDeliveredExactlyOnceInFileShape)
{
	TFix f; f.Setup();
	f.pub.subs = 1;
	TempFolder tmp;
	for (int i = 0; i < 6; ++i) { f.Step(1, uint32_t(i % 4) + 1u); f.TickAt(double(i)); f.TickAt(double(i) + 0.5); }
	const std::vector<Json::Value> events = AllEvents(f.pub.Of(kEvTopic));
	ExportOptions opt; opt.overwrite = true;
	ExportResult res;
	const std::string path = tmp.File("run.cell_history.json");
	ASSERT_TRUE(HistoryExporter::WriteFile(*f.r.rec, opt, path.c_str(), res));
	DecodedExport dec;
	ASSERT_TRUE(dec.Load(path.c_str()));
	ASSERT_EQ(events.size(), size_t(dec.root["events"].size()));
	ASSERT_GT(events.size(), 0u);
	for (size_t i = 0; i < events.size(); ++i) { EXPECT_TRUE(JsonEq(events[i], dec.root["events"][Json::ArrayIndex(i)])) << i; }
	for (const Json::Value& m : f.pub.Of(kEvTopic)) { EXPECT_TRUE(m.isMember("events")); EXPECT_FALSE(m.isMember("structure")); }
}

TEST(HistoryTopicsEvents, BatchedAtTwoHundredFiftySix)
{
	TFix f; f.Setup();
	f.pub.subs = 1;
	f.Step(1, 600);
	f.TickAt(0.0);
	const std::vector<Json::Value> m = f.pub.Of(kEvTopic);
	ASSERT_EQ(m.size(), 3u);
	EXPECT_EQ(m[0]["events"].size(), 256u);
	EXPECT_EQ(m[1]["events"].size(), 256u);
	EXPECT_EQ(m[2]["events"].size(), 88u);
	f.TickAt(0.1);
	EXPECT_EQ(f.pub.Count(kEvTopic), 3u);	// nothing is sent twice
	EXPECT_EQ(m[0]["events"][0u]["text"].asString(), "e0");
	EXPECT_EQ(m[2]["events"][87u]["text"].asString(), "e599");
}

TEST(HistoryTopicsEvents, MessageCountPerTickIsBoundedAndTheCursorResumesMidSample)
{
	TFix f; f.Setup();
	f.topics->SetMaxEventsPerMessage(1);
	EXPECT_EQ(f.topics->MaxEventsPerMessage(), 1u);
	f.pub.subs = 1;
	f.Step(1, 10);
	f.TickAt(0.0);
	EXPECT_EQ(f.pub.Count(kEvTopic), size_t(HistoryTopics::kMaxEventMessagesPerTick));
	f.Step(1, 2);							// a later sample queues behind the unfinished one
	f.TickAt(0.1);
	const std::vector<Json::Value> events = AllEvents(f.pub.Of(kEvTopic));
	ASSERT_EQ(events.size(), 12u);
	for (size_t i = 0; i < events.size(); ++i)
	{
		EXPECT_EQ(events[i]["text"].asString(), "e" + std::to_string(i)) << i;	// in order, no repeat, no gap
		EXPECT_EQ(events[i]["sample"].asUInt(), i < 10 ? 0u : 1u);
	}
}

TEST(HistoryTopicsEvents, MaxEventsPerMessageIsClamped)
{
	TFix f; f.Setup();
	f.topics->SetMaxEventsPerMessage(100000);
	EXPECT_EQ(f.topics->MaxEventsPerMessage(), 256u);
	f.topics->SetMaxEventsPerMessage(0);
	EXPECT_EQ(f.topics->MaxEventsPerMessage(), 1u);
}

TEST(HistoryTopicsEvents, EventsBecomeVisibleWhenTheirSampleIsCommitted)
{
	TFix f; f.Setup();
	f.pub.subs = 1;
	f.Emit(3);
	f.TickAt(0.0);
	EXPECT_EQ(f.pub.Count(kEvTopic), 0u);	// pending, not yet attached to a sample
	f.Vary(0); f.r.Take();
	f.Emit(2);
	f.Vary(1); f.r.Take();				// two samples between Ticks
	f.TickAt(0.1);
	const std::vector<Json::Value> events = AllEvents(f.pub.Of(kEvTopic));
	ASSERT_EQ(events.size(), 5u);
	EXPECT_EQ(events[0]["sample"].asUInt(), 0u);
	EXPECT_EQ(events[3]["sample"].asUInt(), 1u);
	EXPECT_EQ(f.pub.Count(kEvTopic), 1u);	// one batch for both samples
}

TEST(HistoryTopicsEvents, UnresolvedTypeReadsAsCrc)
{
	TFix f; f.Setup();
	f.pub.subs = 1;
	HistoryEvent e;
	e.type = StringCRC("not.in.schema");
	f.r.rec->Emit(e);
	f.Vary(0); f.r.Take();
	f.TickAt(0.0);
	const std::vector<Json::Value> events = AllEvents(f.pub.Of(kEvTopic));
	ASSERT_EQ(events.size(), 1u);
	char expect[24];
	snprintf(expect, sizeof(expect), "crc:%08x", StringCRC("not.in.schema").Value());
	EXPECT_EQ(events[0]["type"].asString(), expect);
}

// ---- truncate / reset ------------------------------------------------------------------------------------------------------

TEST(HistoryTopicsReset, TruncateMovesTheCursorsToTheNewHeadWithoutReplay)
{
	TFix f; f.Setup();
	f.pub.subs = 1;
	f.Step(5, 1); f.TickAt(0.0);
	const size_t aggBefore = f.pub.Count(kAggTopic), evBefore = f.pub.Count(kEvTopic);
	f.r.rec->TruncateAfter(2);			// head 3
	f.TickAt(1.0);
	EXPECT_EQ(f.pub.Count(kAggTopic), aggBefore);	// nothing replayed, nothing re-sent
	EXPECT_EQ(f.pub.Count(kEvTopic), evBefore);
	f.Step(1, 1); f.TickAt(2.0);
	ASSERT_EQ(f.pub.Count(kAggTopic), aggBefore + 1);
	const Json::Value last = f.pub.Of(kAggTopic).back();
	EXPECT_EQ(last["fromSample"].asUInt(), 3u);	// the client sees fromSample < its end and re-fetches
	EXPECT_EQ(last["count"].asUInt(), 1u);
	const std::vector<Json::Value> ev = AllEvents(f.pub.Of(kEvTopic));
	ASSERT_EQ(ev.size(), 6u);
	EXPECT_EQ(ev.back()["sample"].asUInt(), 3u);	// exactly the one new event, not the truncated ones
}

TEST(HistoryTopicsReset, ResetHistoryRestartsFromZeroWithoutReplayOrSkip)
{
	TFix f; f.Setup();
	f.pub.subs = 1;
	f.Step(4, 1); f.TickAt(0.0);
	const uint64_t runBefore = f.r.rec->RunId();
	f.r.rec->ResetHistory(12345);
	ASSERT_NE(f.r.rec->RunId(), runBefore);
	f.TickAt(1.0);
	const size_t evBefore = f.pub.Count(kEvTopic), aggBefore = f.pub.Count(kAggTopic);
	f.Step(1, 2); f.TickAt(2.0);
	ASSERT_EQ(f.pub.Count(kAggTopic), aggBefore + 1);
	EXPECT_EQ(f.pub.Of(kAggTopic).back()["fromSample"].asUInt(), 0u);
	const std::vector<Json::Value> ev = AllEvents(f.pub.Of(kEvTopic));
	ASSERT_EQ(ev.size(), 4u + 2u);
	EXPECT_EQ(ev[4]["sample"].asUInt(), 0u);
	EXPECT_EQ(f.pub.Count(kEvTopic), evBefore + 1);
}

TEST(HistoryTopicsReset, ResetThenRegrowingPastTheOldHeadStillDeliversTheNewEventsOnce)
{
	TFix f; f.Setup();
	f.pub.subs = 1;
	f.Step(2, 1); f.TickAt(0.0);
	f.r.rec->ResetHistory(777);
	f.TickAt(1.0);						// the Tick between reset and the next samples sees the new run
	f.Step(5, 1); f.TickAt(2.0);
	const std::vector<Json::Value> ev = AllEvents(f.pub.Of(kEvTopic));
	ASSERT_EQ(ev.size(), 2u + 5u);
	for (size_t i = 0; i < 5; ++i) { EXPECT_EQ(ev[2 + i]["sample"].asUInt(), uint32_t(i)); }
}

// ---- publisher rejection ---------------------------------------------------------------------------------------------------

TEST(HistoryTopicsDrops, RejectedMessagesAreCountedAndWarnedOncePerSecond)
{
	TFix f; f.Setup();
	TopicObservation obs;
	f.topics->SetObservation(&obs);
	LogCapture log("HIS");
	f.pub.subs = 1;
	const uint64_t drops0 = C("dia.cellhistory.topic_drops");
	f.pub.rejectNext = 1000;
	f.Step(1, 1); f.TickAt(0.0);		// aggregates + events both rejected
	f.Step(1, 1); f.TickAt(0.4);		// events rejected again (aggregates throttled)
	f.Step(1, 1); f.TickAt(0.8);
	EXPECT_EQ(f.topics->Drops(), 4u);
	EXPECT_EQ(C("dia.cellhistory.topic_drops") - drops0, 4u);
	EXPECT_EQ(log.Warnings("rejected"), 1u);	// one Warning inside the second
	f.Step(1, 1); f.TickAt(1.6);
	EXPECT_EQ(log.Warnings("rejected"), 2u);
	// The cursors advanced anyway: once the transport recovers only new data is sent.
	f.pub.rejectNext = 0;
	f.pub.msgs.clear();
	f.Step(1, 1); f.TickAt(3.0);
	ASSERT_EQ(f.pub.Count(kAggTopic), 1u);
	EXPECT_EQ(f.pub.Of(kAggTopic)[0]["fromSample"].asUInt(), 4u);
	const std::vector<Json::Value> ev = AllEvents(f.pub.Of(kEvTopic));
	ASSERT_EQ(ev.size(), 1u);
	EXPECT_EQ(ev[0]["sample"].asUInt(), 4u);
}

// ---- logs and metrics ------------------------------------------------------------------------------------------------------

TEST(HistoryTopicsObservation, SubscriberTransitionsLogInfoOnce)
{
	TFix f; f.Setup();
	LogCapture log("HIS");
	typedef LogCapture::Level L;
	f.TickAt(0.0);
	EXPECT_EQ(log.Count(L::kInfo, "first subscriber"), 0u);
	f.pub.subs = 2;
	f.TickAt(1.0); f.TickAt(2.0);
	EXPECT_EQ(log.Count(L::kInfo, "first subscriber"), 1u);
	f.pub.subs = 1;
	f.TickAt(3.0);
	EXPECT_EQ(log.Count(L::kInfo, "last subscriber"), 0u);
	f.pub.subs = 0;
	f.TickAt(4.0); f.TickAt(5.0);
	EXPECT_EQ(log.Count(L::kInfo, "last subscriber"), 1u);
	f.pub.subs = 1;
	f.TickAt(6.0);
	EXPECT_EQ(log.Count(L::kInfo, "first subscriber"), 2u);
}

TEST(HistoryTopicsObservation, MetricsCountMessagesAndBytes)
{
	TFix f; f.Setup();
	TopicObservation obs;
	f.topics->SetObservation(&obs);
	f.pub.subs = 1;
	const uint64_t m0 = C("dia.cellhistory.topic_messages"), b0 = C("dia.cellhistory.topic_bytes"), d0 = C("dia.cellhistory.topic_drops");
	f.Step(2, 3); f.TickAt(0.0);
	f.Step(1, 1); f.TickAt(1.0);
	uint64_t bytes = 0;
	for (const FakePub::Msg& m : f.pub.msgs) { bytes += Size(m.payload); }
	EXPECT_EQ(f.pub.msgs.size(), 4u);
	EXPECT_EQ(C("dia.cellhistory.topic_messages") - m0, 4u);
	EXPECT_EQ(C("dia.cellhistory.topic_bytes") - b0, bytes);
	EXPECT_EQ(C("dia.cellhistory.topic_drops") - d0, 0u);
	EXPECT_EQ(f.topics->AggregateMessages(), 2u);
	EXPECT_EQ(f.topics->EventMessages(), 2u);
}

// ---- client reconstruction (AC 12) -----------------------------------------------------------------------------------------

TEST(HistoryTopicsClient, QueryCatchUpPlusTopicsReproduceTheFile)
{
	TFix f; f.Setup();
	TempFolder tmp;
	f.Step(9, 2);						// history before the client connects
	f.TickAt(0.0);						// no subscriber yet: cursors at the head (sample 9)
	HistoryQueries q(*f.r.rec);

	// Client connects: meta first, then a full aggregates and events catch-up by query, then it subscribes.
	const Json::Value meta = q.Meta(Json::Value(Json::objectValue));
	ASSERT_FALSE(meta.isMember("error"));
	char hex[24];
	snprintf(hex, sizeof(hex), "%016llx", (unsigned long long)f.r.rec->StructureHash());
	EXPECT_EQ(meta["structure"].asString(), hex);
	const uint32_t head = meta["header"]["sampleCount"].asUInt();
	ASSERT_EQ(head, 9u);

	Series client;
	std::vector<Json::Value> clientEvents;
	{
		Json::Value args(Json::objectValue);
		args["from"] = 0; args["to"] = head;
		for (int guard = 0; guard < 1000; ++guard)
		{
			const Json::Value rep = q.Aggregates(args);
			ASSERT_FALSE(rep.isMember("error")) << rep["error"].asString();
			client.Add(rep);
			if (!rep.isMember("next")) { break; }
			args["from"] = rep["next"]["from"];
		}
		Json::Value eargs(Json::objectValue);
		eargs["from"] = 0; eargs["to"] = head;
		for (int guard = 0; guard < 1000; ++guard)
		{
			const Json::Value rep = q.Events(eargs);
			ASSERT_FALSE(rep.isMember("error")) << rep["error"].asString();
			for (const Json::Value& e : rep["events"]) { clientEvents.push_back(e); }
			if (!rep.isMember("next")) { break; }
			eargs["from"] = rep["next"]["from"];
			eargs["skip"] = rep["next"]["skip"];
		}
	}
	ASSERT_EQ(client.samples, head);

	f.pub.subs = 1;					// ... and follows the topics
	double t = 1.0;
	for (int round = 0; round < 5; ++round) { f.Step(3, 2); f.TickAt(t += 1.0); f.TickAt(t += 0.1); }
	for (const Json::Value& m : f.pub.Of(kAggTopic))
	{
		EXPECT_EQ(m["fromSample"].asUInt(), client.samples);	// contiguous with the catch-up and with each other
		client.Add(m);
	}
	for (const Json::Value& e : AllEvents(f.pub.Of(kEvTopic))) { clientEvents.push_back(e); }

	ExportOptions opt; opt.overwrite = true;
	ExportResult res;
	const std::string path = tmp.File("run.cell_history.json");
	ASSERT_TRUE(HistoryExporter::WriteFile(*f.r.rec, opt, path.c_str(), res));
	DecodedExport dec;
	ASSERT_TRUE(dec.Load(path.c_str()));
	client.ExpectEqualsFile(dec);
	ASSERT_EQ(clientEvents.size(), size_t(dec.root["events"].size()));
	for (size_t i = 0; i < clientEvents.size(); ++i) { EXPECT_TRUE(JsonEq(clientEvents[i], dec.root["events"][Json::ArrayIndex(i)])) << i; }
}

#endif // DIA_DEBUG
