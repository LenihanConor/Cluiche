////////////////////////////////////////////////////////////////////////////////
// Filename: TestMetricInfoEdge.cpp
// Description: Edge / property / stress tests for MetricInfo and the MetricRegistry
//              info overloads, GetInfo, ForEachMetric, Generation, ConflictWarningCount.
// Spec: docs/specs/applications/dia/systems/diavalueschema/code-side-descriptions.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/MetricInfo.h>
#include <DiaObservation/Metric/Testing/MetricFixture.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <memory>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

using namespace Dia::Observation::Metric;
using Dia::Core::StringCRC;

static_assert(sizeof(MetricInfo) == 3 * sizeof(void*), "MetricInfo must stay three pointers");
static_assert(std::is_trivially_copyable<MetricInfo>::value, "MetricInfo must stay trivially copyable");

namespace
{
	const float kBounds[] = { 1.0f, 2.0f, 4.0f };

	struct Visit
	{
		StringCRC name;
		MetricKind kind;
		const MetricInfo* info;
		MetricInfo infoCopy;
	};

	struct Collector
	{
		std::vector<Visit> visits;
		void* lastUser = nullptr;
	};

	void CollectAll(const StringCRC& name, MetricKind kind, const MetricInfo* info, void* user)
	{
		Collector* c = static_cast<Collector*>(user);
		Visit v;
		v.name = name;
		v.kind = kind;
		v.info = info;
		v.infoCopy = info ? *info : MetricInfo();
		c->visits.push_back(v);
	}

	size_t CountVisits(const MetricRegistry& r)
	{
		Collector c;
		r.ForEachMetric(&CollectAll, &c);
		return c.visits.size();
	}

	std::string Name(const char* prefix, int a, int b = -1)
	{
		char buf[64];
		if (b < 0) snprintf(buf, sizeof(buf), "%s.%d", prefix, a);
		else       snprintf(buf, sizeof(buf), "%s.%d.%d", prefix, a, b);
		return buf;
	}

	// Registers by kind index (0 counter, 1 gauge, 2 histogram).
	void RegisterByKind(MetricRegistry& r, int kind, const StringCRC& n, const MetricInfo& info)
	{
		switch (kind)
		{
		case 0: r.RegisterCounter(n, info); break;
		case 1: r.RegisterGauge(n, info); break;
		default: r.RegisterHistogram(n, kBounds, 3, info); break;
		}
	}

	void RegisterByKindNoInfo(MetricRegistry& r, int kind, const StringCRC& n)
	{
		switch (kind)
		{
		case 0: r.RegisterCounter(n); break;
		case 1: r.RegisterGauge(n); break;
		default: r.RegisterHistogram(n, kBounds, 3); break;
		}
	}
}

class MetricInfoEdge : public ::testing::Test
{
protected:
	MetricFixture mFixture;   // Reset() on construction and destruction
	MetricRegistry& R() { return MetricRegistry::Instance(); }
};

////////////////////////////////////////////////////////////////////////////////
// MetricInfo value type
////////////////////////////////////////////////////////////////////////////////
TEST_F(MetricInfoEdge, DefaultInfoIsAllNull)
{
	MetricInfo i;
	EXPECT_EQ(nullptr, i.label);
	EXPECT_EQ(nullptr, i.description);
	EXPECT_EQ(nullptr, i.unit);
}

TEST_F(MetricInfoEdge, InfoStringsAreStoredByPointerNotCopied)
{
	static const char kLabel[] = "ptr-label";
	static const char kDesc[] = "ptr-desc";
	static const char kUnit[] = "ptr-unit";
	R().RegisterCounter(StringCRC("mie.ptr"), MetricInfo{ kLabel, kDesc, kUnit });
	const MetricInfo* got = R().GetInfo(StringCRC("mie.ptr"));
	ASSERT_NE(nullptr, got);
	EXPECT_EQ(static_cast<const char*>(kLabel), got->label);
	EXPECT_EQ(static_cast<const char*>(kDesc), got->description);
	EXPECT_EQ(static_cast<const char*>(kUnit), got->unit);
}

TEST_F(MetricInfoEdge, LongStringsAreSupported)
{
	const std::string label(2048, 'L'), desc(4096, 'D'), unit(1024, 'U');
	R().RegisterGauge(StringCRC("mie.long"), MetricInfo{ label.c_str(), desc.c_str(), unit.c_str() });
	const MetricInfo* got = R().GetInfo(StringCRC("mie.long"));
	ASSERT_NE(nullptr, got);
	EXPECT_EQ(label, got->label);
	EXPECT_EQ(desc, got->description);
	EXPECT_EQ(unit, got->unit);

	// Equal long strings in other buffers do not conflict.
	const std::string label2(label), desc2(desc), unit2(unit);
	const unsigned w = R().ConflictWarningCount();
	R().RegisterGauge(StringCRC("mie.long"), MetricInfo{ label2.c_str(), desc2.c_str(), unit2.c_str() });
	EXPECT_EQ(w, R().ConflictWarningCount());
}

////////////////////////////////////////////////////////////////////////////////
// Info comparison rules
////////////////////////////////////////////////////////////////////////////////
// PIN: empty string and null are different values for the conflict check (SameStr).
TEST_F(MetricInfoEdge, PinEmptyStringVersusNullConflicts)
{
	const unsigned w = R().ConflictWarningCount();
	R().RegisterCounter(StringCRC("mie.nullvsempty"), MetricInfo{ "", "d", "u" });
	R().RegisterCounter(StringCRC("mie.nullvsempty"), MetricInfo{ nullptr, "d", "u" });
	EXPECT_EQ(w + 1, R().ConflictWarningCount());
	ASSERT_NE(nullptr, R().GetInfo(StringCRC("mie.nullvsempty")));
	EXPECT_STREQ("", R().GetInfo(StringCRC("mie.nullvsempty"))->label);
}

TEST_F(MetricInfoEdge, NullVersusNullIsTheSame)
{
	const unsigned w = R().ConflictWarningCount();
	R().RegisterCounter(StringCRC("mie.nullnull"), MetricInfo{ nullptr, "d", nullptr });
	R().RegisterCounter(StringCRC("mie.nullnull"), MetricInfo{ nullptr, "d", nullptr });
	EXPECT_EQ(w, R().ConflictWarningCount());
}

// PIN: an all-null MetricInfo passed to an info overload IS stored as the metric's info
// (GetInfo returns non-null with null fields, Generation bumps for the info), and a later
// real info then counts as a conflict and is ignored. The header says "first non-empty
// info wins" but an empty info is not treated as "no info".
TEST_F(MetricInfoEdge, PinAllNullInfoIsStoredAndBlocksLaterInfo)
{
	const StringCRC n("mie.allnull");
	R().RegisterCounter(n, MetricInfo());
	const MetricInfo* got = R().GetInfo(n);
	ASSERT_NE(nullptr, got);
	EXPECT_EQ(nullptr, got->label);

	const unsigned w = R().ConflictWarningCount();
	R().RegisterCounter(n, MetricInfo{ "Real", "d", "u" });
	EXPECT_EQ(w + 1, R().ConflictWarningCount());
	EXPECT_EQ(nullptr, R().GetInfo(n)->label);
}

TEST_F(MetricInfoEdge, SameTextDifferentPointerPerFieldIsNotAConflict)
{
	for (int field = 0; field < 3; field++)
	{
		const std::string n = Name("mie.sametext", field);
		char a1[] = "text", a2[] = "text";
		char b1[] = "other", b2[] = "other";
		char c1[] = "more", c2[] = "more";
		MetricInfo first{ field == 0 ? a1 : "L", field == 1 ? b1 : "D", field == 2 ? c1 : "U" };
		MetricInfo second{ field == 0 ? a2 : "L", field == 1 ? b2 : "D", field == 2 ? c2 : "U" };
		R().RegisterCounter(StringCRC(n.c_str()), first);
		const unsigned w = R().ConflictWarningCount();
		const unsigned g = R().Generation();
		R().RegisterCounter(StringCRC(n.c_str()), second);
		EXPECT_EQ(w, R().ConflictWarningCount()) << "field " << field;
		EXPECT_EQ(g, R().Generation()) << "field " << field;
	}
}

TEST_F(MetricInfoEdge, ConflictInSingleFieldWarnsAndKeepsFirst)
{
	for (int field = 0; field < 3; field++)
	{
		const std::string n = Name("mie.onefield", field);
		const StringCRC crc(n.c_str());
		MetricInfo first{ "L", "D", "U" };
		MetricInfo second = first;
		if (field == 0) second.label = "L2";
		if (field == 1) second.description = "D2";
		if (field == 2) second.unit = "U2";

		R().RegisterCounter(crc, first);
		const unsigned w = R().ConflictWarningCount();
		const unsigned g = R().Generation();
		R().RegisterCounter(crc, second);
		EXPECT_EQ(w + 1, R().ConflictWarningCount()) << "field " << field;
		EXPECT_EQ(g, R().Generation()) << "field " << field;
		const MetricInfo* got = R().GetInfo(crc);
		ASSERT_NE(nullptr, got);
		EXPECT_STREQ("L", got->label);
		EXPECT_STREQ("D", got->description);
		EXPECT_STREQ("U", got->unit);
	}
}

TEST_F(MetricInfoEdge, ThirdRegistrationAfterConflictKeepsCountAtOne)
{
	const StringCRC n("mie.third");
	const unsigned w = R().ConflictWarningCount();
	R().RegisterCounter(n, MetricInfo{ "A", "d", "u" });
	R().RegisterCounter(n, MetricInfo{ "B", "d", "u" });   // conflict -> warn
	R().RegisterCounter(n, MetricInfo{ "C", "d", "u" });   // different conflict, silent
	R().RegisterCounter(n, MetricInfo{ "B", "d", "u" });   // repeat of the loser, silent
	EXPECT_EQ(w + 1, R().ConflictWarningCount());
	EXPECT_STREQ("A", R().GetInfo(n)->label);
}

// PIN: once warned, the per-name flag stays set: matching the first info again, or a
// no-info registration, does not clear it and no new warning is ever issued for the name.
TEST_F(MetricInfoEdge, PinWarnedFlagIsPerNameAndNeverClears)
{
	const StringCRC n("mie.flag");
	const unsigned w = R().ConflictWarningCount();
	R().RegisterGauge(n, MetricInfo{ "A", "d", "u" });
	R().RegisterGauge(n, MetricInfo{ "B", "d", "u" });
	R().RegisterGauge(n, MetricInfo{ "A", "d", "u" });
	R().RegisterGauge(n);
	R().RegisterGauge(n, MetricInfo{ "Z", "d", "u" });
	EXPECT_EQ(w + 1, R().ConflictWarningCount());

	// A different name warns independently.
	const StringCRC other("mie.flag.other");
	R().RegisterGauge(other, MetricInfo{ "A", "d", "u" });
	R().RegisterGauge(other, MetricInfo{ "B", "d", "u" });
	EXPECT_EQ(w + 2, R().ConflictWarningCount());
}

TEST_F(MetricInfoEdge, NoInfoRegistrationNeverWarnsOrStores)
{
	const unsigned w = R().ConflictWarningCount();
	for (int kind = 0; kind < 3; kind++)
	{
		const std::string n = Name("mie.noinfo", kind);
		RegisterByKindNoInfo(R(), kind, StringCRC(n.c_str()));
		RegisterByKindNoInfo(R(), kind, StringCRC(n.c_str()));
		EXPECT_EQ(nullptr, R().GetInfo(StringCRC(n.c_str())));
	}
	EXPECT_EQ(w, R().ConflictWarningCount());
}

////////////////////////////////////////////////////////////////////////////////
// Kind parity
////////////////////////////////////////////////////////////////////////////////
TEST_F(MetricInfoEdge, CounterGaugeHistogramParityForEveryInfoPath)
{
	for (int kind = 0; kind < 3; kind++)
	{
		const std::string base = Name("mie.parity", kind);
		const StringCRC fresh((base + ".fresh").c_str());
		const StringCRC late((base + ".late").c_str());
		const StringCRC conflict((base + ".conflict").c_str());
		const StringCRC same((base + ".same").c_str());

		// New metric with info: stored, generation +2 (creation + info).
		unsigned g = R().Generation();
		RegisterByKind(R(), kind, fresh, MetricInfo{ "L", "D", "U" });
		EXPECT_EQ(g + 2, R().Generation()) << kind;
		ASSERT_NE(nullptr, R().GetInfo(fresh));
		EXPECT_STREQ("L", R().GetInfo(fresh)->label);

		// Info after no-info: stored, generation +1.
		RegisterByKindNoInfo(R(), kind, late);
		g = R().Generation();
		RegisterByKind(R(), kind, late, MetricInfo{ "Late", "D", "U" });
		EXPECT_EQ(g + 1, R().Generation()) << kind;
		EXPECT_STREQ("Late", R().GetInfo(late)->label);

		// Conflict: first wins, one warning, generation unchanged.
		RegisterByKind(R(), kind, conflict, MetricInfo{ "First", "D", "U" });
		g = R().Generation();
		const unsigned w = R().ConflictWarningCount();
		RegisterByKind(R(), kind, conflict, MetricInfo{ "Second", "D", "U" });
		EXPECT_EQ(g, R().Generation()) << kind;
		EXPECT_EQ(w + 1, R().ConflictWarningCount()) << kind;
		EXPECT_STREQ("First", R().GetInfo(conflict)->label);

		// Same info again: no change.
		RegisterByKind(R(), kind, same, MetricInfo{ "S", "D", "U" });
		g = R().Generation();
		RegisterByKind(R(), kind, same, MetricInfo{ "S", "D", "U" });
		EXPECT_EQ(g, R().Generation()) << kind;
	}
}

TEST_F(MetricInfoEdge, InfoOverloadsReturnTheSamePrimitiveAsPlainOverloads)
{
	Counter* c = R().RegisterCounter(StringCRC("mie.same.c"), MetricInfo{ "L", "D", "U" });
	Gauge* g = R().RegisterGauge(StringCRC("mie.same.g"), MetricInfo{ "L", "D", "U" });
	Histogram* h = R().RegisterHistogram(StringCRC("mie.same.h"), kBounds, 3, MetricInfo{ "L", "D", "U" });
	EXPECT_EQ(c, R().RegisterCounter(StringCRC("mie.same.c")));
	EXPECT_EQ(g, R().RegisterGauge(StringCRC("mie.same.g")));
	EXPECT_EQ(h, R().RegisterHistogram(StringCRC("mie.same.h"), kBounds, 3));
	EXPECT_EQ(c, R().RegisterCounter(StringCRC("mie.same.c"), MetricInfo{ "Other", "D", "U" })); // conflict still returns it
	EXPECT_EQ(c, R().FindCounter(StringCRC("mie.same.c")));
}

////////////////////////////////////////////////////////////////////////////////
// Generation
////////////////////////////////////////////////////////////////////////////////
// PIN: a brand new metric registered WITH info takes two lock scopes (creation, then
// info) and so bumps Generation by exactly 2. The documented contract is only "at least one".
TEST_F(MetricInfoEdge, PinNewMetricWithInfoBumpsGenerationTwice)
{
	const unsigned g0 = R().Generation();
	R().RegisterCounter(StringCRC("mie.gen2"), MetricInfo{ "L", "D", "U" });
	const unsigned g1 = R().Generation();
	EXPECT_GE(g1, g0 + 1);   // contract
	EXPECT_EQ(g0 + 2, g1);   // current behaviour

	R().RegisterCounter(StringCRC("mie.gen1"));
	EXPECT_EQ(g1 + 1, R().Generation());
}

TEST_F(MetricInfoEdge, GenerationUnchangedByReadOperations)
{
	R().RegisterCounter(StringCRC("mie.readonly"), MetricInfo{ "L", "D", "U" });
	const unsigned g = R().Generation();
	R().GetInfo(StringCRC("mie.readonly"));
	R().GetInfo(StringCRC("mie.readonly.missing"));
	R().FindCounter(StringCRC("mie.readonly"));
	CountVisits(R());
	std::unique_ptr<MetricSnapshot> snap(new MetricSnapshot());
	R().Snapshot(*snap);
	EXPECT_EQ(g, R().Generation());
}

TEST_F(MetricInfoEdge, GenerationIsMonotonicOverRegistrations)
{
	unsigned last = R().Generation();
	for (int i = 0; i < 200; i++)
	{
		const std::string n = Name("mie.mono", i % 50);
		const MetricInfo info{ (i % 3 == 0) ? "A" : "B", "D", "U" };
		RegisterByKind(R(), (i % 50) % 3, StringCRC(n.c_str()), info);
		const unsigned now = R().Generation();
		ASSERT_GE(now, last);
		last = now;
	}
}

////////////////////////////////////////////////////////////////////////////////
// Reset
////////////////////////////////////////////////////////////////////////////////
// Info pointers are invalidated by Reset(); they must not be dereferenced afterwards. This
// test only observes that the lookups report nothing after Reset.
TEST_F(MetricInfoEdge, ResetClearsInfosAndMetricsAndBumpsGeneration)
{
	R().RegisterCounter(StringCRC("mie.reset.c"), MetricInfo{ "L", "D", "U" });
	R().RegisterGauge(StringCRC("mie.reset.g"), MetricInfo{ "L", "D", "U" });
	R().RegisterHistogram(StringCRC("mie.reset.h"), kBounds, 3, MetricInfo{ "L", "D", "U" });
	ASSERT_NE(nullptr, R().GetInfo(StringCRC("mie.reset.c")));
	ASSERT_EQ(3u, CountVisits(R()));

	const unsigned g = R().Generation();
	R().Reset();
	EXPECT_GT(R().Generation(), g);
	EXPECT_EQ(nullptr, R().GetInfo(StringCRC("mie.reset.c")));
	EXPECT_EQ(nullptr, R().GetInfo(StringCRC("mie.reset.g")));
	EXPECT_EQ(nullptr, R().GetInfo(StringCRC("mie.reset.h")));
	EXPECT_EQ(nullptr, R().FindCounter(StringCRC("mie.reset.c")));
	EXPECT_EQ(0u, CountVisits(R()));
}

TEST_F(MetricInfoEdge, ResetThenReRegisterWithDifferentInfoIsFresh)
{
	const StringCRC n("mie.reset.fresh");
	R().RegisterCounter(n, MetricInfo{ "Old", "D", "U" });
	R().Reset();

	const unsigned w = R().ConflictWarningCount();
	R().RegisterCounter(n, MetricInfo{ "New", "D2", "U2" });
	EXPECT_EQ(w, R().ConflictWarningCount());   // no conflict with the pre-Reset info
	ASSERT_NE(nullptr, R().GetInfo(n));
	EXPECT_STREQ("New", R().GetInfo(n)->label);
	EXPECT_STREQ("D2", R().GetInfo(n)->description);

	// The warned flag was also cleared: a conflict warns again.
	R().RegisterCounter(n, MetricInfo{ "Third", "D", "U" });
	EXPECT_EQ(w + 1, R().ConflictWarningCount());
}

// PIN: ConflictWarningCount is a process-lifetime diagnostic and is never reset by Reset().
TEST_F(MetricInfoEdge, PinConflictWarningCountSurvivesReset)
{
	const StringCRC n("mie.reset.count");
	R().RegisterCounter(n, MetricInfo{ "A", "d", "u" });
	R().RegisterCounter(n, MetricInfo{ "B", "d", "u" });
	const unsigned w = R().ConflictWarningCount();
	EXPECT_GE(w, 1u);
	R().Reset();
	EXPECT_EQ(w, R().ConflictWarningCount());
}

TEST_F(MetricInfoEdge, ResetOnEmptyRegistryIsSafeAndBumps)
{
	R().Reset();
	const unsigned g = R().Generation();
	R().Reset();
	EXPECT_GT(R().Generation(), g);
	EXPECT_EQ(0u, CountVisits(R()));
}

////////////////////////////////////////////////////////////////////////////////
// ForEachMetric
////////////////////////////////////////////////////////////////////////////////
TEST_F(MetricInfoEdge, ForEachOrderIsCountersGaugesHistogramsEachInRegistrationOrder)
{
	R().RegisterGauge(StringCRC("mie.order.g1"));
	R().RegisterCounter(StringCRC("mie.order.c1"));
	R().RegisterHistogram(StringCRC("mie.order.h1"), kBounds, 3);
	R().RegisterCounter(StringCRC("mie.order.c2"), MetricInfo{ "C2", "d", "u" });
	R().RegisterGauge(StringCRC("mie.order.g2"), MetricInfo{ "G2", "d", "u" });
	R().RegisterHistogram(StringCRC("mie.order.h2"), kBounds, 3, MetricInfo{ "H2", "d", "u" });

	Collector c;
	R().ForEachMetric(&CollectAll, &c);
	ASSERT_EQ(6u, c.visits.size());
	const char* expectedNames[] = { "mie.order.c1", "mie.order.c2", "mie.order.g1", "mie.order.g2", "mie.order.h1", "mie.order.h2" };
	const MetricKind expectedKinds[] = { MetricKind::Counter, MetricKind::Counter, MetricKind::Gauge,
	                                     MetricKind::Gauge, MetricKind::Histogram, MetricKind::Histogram };
	for (int i = 0; i < 6; i++)
	{
		EXPECT_TRUE(c.visits[i].name == StringCRC(expectedNames[i])) << i;
		EXPECT_EQ(expectedKinds[i], c.visits[i].kind) << i;
	}
	EXPECT_EQ(nullptr, c.visits[0].info);
	ASSERT_NE(nullptr, c.visits[1].info);
	EXPECT_STREQ("C2", c.visits[1].infoCopy.label);
	EXPECT_EQ(nullptr, c.visits[2].info);
	EXPECT_STREQ("G2", c.visits[3].infoCopy.label);
	EXPECT_STREQ("H2", c.visits[5].infoCopy.label);
}

TEST_F(MetricInfoEdge, ForEachOnEmptyRegistryNeverCallsVisitor)
{
	Collector c;
	R().ForEachMetric(&CollectAll, &c);
	EXPECT_TRUE(c.visits.empty());
}

TEST_F(MetricInfoEdge, ForEachWithNullVisitorDoesNotCrash)
{
	R().RegisterCounter(StringCRC("mie.nullvisitor"));
	R().RegisterGauge(StringCRC("mie.nullvisitor.g"), MetricInfo{ "L", "d", "u" });
	R().ForEachMetric(nullptr, nullptr);
	int dummy = 0;
	R().ForEachMetric(nullptr, &dummy);
	SUCCEED();
}

namespace
{
	void* gSeenUser = reinterpret_cast<void*>(1);
	int gSeenCalls = 0;
	void RecordUser(const StringCRC&, MetricKind, const MetricInfo*, void* user)
	{
		gSeenUser = user;
		gSeenCalls++;
	}
}

TEST_F(MetricInfoEdge, NullUserPointerIsPassedThrough)
{
	R().RegisterCounter(StringCRC("mie.nulluser"));
	gSeenUser = reinterpret_cast<void*>(1);
	gSeenCalls = 0;
	R().ForEachMetric(&RecordUser, nullptr);
	EXPECT_EQ(1, gSeenCalls);
	EXPECT_EQ(nullptr, gSeenUser);

	int token = 0;
	R().ForEachMetric(&RecordUser, &token);
	EXPECT_EQ(&token, gSeenUser);
}

TEST_F(MetricInfoEdge, InfoPointerInVisitorMatchesGetInfo)
{
	R().RegisterHistogram(StringCRC("mie.visitptr"), kBounds, 3, MetricInfo{ "L", "d", "u" });
	Collector c;
	R().ForEachMetric(&CollectAll, &c);
	ASSERT_EQ(1u, c.visits.size());
	EXPECT_EQ(R().GetInfo(StringCRC("mie.visitptr")), c.visits[0].info);
}

namespace
{
	struct ReentrantCtx
	{
		MetricRegistry* reg;
		int visited = 0;
		bool registered = false;
		bool getInfoOk = false;
		unsigned genBefore = 0;
		unsigned genAfter = 0;
		std::vector<std::string> seen;
	};

	void ReentrantVisitor(const StringCRC& name, MetricKind, const MetricInfo*, void* user)
	{
		ReentrantCtx* c = static_cast<ReentrantCtx*>(user);
		c->visited++;
		c->seen.push_back(name.AsChar());
		const MetricInfo* i = c->reg->GetInfo(name);
		if (i && i->label && std::strcmp(i->label, "Seed") == 0) c->getInfoOk = true;
		if (!c->registered)
		{
			c->registered = true;
			c->genBefore = c->reg->Generation();
			c->reg->RegisterGauge(StringCRC("mie.reentrant.added"), MetricInfo{ "Added", "d", "u" });
			c->genAfter = c->reg->Generation();
		}
	}
}

TEST_F(MetricInfoEdge, VisitorMayRegisterMetricsAndCallBackIntoRegistry)
{
	R().RegisterCounter(StringCRC("mie.reentrant.seed"), MetricInfo{ "Seed", "d", "u" });

	ReentrantCtx ctx;
	ctx.reg = &R();
	R().ForEachMetric(&ReentrantVisitor, &ctx);   // would deadlock if the lock were held

	EXPECT_EQ(1, ctx.visited);                    // the new metric is not visited this pass
	EXPECT_TRUE(ctx.getInfoOk);
	EXPECT_GT(ctx.genAfter, ctx.genBefore);

	ReentrantCtx second;
	second.reg = &R();
	second.registered = true;                     // do not register again
	R().ForEachMetric(&ReentrantVisitor, &second);
	EXPECT_EQ(2, second.visited);                 // now both are visited
	ASSERT_NE(nullptr, R().GetInfo(StringCRC("mie.reentrant.added")));
}

namespace
{
	void ResetFromVisitor(const StringCRC&, MetricKind, const MetricInfo*, void* user)
	{
		static_cast<MetricRegistry*>(user)->Reset();
	}
}

// The visitor runs outside the lock, so even Reset() from inside it must not deadlock.
TEST_F(MetricInfoEdge, VisitorMayResetRegistryWithoutDeadlock)
{
	R().RegisterCounter(StringCRC("mie.visitreset.a"));
	R().RegisterGauge(StringCRC("mie.visitreset.b"));
	R().ForEachMetric(&ResetFromVisitor, &R());
	EXPECT_EQ(0u, CountVisits(R()));
}

////////////////////////////////////////////////////////////////////////////////
// Pointer stability
////////////////////////////////////////////////////////////////////////////////
TEST_F(MetricInfoEdge, GetInfoPointerStableAcrossManyLaterRegistrations)
{
	static std::vector<std::string> labels;
	labels.clear();
	labels.reserve(1100);

	R().RegisterCounter(StringCRC("mie.stable.first"), MetricInfo{ "First", "d", "u" });
	const MetricInfo* first = R().GetInfo(StringCRC("mie.stable.first"));
	ASSERT_NE(nullptr, first);

	for (int i = 0; i < 1000; i++)
	{
		labels.push_back(Name("label", i));
		const std::string n = Name("mie.stable.n", i);
		RegisterByKind(R(), i % 3, StringCRC(n.c_str()), MetricInfo{ labels.back().c_str(), "d", "u" });
	}
	EXPECT_EQ(first, R().GetInfo(StringCRC("mie.stable.first")));
	EXPECT_STREQ("First", first->label);
	EXPECT_STREQ("label.0", R().GetInfo(StringCRC("mie.stable.n.0"))->label);
	EXPECT_STREQ("label.999", R().GetInfo(StringCRC("mie.stable.n.999"))->label);
	EXPECT_EQ(1001u, CountVisits(R()));
}

////////////////////////////////////////////////////////////////////////////////
// Threads
////////////////////////////////////////////////////////////////////////////////
TEST_F(MetricInfoEdge, StressConcurrentRegistrationWithReader)
{
	const int kThreads = 4, kPer = 250;
	static std::vector<std::string> labels;
	labels.clear();
	labels.reserve(kThreads * kPer);
	for (int t = 0; t < kThreads; t++)
		for (int i = 0; i < kPer; i++)
			labels.push_back(Name("lbl", t, i));

	std::atomic<bool> stop{ false };
	std::atomic<int> readerErrors{ 0 };
	std::thread reader([&]()
	{
		unsigned last = 0;
		while (!stop.load())
		{
			Collector c;
			R().ForEachMetric(&CollectAll, &c);
			for (const Visit& v : c.visits)
			{
				const MetricInfo* i = R().GetInfo(v.name);
				// A metric may be created a moment before its info is stored.
				if (i && (!i->label || std::strncmp(i->label, "lbl.", 4) != 0)) readerErrors++;
			}
			const unsigned g = R().Generation();
			if (g < last) readerErrors++;
			last = g;
		}
	});

	std::vector<std::thread> writers;
	for (int t = 0; t < kThreads; t++)
	{
		writers.emplace_back([t, kPer]()
		{
			MetricRegistry& r = MetricRegistry::Instance();
			for (int i = 0; i < kPer; i++)
			{
				const std::string n = Name("mie.mt", t, i);
				RegisterByKind(r, i % 3, StringCRC(n.c_str()), MetricInfo{ labels[t * kPer + i].c_str(), "d", "u" });
			}
		});
	}
	for (auto& w : writers) w.join();
	stop = true;
	reader.join();

	EXPECT_EQ(0, readerErrors.load());
	EXPECT_EQ((size_t)(kThreads * kPer), CountVisits(R()));
	for (int t = 0; t < kThreads; t++)
		for (int i = 0; i < kPer; i++)
		{
			const std::string n = Name("mie.mt", t, i);
			const MetricInfo* info = R().GetInfo(StringCRC(n.c_str()));
			ASSERT_NE(nullptr, info) << n;
			EXPECT_EQ(labels[t * kPer + i], info->label) << n;
		}
}

TEST_F(MetricInfoEdge, StressConcurrentSameNameDifferentInfoWarnsOnceAndFirstWins)
{
	const int kThreads = 8;
	static const char* const kLabels[kThreads] = { "T0", "T1", "T2", "T3", "T4", "T5", "T6", "T7" };
	const StringCRC n("mie.race");
	const unsigned w = R().ConflictWarningCount();

	std::atomic<bool> go{ false };
	std::vector<std::thread> threads;
	for (int t = 0; t < kThreads; t++)
	{
		threads.emplace_back([t, &go, n]()
		{
			while (!go.load()) std::this_thread::yield();
			MetricRegistry::Instance().RegisterCounter(n, MetricInfo{ kLabels[t], "d", "u" });
		});
	}
	go = true;
	for (auto& th : threads) th.join();

	EXPECT_EQ(w + 1, R().ConflictWarningCount());
	const MetricInfo* got = R().GetInfo(n);
	ASSERT_NE(nullptr, got);
	bool isCandidate = false;
	for (const char* l : kLabels) isCandidate |= (std::strcmp(l, got->label) == 0);
	EXPECT_TRUE(isCandidate);
	EXPECT_EQ(1u, CountVisits(R()));
}

TEST_F(MetricInfoEdge, StressConcurrentSameNameSameInfoNeverWarns)
{
	const StringCRC n("mie.race.same");
	const unsigned w = R().ConflictWarningCount();
	std::vector<std::thread> threads;
	for (int t = 0; t < 8; t++)
	{
		threads.emplace_back([n]()
		{
			for (int i = 0; i < 100; i++)
				MetricRegistry::Instance().RegisterGauge(n, MetricInfo{ "Same", "d", "u" });
		});
	}
	for (auto& th : threads) th.join();
	EXPECT_EQ(w, R().ConflictWarningCount());
	EXPECT_EQ(1u, CountVisits(R()));
	EXPECT_STREQ("Same", R().GetInfo(n)->label);
}
