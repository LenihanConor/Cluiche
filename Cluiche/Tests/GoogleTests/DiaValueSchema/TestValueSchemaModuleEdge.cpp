#include <gtest/gtest.h>
#include "ValueSchemaTestSupport.h"
#include <DiaValueSchema/ValueSchemaModule.h>
#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaValueSchema/IValueSchemaContributor.h>
#include <DiaValueSchema/Testing/FakeCodeSideSource.h>
#include <DiaValueSchema/Testing/SchemaFixtureBuilder.h>
#include <DiaValueSchema/Testing/SchemaTempDirectory.h>
#include <DiaValueSchema/Testing/ValueInfoBuilder.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/FilePath/PathStore.h>
#include <DiaObservation/Health/HealthRegistry.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/Gauge.h>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// Exhaustive edge tests for ValueSchemaModule: config forms, alias resolution, contributors, lifecycle (Start/Stop
// cycles, double Start, Stop without Start), gauges, late-registration accounting. PIN tests document current
// behaviour on purpose.

using namespace Dia::ApplicationFlow;
using namespace Dia::ValueSchema;
using Dia::Core::StringCRC;
using Dia::Core::PathStore;
using Dia::ValueSchema::Testing::FakeCodeSideSource;
using Dia::ValueSchema::Testing::SchemaFixtureBuilder;
using Dia::ValueSchema::Testing::SchemaTempDirectory;
using Dia::ValueSchema::Testing::MakeValueInfo;
namespace fs = std::filesystem;

namespace
{
	constexpr float kDt = 1.0f / 60.0f;

	struct EdgeConsumer : Module
	{
		static const StringCRC kTypeId;
		explicit EdgeConsumer(const StringCRC& id) : Module(id) {}
		ServiceStreamReader<ValueSchemaService> mReader{ this, StringCRC(ValueSchemaModule::kServiceStreamId) };
		void OnConnectStreams(Application& app) override { mReader.Connect(app); }
		StartResult DoStart() override { return StartResult::kReady; }
		void DoUpdate(float) override {}
		StopResult DoStop() override { return StopResult::kDone; }
	};
	const StringCRC EdgeConsumer::kTypeId("EdgeConsumer");

	struct TestVsc : ValueSchemaModule
	{
		using ValueSchemaModule::ValueSchemaModule;
		void Configure(const char* json) { OnConfigure(json); }
		void CycleStop() { DoStop(); }
		void CycleStart() { DoStart(); }
		void Tick(float dt) { DoUpdate(dt); }
	};

	TestVsc*							g_module = nullptr;
	EdgeConsumer*						g_consumer = nullptr;
	FakeCodeSideSource*					g_source = nullptr;
	std::vector<IValueSchemaContributor*>	g_contributors;

	Module* CreateVsc(const StringCRC& id)
	{
		auto* m = new TestVsc(id);
		if (g_source) { m->SetCodeSideSource(g_source); }
		for (IValueSchemaContributor* c : g_contributors) { m->AddContributor(c); }
		g_module = m;
		return m;
	}
	Module* CreateConsumer(const StringCRC& id) { auto* m = new EdgeConsumer(id); g_consumer = m; return m; }

	ModuleDeclaration MakeModule(const StringCRC& typeId, const char* instanceId, const char* role)
	{
		ModuleDeclaration mod;
		mod.instanceId = StringCRC(instanceId);
		mod.typeId = typeId;
		mod.startTimeoutMs = 10000.0f;
		mod.stopTimeoutMs = 5000.0f;
		mod.stages.Add(StringCRC("Boot"));
		ChannelBinding ch;
		ch.id = StringCRC(ValueSchemaModule::kServiceStreamId);
		ch.role = StringCRC(role);
		mod.channels.Add(ch);
		return mod;
	}

	struct Harness
	{
		TypeRegistry					mReg;
		ApplicationManifestV3			mManifest;
		std::unique_ptr<Application>	mApp;
		std::string						mConfig;

		Harness(const std::string& configJson, FakeCodeSideSource& source, std::vector<IValueSchemaContributor*> contributors = {})
			: mConfig(configJson)
		{
			g_module = nullptr;
			g_consumer = nullptr;
			g_source = &source;
			g_contributors = std::move(contributors);
			mReg.Register(ValueSchemaModule::kTypeId, CreateVsc);
			mReg.Register(EdgeConsumer::kTypeId, CreateConsumer);
			ApplicationManifestV3& m = mManifest;
			m.version = 3;
			StageDeclaration boot;
			boot.name = StringCRC("Boot");
			m.stages.Add(boot);
			m.initialStage = StringCRC("Boot");
			StreamDeclaration svc;
			svc.id = StringCRC(ValueSchemaModule::kServiceStreamId);
			svc.kind = StringCRC("ServiceStream");
			svc.payloadType = StringCRC("ValueSchemaService");
			m.streams.Add(svc);
			ProcessingUnitDeclaration pu;
			pu.instanceId = StringCRC("MainPU");
			pu.frequencyHz = 60.0f;
			pu.dedicatedThread = false;
			ModuleDeclaration vsc = MakeModule(ValueSchemaModule::kTypeId, "valueSchema", "provides");
			vsc.configJson = mConfig.c_str();
			pu.modules.Add(vsc);
			pu.modules.Add(MakeModule(EdgeConsumer::kTypeId, "consumer", "consumes"));
			m.processingUnits.Add(pu);
			mApp.reset(new Application(mManifest, mReg));
		}
		bool Start()
		{
			if (!mApp->Start()) { return false; }
			for (int i = 0; i < 10; ++i) { mApp->Update(kDt); }
			return g_module != nullptr && g_consumer != nullptr;
		}
		~Harness()
		{
			if (mApp)
			{
				mApp->RequestShutdown();
				for (int guard = 0; guard < 1000 && mApp->Update(kDt); ++guard) {}
				mApp.reset();
			}
			g_source = nullptr;
			g_contributors.clear();
		}
	};

	struct ScopedAlias
	{
		StringCRC mAlias;
		ScopedAlias(const char* alias, const std::string& path) : mAlias(alias)
		{
			PathStore::RegisterToStore(mAlias, Dia::Core::Path::String(path.c_str()));
		}
		~ScopedAlias() { PathStore::UnregisterFromStore(mAlias); }
	};

	std::string RampJson(const char* id, const char* c0 = "#000000", const char* c1 = "#ffffff")
	{
		return std::string(R"({"id":")") + id + R"(","kind":"continuous","stops":[{"t":0.0,"colour":")" + c0 +
			R"("},{"t":1.0,"colour":")" + c1 + R"("}]})";
	}
	std::string RampFile(const std::string& ramps)
	{
		return std::string(R"({"format":"dia.ramps","version":1,"ramps":[)") + ramps + "]}";
	}
	std::string GoodFile(const char* forFormat, const char* key)
	{
		return SchemaFixtureBuilder().For(forFormat).Entry(key).Build();
	}

	std::string ArrayConfig(const char* key, const std::vector<std::string>& aliases)
	{
		std::string s = std::string("{\"") + key + "\":[";
		for (size_t i = 0; i < aliases.size(); ++i) { s += (i ? ",\"" : "\"") + aliases[i] + "\""; }
		return s + "]}";
	}
	std::string Roots(const std::vector<std::string>& aliases) { return ArrayConfig("schema_roots", aliases); }

	double GaugeValue(const char* name)
	{
		auto* g = Dia::Observation::Metric::MetricRegistry::Instance().FindGauge(StringCRC(name));
		return g ? g->Value() : -1.0;
	}

	unsigned int CountHealthReporters()
	{
		Dia::Observation::Health::HealthRegistry::ReporterSnapshot snap[64];
		unsigned int n = 0;
		Dia::Observation::Health::HealthRegistry::Instance().Snapshot(snap, 64, n);
		unsigned int c = 0;
		for (unsigned int i = 0; i < n; ++i) { if (snap[i].name == StringCRC("dia.valueschema")) { ++c; } }
		return c;
	}

	using Dia::Observation::Health::HealthStatus;

	struct OrderContributor : IValueSchemaContributor
	{
		std::vector<int>*	mOrder = nullptr;
		int					mId = 0;
		unsigned int		mCountSeen = 0;
		int					mCalls = 0;
		void ContributeEntries(ValueSchemaRegistry& registry) override
		{
			++mCalls;
			mCountSeen = registry.Count();
			if (mOrder) { mOrder->push_back(mId); }
		}
	};

	struct DupContributor : IValueSchemaContributor
	{
		RegisterResult mResult = RegisterResult::Ok;
		void ContributeEntries(ValueSchemaRegistry& registry) override
		{
			mResult = registry.Register(MakeValueInfo("dup.fmt", "k", ValueKind::Parameter, "<code>"));
		}
	};

	struct RetainingContributor : IValueSchemaContributor
	{
		ValueSchemaRegistry* mRegistry = nullptr;
		void ContributeEntries(ValueSchemaRegistry& registry) override { mRegistry = &registry; }
	};
}

// ---- config forms ---------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaModuleEdge, DegenerateConfigsYieldNoRoots)
{
	for (const char* cfg : { "", "[]", "{}", "null", "42", "\"str\"", "{", "}{", "not json", "true",
		R"({"schema_roots":"x"})", R"({"schema_roots":{}})", R"({"schema_roots":null})", R"({"schema_roots":[]})",
		R"({"schema_roots":5,"ramp_roots":"y"})", R"({"ramp_roots":{"a":1}})" })
	{
		TestVsc m(StringCRC("cfg"));
		m.Configure(cfg);
		EXPECT_EQ(m.GetSchemaRootCount(), 0u) << cfg;
		EXPECT_EQ(m.GetRampRootCount(), 0u) << cfg;
	}
	TestVsc m(StringCRC("cfgnull"));
	m.Configure(nullptr);
	EXPECT_EQ(m.GetSchemaRootCount(), 0u);
	EXPECT_EQ(m.GetRampRootCount(), 0u);
}

TEST(DiaValueSchemaModuleEdge, NonStringAndEmptyEntriesAreIgnored)
{
	TestVsc m(StringCRC("cfg"));
	m.Configure(R"({"schema_roots":[1,null,"",{},[],true,"ok",1.5,"also"],"ramp_roots":[false,"","r"]})");
	ASSERT_EQ(m.GetSchemaRootCount(), 2u);
	EXPECT_STREQ(m.GetSchemaRoot(0), "ok");
	EXPECT_STREQ(m.GetSchemaRoot(1), "also");
	ASSERT_EQ(m.GetRampRootCount(), 1u);
	EXPECT_STREQ(m.GetRampRoot(0), "r");
}

TEST(DiaValueSchemaModuleEdge, AliasOf63CharsAcceptedAnd64Rejected)
{
	const std::string a63(63, 'a'), a64(64, 'b');
	TestVsc m(StringCRC("cfg"));
	m.Configure(ArrayConfig("schema_roots", { a64, a63, "tail" }).c_str());
	ASSERT_EQ(m.GetSchemaRootCount(), 2u);
	EXPECT_STREQ(m.GetSchemaRoot(0), a63.c_str());
	EXPECT_STREQ(m.GetSchemaRoot(1), "tail");
	m.Configure(ArrayConfig("ramp_roots", { a64, a63 }).c_str());
	ASSERT_EQ(m.GetRampRootCount(), 1u);
	EXPECT_STREQ(m.GetRampRoot(0), a63.c_str());
	EXPECT_EQ(m.GetSchemaRootCount(), 0u);	// reconfigure replaces the earlier lists
}

TEST(DiaValueSchemaModuleEdge, SixteenRootsAcceptedSeventeenthDropped)
{
	std::vector<std::string> a16, a17;
	for (int i = 0; i < 17; ++i)
	{
		a17.push_back("alias" + std::to_string(i));
		if (i < 16) { a16.push_back("alias" + std::to_string(i)); }
	}
	ASSERT_EQ(ValueSchemaModule::kMaxSchemaRoots, 16u);
	TestVsc m(StringCRC("cfg"));
	m.Configure(ArrayConfig("schema_roots", a16).c_str());
	EXPECT_EQ(m.GetSchemaRootCount(), 16u);
	m.Configure((std::string("{\"schema_roots\":[") + [&] { std::string s; for (size_t i = 0; i < a17.size(); ++i) { s += (i ? ",\"" : "\"") + a17[i] + "\""; } return s; }() +
		"],\"ramp_roots\":[" + [&] { std::string s; for (size_t i = 0; i < a17.size(); ++i) { s += (i ? ",\"" : "\"") + a17[i] + "\""; } return s; }() + "]}").c_str());
	ASSERT_EQ(m.GetSchemaRootCount(), 16u);
	ASSERT_EQ(m.GetRampRootCount(), 16u);
	EXPECT_STREQ(m.GetSchemaRoot(15), "alias15");
	EXPECT_STREQ(m.GetRampRoot(15), "alias15");
}

TEST(DiaValueSchemaModuleEdge, SchemaAndRampRootListsAreIndependent)
{
	TestVsc m(StringCRC("cfg"));
	m.Configure(R"({"schema_roots":["a"],"ramp_roots":["b","c"]})");
	EXPECT_EQ(m.GetSchemaRootCount(), 1u);
	EXPECT_EQ(m.GetRampRootCount(), 2u);
	EXPECT_STREQ(m.GetRampRoot(1), "c");
	m.Configure(R"({"ramp_roots":["only"]})");
	EXPECT_EQ(m.GetSchemaRootCount(), 0u);
	EXPECT_EQ(m.GetRampRootCount(), 1u);
}

TEST(DiaValueSchemaModuleEdge, DuplicateAliasesAreKeptPin)
{
	// PIN: the same alias listed twice is kept twice (the loaders de-duplicate the resulting files).
	TestVsc m(StringCRC("cfg"));
	m.Configure(R"({"schema_roots":["a","a"]})");
	EXPECT_EQ(m.GetSchemaRootCount(), 2u);
}

TEST(DiaValueSchemaModuleEdge, PrintfDirectivesInAliasesAreKeptVerbatim)
{
	TestVsc m(StringCRC("cfg"));
	m.Configure(R"({"schema_roots":["a%s%d%n","100%","%%"],"ramp_roots":["r%n%s"]})");
	ASSERT_EQ(m.GetSchemaRootCount(), 3u);
	EXPECT_STREQ(m.GetSchemaRoot(0), "a%s%d%n");
	EXPECT_STREQ(m.GetSchemaRoot(1), "100%");
	EXPECT_STREQ(m.GetSchemaRoot(2), "%%");
	ASSERT_EQ(m.GetRampRootCount(), 1u);
	EXPECT_STREQ(m.GetRampRoot(0), "r%n%s");
}

TEST(DiaValueSchemaModuleEdge, UnicodeAliasIsKeptByteForByte)
{
	TestVsc m(StringCRC("cfg"));
	m.Configure("{\"schema_roots\":[\"caf\xC3\xA9_\xE6\x97\xA5\"]}");
	ASSERT_EQ(m.GetSchemaRootCount(), 1u);
	EXPECT_STREQ(m.GetSchemaRoot(0), "caf\xC3\xA9_\xE6\x97\xA5");
}

TEST(DiaValueSchemaModuleEdge, ConfigOfExactly255CharsLoadsAnd300CharsIsTruncatedPin)
{
	SchemaTempDirectory dir("ModEdge_Cfg255");
	dir.Write("a.schema.json", GoodFile("cfg.fmt", "k"));
	ScopedAlias alias("vsc_edge_cfg255", dir.Path());
	const std::string head = R"({"schema_roots":["vsc_edge_cfg255"])";
	std::string ok = head + std::string(255 - head.size() - 1, ' ') + "}";
	ASSERT_EQ(ok.size(), 255u);
	{
		FakeCodeSideSource src;
		Harness h(ok, src);
		ASSERT_TRUE(h.Start());
		EXPECT_EQ(g_module->GetSchemaRootCount(), 1u);
		EXPECT_EQ(g_module->GetService()->Count(), 1u);
	}
	// PIN: the manifest holds the config in a String256; a longer config is cut and so is malformed JSON.
	std::string tooLong = head + std::string(300 - head.size() - 1, ' ') + "}";
	{
		FakeCodeSideSource src;
		Harness h(tooLong, src);
		ASSERT_TRUE(h.Start());
		EXPECT_EQ(g_module->GetSchemaRootCount(), 0u);
		ASSERT_NE(g_module->GetService(), nullptr);
		EXPECT_EQ(g_module->GetService()->Count(), 0u);
	}
}

TEST(DiaValueSchemaModuleEdge, CodeSideFlagOnlyHonouredWhenBool)
{
	struct Case { const char* cfg; bool enabled; };
	const Case cases[] = {
		{ "{}", true }, { R"({"code_side":true})", true }, { R"({"code_side":false})", false },
		{ R"({"code_side":"false"})", true }, { R"({"code_side":0})", true }, { R"({"code_side":null})", true },
		{ R"({"code_side":[false]})", true }, { R"({"code_side":1})", true },
	};
	for (const Case& c : cases)
	{
		FakeCodeSideSource src;
		src.AddMetric("m.one");
		Harness h(c.cfg, src);
		ASSERT_TRUE(h.Start()) << c.cfg;
		EXPECT_EQ(g_module->GetCodeSideReport().mMetricsCollected, c.enabled ? 1u : 0u) << c.cfg;
	}
}

// ---- alias resolution ----------------------------------------------------------------------------------------------

TEST(DiaValueSchemaModuleEdge, UnregisteredSchemaAliasCountsAsRootMissingAndWarnsInHealth)
{
	FakeCodeSideSource src;
	Harness h(Roots({ "vsc_edge_nope_a", "vsc_edge_nope_b" }), src);
	ASSERT_TRUE(h.Start());
	EXPECT_EQ(g_module->GetLoadStats().mRootsMissing, 2u);
	const auto r = g_module->GetHealthReporter().Report();
	EXPECT_EQ(r.warnings, 2u);
	EXPECT_EQ(r.status, HealthStatus::kOK);
	EXPECT_EQ(g_module->GetService()->Count(), 0u);
}

TEST(DiaValueSchemaModuleEdge, UnregisteredRampAliasIsIgnoredByHealthPin)
{
	// PIN: an unregistered ramp_roots alias is only logged; it is not counted in mRootsMissing and gives no warning.
	FakeCodeSideSource src;
	Harness h(ArrayConfig("ramp_roots", { "vsc_edge_nope_ramp" }), src);
	ASSERT_TRUE(h.Start());
	EXPECT_EQ(g_module->GetLoadStats().mRootsMissing, 0u);
	EXPECT_EQ(g_module->GetHealthReporter().Report().warnings, 0u);
	EXPECT_EQ(g_module->GetRampLoadStats().mFilesFound, 0u);
	EXPECT_EQ(g_module->GetService()->RampCount(), 0u);
}

TEST(DiaValueSchemaModuleEdge, RegisteredAliasToMissingDirectoryCountsMissing)
{
	SchemaTempDirectory dir("ModEdge_MissingDir");
	ScopedAlias alias("vsc_edge_gone", dir.Path("not/there"));
	FakeCodeSideSource src;
	Harness h(Roots({ "vsc_edge_gone" }), src);
	ASSERT_TRUE(h.Start());
	EXPECT_EQ(g_module->GetLoadStats().mRootsMissing, 1u);
	EXPECT_EQ(g_module->GetLoadStats().mFilesFound, 0u);
}

TEST(DiaValueSchemaModuleEdge, AliasOf63CharsResolvesEndToEnd)
{
	SchemaTempDirectory dir("ModEdge_Alias63");
	dir.Write("a.schema.json", GoodFile("al.fmt", "k"));
	const std::string a63 = "vsc_edge_" + std::string(54, 'x');
	ASSERT_EQ(a63.size(), 63u);
	ScopedAlias alias(a63.c_str(), dir.Path());
	FakeCodeSideSource src;
	Harness h(Roots({ a63 }), src);
	ASSERT_TRUE(h.Start());
	EXPECT_EQ(g_module->GetService()->Count(), 1u);
	EXPECT_EQ(g_module->GetLoadStats().mRootsMissing, 0u);
}

TEST(DiaValueSchemaModuleEdge, AliasListOrderDoesNotChangeRegistryContent)
{
	SchemaTempDirectory dir("ModEdge_Order");
	dir.Write("a/1.schema.json", SchemaFixtureBuilder().For("ord.fmt").Entry("a1").Entry("a2").Build());
	dir.Write("b/2.schema.json", SchemaFixtureBuilder().For("ord.fmt").Entry("b1").Build());
	dir.Write("b/3.schema.json", SchemaFixtureBuilder().For("ord.other").Entry("z").Build());
	ScopedAlias aliasA("vsc_edge_ord_a", dir.Path("a"));
	ScopedAlias aliasB("vsc_edge_ord_b", dir.Path("b"));

	auto snapshot = [](const std::vector<std::string>& aliases)
	{
		FakeCodeSideSource src;
		Harness h(Roots(aliases), src);
		EXPECT_TRUE(h.Start());
		std::vector<std::string> keys;
		const ValueInfoSpan span = g_module->GetService()->ForFormat(StringCRC("ord.fmt"));
		for (unsigned int i = 0; i < span.mCount; ++i) { keys.push_back(span.mItems[i]->mKey.AsCStr()); }
		keys.push_back("count=" + std::to_string(g_module->GetService()->Count()));
		return keys;
	};
	const auto ab = snapshot({ "vsc_edge_ord_a", "vsc_edge_ord_b" });
	const auto ba = snapshot({ "vsc_edge_ord_b", "vsc_edge_ord_a" });
	EXPECT_EQ(ab, ba);
	ASSERT_EQ(ab.size(), 4u);
	EXPECT_EQ(ab[0], "a1");
	EXPECT_EQ(ab[2], "b1");
}

// ---- contributors ---------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaModuleEdge, ContributorsRunInAddOrderAfterFilesAndBeforeCodeSide)
{
	SchemaTempDirectory dir("ModEdge_ContribOrder");
	dir.Write("a.schema.json", SchemaFixtureBuilder().For("co.fmt").Entry("x").Entry("y").Build());
	ScopedAlias alias("vsc_edge_contrib_order", dir.Path());
	std::vector<int> order;
	OrderContributor c1, c2, c3;
	c1.mOrder = c2.mOrder = c3.mOrder = &order;
	c1.mId = 1; c2.mId = 2; c3.mId = 3;
	FakeCodeSideSource src;
	src.AddMetric("code.metric");
	Harness h(Roots({ "vsc_edge_contrib_order" }), src, { &c1, &c2, &c3 });
	ASSERT_TRUE(h.Start());
	EXPECT_EQ(order, (std::vector<int>{ 1, 2, 3 }));
	EXPECT_EQ(c1.mCountSeen, 2u);	// files are in, code-side collection has not run yet
	EXPECT_EQ(c3.mCountSeen, 2u);
	EXPECT_EQ(g_module->GetService()->Count(), 3u);
}

TEST(DiaValueSchemaModuleEdge, SixteenContributorsAcceptedSeventeenthRejected)
{
	ASSERT_EQ(ValueSchemaModule::kMaxContributors, 16u);
	ValueSchemaModule m(StringCRC("standalone"));
	std::vector<OrderContributor> cs(18);
	for (int i = 0; i < 16; ++i) { EXPECT_TRUE(m.AddContributor(&cs[i])) << i; }
	EXPECT_FALSE(m.AddContributor(&cs[16]));
	EXPECT_FALSE(m.AddContributor(&cs[17]));
	EXPECT_FALSE(m.AddContributor(nullptr));
}

TEST(DiaValueSchemaModuleEdge, SameContributorAddedTwiceIsCalledTwicePin)
{
	// PIN: AddContributor does not de-duplicate; the second registration of an entry is a harmless duplicate.
	FakeCodeSideSource src;
	OrderContributor c;
	Harness h("{}", src, { &c, &c });
	ASSERT_TRUE(h.Start());
	EXPECT_EQ(c.mCalls, 2);
}

TEST(DiaValueSchemaModuleEdge, ContributorDuplicateOfFileEntryLosesToTheFile)
{
	SchemaTempDirectory dir("ModEdge_ContribDup");
	dir.Write("a.schema.json", SchemaFixtureBuilder().For("dup.fmt").Entry("k").Label("FromFile").Build());
	ScopedAlias alias("vsc_edge_contrib_dup", dir.Path());
	DupContributor c;
	FakeCodeSideSource src;
	Harness h(Roots({ "vsc_edge_contrib_dup" }), src, { &c });
	ASSERT_TRUE(h.Start());
	EXPECT_EQ(c.mResult, RegisterResult::DuplicateKey);
	const ValueInfo* v = g_module->GetService()->Find(StringCRC("dup.fmt/k"));
	ASSERT_NE(v, nullptr);
	EXPECT_STREQ(v->mLabel.AsCStr(), "FromFile");
	EXPECT_EQ(g_module->GetService()->Count(), 1u);
}

TEST(DiaValueSchemaModuleEdge, ContributorsCalledOnEveryStart)
{
	OrderContributor c;
	FakeCodeSideSource src;
	Harness h("{}", src, { &c });
	ASSERT_TRUE(h.Start());
	EXPECT_EQ(c.mCalls, 1);
	g_module->CycleStop();
	g_module->CycleStart();
	EXPECT_EQ(c.mCalls, 2);
	g_module->CycleStop();
	g_module->CycleStart();
	EXPECT_EQ(c.mCalls, 3);
}

TEST(DiaValueSchemaModuleEdge, RetainedRegistryRejectsEveryRegisterAfterPublish)
{
	SchemaTempDirectory dir("ModEdge_Retained");
	dir.Write("a.schema.json", GoodFile("ret.fmt", "k"));
	ScopedAlias alias("vsc_edge_retained", dir.Path());
	RetainingContributor c;
	FakeCodeSideSource src;
	Harness h(Roots({ "vsc_edge_retained" }), src, { &c });
	ASSERT_TRUE(h.Start());
	ASSERT_NE(c.mRegistry, nullptr);
	EXPECT_TRUE(c.mRegistry->IsPublished());
	EXPECT_EQ(c.mRegistry->Register(MakeValueInfo("ret.fmt", "k")), RegisterResult::AlreadyPublished);	// even a duplicate
	EXPECT_EQ(c.mRegistry->Register(MakeValueInfo("ret.new", "n")), RegisterResult::AlreadyPublished);
	EXPECT_EQ(g_module->GetService()->Count(), 1u);
	EXPECT_EQ(g_module->GetService()->Find(StringCRC("ret.new/n")), nullptr);
}

// ---- lifecycle ------------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaModuleEdge, StandaloneModuleBeforeStartIsInert)
{
	TestVsc m(StringCRC("standalone"));
	EXPECT_EQ(m.GetService(), nullptr);
	EXPECT_EQ(m.GetLoadStats().mFilesFound, 0u);
	EXPECT_EQ(m.GetRampLoadStats().mFilesFound, 0u);
	EXPECT_EQ(m.GetHealthReporter().Report().status, HealthStatus::kOK);
	EXPECT_EQ(m.LateRegistrationCount(), 0u);
	m.CheckLateRegistrations();
	EXPECT_EQ(m.LateWarningCount(), 0u);
	EXPECT_EQ(m.GetSchemaRootCount(), 0u);
}

TEST(DiaValueSchemaModuleEdge, StopWithoutStartIsHarmless)
{
	TestVsc m(StringCRC("standalone"));
	m.CycleStop();
	m.CycleStop();
	EXPECT_EQ(m.GetService(), nullptr);
	EXPECT_EQ(m.GetHealthReporter().Report().status, HealthStatus::kOK);
}

TEST(DiaValueSchemaModuleEdge, TenStartStopCyclesReplaceServiceAndRebindMetrics)
{
	SchemaTempDirectory dir("ModEdge_Cycles");
	for (int i = 0; i < 10; ++i) { dir.Write(("f" + std::to_string(i) + ".schema.json").c_str(), GoodFile("cyc.fmt", ("k" + std::to_string(i)).c_str())); }
	ScopedAlias alias("vsc_edge_cycles", dir.Path());
	FakeCodeSideSource src;
	Harness h(Roots({ "vsc_edge_cycles" }), src);
	ASSERT_TRUE(h.Start());
	const unsigned int generation = Dia::Observation::Metric::MetricRegistry::Instance().Generation();

	for (int cycle = 0; cycle < 10; ++cycle)
	{
		ASSERT_NE(g_module->GetService(), nullptr);
		EXPECT_EQ(g_module->GetService()->Count(), static_cast<unsigned int>(10 - cycle)) << cycle;
		EXPECT_DOUBLE_EQ(GaugeValue("valueschema.entries_loaded"), static_cast<double>(10 - cycle)) << cycle;
		g_module->CycleStop();
		fs::remove(fs::path(dir.Path(("f" + std::to_string(cycle) + ".schema.json").c_str())));
		g_module->CycleStart();
	}
	ASSERT_NE(g_module->GetService(), nullptr);
	EXPECT_EQ(g_module->GetService()->Count(), 0u);
	EXPECT_DOUBLE_EQ(GaugeValue("valueschema.entries_loaded"), 0.0);
	EXPECT_EQ(Dia::Observation::Metric::MetricRegistry::Instance().Generation(), generation) << "no metric registered twice";
}

TEST(DiaValueSchemaModuleEdge, HundredStartStopCyclesLeakNothingObservable)
{
	SchemaTempDirectory dir("ModEdge_Cycles100");
	dir.Write("a.schema.json", GoodFile("c100.fmt", "k"));
	ScopedAlias alias("vsc_edge_cycles100", dir.Path());
	FakeCodeSideSource src;
	src.AddMetric("m.one");
	Harness h(Roots({ "vsc_edge_cycles100" }), src);
	ASSERT_TRUE(h.Start());
	const unsigned int generation = Dia::Observation::Metric::MetricRegistry::Instance().Generation();
	for (int i = 0; i < 100; ++i)
	{
		g_module->CycleStop();
		g_module->CycleStart();
		ASSERT_NE(g_module->GetService(), nullptr);
		ASSERT_EQ(g_module->GetService()->Count(), 2u) << i;
	}
	EXPECT_EQ(Dia::Observation::Metric::MetricRegistry::Instance().Generation(), generation);
	// The clamp counter is rebound to the live module after the cycles.
	auto* counter = Dia::Observation::Metric::MetricRegistry::Instance().FindCounter(StringCRC("valueschema.clamp.count"));
	ASSERT_NE(counter, nullptr);
	const auto before = counter->Value();
	const ValueInfo* info = g_module->GetService()->Find(StringCRC("c100.fmt/k"));
	ASSERT_NE(info, nullptr);
	g_module->GetClamper().Clamp(1.0, *info);
	EXPECT_GE(counter->Value(), before);
}

TEST(DiaValueSchemaModuleEdge, StartTwiceWithoutStopStaysConsistentAndAssertFree)
{
	// FIX 6: a second DoStart while the service is still registered used to re-Register the stream (assert) and free
	// the service the stream still pointed at.
	SchemaTempDirectory dir("ModEdge_DoubleStart");
	dir.Write("a.schema.json", GoodFile("ds.fmt", "k"));
	ScopedAlias alias("vsc_edge_double", dir.Path());
	FakeCodeSideSource src;
	Harness h(Roots({ "vsc_edge_double" }), src);
	ASSERT_TRUE(h.Start());
	const unsigned int healthBefore = CountHealthReporters();
	{
		VsTest::AssertCounter asserts;
		g_module->CycleStart();
		EXPECT_EQ(asserts.Count(), 0) << "second Start must not re-register the stream";
	}
	ASSERT_NE(g_module->GetService(), nullptr);
	EXPECT_EQ(g_module->GetService()->Count(), 1u);
	EXPECT_EQ(CountHealthReporters(), healthBefore) << "health reporter registered once";
	EXPECT_EQ(g_module->GetHealthReporter().Report().status, HealthStatus::kOK);
}

TEST(DiaValueSchemaModuleEdge, HealthReporterRegisteredByStart)
{
	const unsigned int before = CountHealthReporters();
	FakeCodeSideSource src;
	Harness h("{}", src);
	ASSERT_TRUE(h.Start());
	EXPECT_EQ(CountHealthReporters(), before + 1u);
}

TEST(DiaValueSchemaModuleEdge, ServicePointerNonNullAfterStopButStreamDeregisteredPin)
{
	// PIN: DoStop keeps the registry + service alive (GetService() stays non-null) but deregisters the stream.
	FakeCodeSideSource src;
	Harness h("{}", src);
	ASSERT_TRUE(h.Start());
	EXPECT_TRUE(g_consumer->mReader.IsAvailable());
	g_module->CycleStop();
	EXPECT_NE(g_module->GetService(), nullptr);
	EXPECT_FALSE(g_consumer->mReader.IsAvailable());
}

TEST(DiaValueSchemaModuleEdge, DoUpdateIsANoOpForAThousandFrames)
{
	SchemaTempDirectory dir("ModEdge_Update");
	dir.Write("a.schema.json", GoodFile("up.fmt", "k"));
	ScopedAlias alias("vsc_edge_update", dir.Path());
	FakeCodeSideSource src;
	src.AddMetric("m.one");
	Harness h(Roots({ "vsc_edge_update" }), src);
	ASSERT_TRUE(h.Start());
	const ValueSchemaService* service = g_module->GetService();
	const unsigned int count = service->Count();
	const unsigned int generation = Dia::Observation::Metric::MetricRegistry::Instance().Generation();
	const double entries = GaugeValue("valueschema.entries_loaded");
	for (int i = 0; i < 1000; ++i) { g_module->Tick(kDt); }
	EXPECT_EQ(g_module->GetService(), service);
	EXPECT_EQ(service->Count(), count);
	EXPECT_EQ(Dia::Observation::Metric::MetricRegistry::Instance().Generation(), generation);
	EXPECT_DOUBLE_EQ(GaugeValue("valueschema.entries_loaded"), entries);
	EXPECT_EQ(g_module->LateWarningCount(), 0u);
}

// ---- gauges ---------------------------------------------------------------------------------------------------------

namespace
{
	const Dia::API::ValueParam kParams[] = {
		{ "count", "int", true, "How many" },
		{ "name", "string", false, "Which one" },
	};
}

TEST(DiaValueSchemaModuleEdge, GaugesReflectRampsAndCodeSideAfterStart)
{
	SchemaTempDirectory diaDir("ModEdge_GaugesDia"), game("ModEdge_GaugesGame");
	diaDir.Write("ramps.json", RampFile(RampJson("dia.one") + "," + RampJson("dia.two")));
	game.Write("a.ramps.json", RampFile(RampJson("game.one")));
	game.Write("bad.ramps.json", "{ broken");
	ScopedAlias a1("vsc_edge_gauge_dia", diaDir.Path());
	ScopedAlias a2("vsc_edge_gauge_game", game.Path());

	FakeCodeSideSource src;
	src.AddMetric("m.described", "L", "desc", "u");
	src.AddMetric("m.bare");
	src.AddCommand("cmd.described", "Game", "d", kParams, 2);
	Harness h(R"({"schema_roots":["vsc_edge_gauge_game"],"ramp_roots":["vsc_edge_gauge_dia"]})", src);
	ASSERT_TRUE(h.Start());

	EXPECT_DOUBLE_EQ(GaugeValue("valueschema.ramps_loaded"), 3.0);
	EXPECT_DOUBLE_EQ(GaugeValue("valueschema.ramp_files_failed"), 1.0);
	EXPECT_DOUBLE_EQ(GaugeValue("valueschema.args_described"), 2.0);
	EXPECT_DOUBLE_EQ(GaugeValue("valueschema.commands_described"), 1.0);
	EXPECT_DOUBLE_EQ(GaugeValue("valueschema.metrics_described"), 1.0);
	EXPECT_DOUBLE_EQ(GaugeValue("valueschema.metrics_undescribed"), 1.0);
	EXPECT_EQ(g_module->GetHealthReporter().Report().reason, StringCRC("valueschema.ramp_files_failed"));
}

TEST(DiaValueSchemaModuleEdge, CommandsDescribedGaugeCountsEveryCollectedCommandPin)
{
	// PIN (suspected defect 5): there is no "commands described" field in CodeSideReport, so the gauge
	// valueschema.commands_described is set to mCommandsCollected, undescribed commands included.
	FakeCodeSideSource src;
	src.AddCommand("cmd.described", "Game", "Does a thing", kParams, 2);
	src.AddCommand("cmd.undescribed", "Game", "", nullptr, 0);
	Harness h("{}", src);
	ASSERT_TRUE(h.Start());
	const CodeSideReport& r = g_module->GetCodeSideReport();
	EXPECT_DOUBLE_EQ(GaugeValue("valueschema.commands_described"), static_cast<double>(r.mCommandsCollected));
}

TEST(DiaValueSchemaModuleEdge, GaugesAreZeroForEmptyStart)
{
	FakeCodeSideSource src;
	Harness h("{}", src);
	ASSERT_TRUE(h.Start());
	for (const char* g : { "valueschema.entries_loaded", "valueschema.files_failed", "valueschema.ramps_loaded",
		"valueschema.ramp_files_failed", "valueschema.metrics_described", "valueschema.metrics_undescribed",
		"valueschema.commands_described", "valueschema.args_described" })
	{
		EXPECT_DOUBLE_EQ(GaugeValue(g), 0.0) << g;
	}
}

TEST(DiaValueSchemaModuleEdge, DoubleRampRootAliasLoadsDiaRampsOnce)
{
	// FIX 2: the same Dia root listed twice used to reject every ramp on the second load.
	SchemaTempDirectory diaDir("ModEdge_DiaTwice");
	diaDir.Write("ramps.json", RampFile(RampJson("dia.one") + "," + RampJson("dia.two")));
	ScopedAlias a("vsc_edge_dia_twice", diaDir.Path());
	FakeCodeSideSource src;
	Harness h(ArrayConfig("ramp_roots", { "vsc_edge_dia_twice", "vsc_edge_dia_twice" }), src);
	ASSERT_TRUE(h.Start());
	EXPECT_EQ(g_module->GetService()->RampCount(), 2u);
	EXPECT_EQ(g_module->GetRampLoadStats().mRampsRejected, 0u);
	EXPECT_EQ(g_module->GetRampLoadStats().mFilesFound, 1u);
}

// ---- late registrations ---------------------------------------------------------------------------------------------

TEST(DiaValueSchemaModuleEdge, LateRegistrationCountSaturatesWhenGenerationGoesBackwards)
{
	// FIX 4: unsigned subtraction wrapped to ~4 billion when a source generation went backwards (registry reset).
	FakeCodeSideSource src;
	src.AddMetric("m.a");
	src.AddMetric("m.b");
	src.AddMetric("m.c");
	src.AddCommand("c.a", "Game", "d", nullptr, 0, true);
	Harness h("{}", src);
	ASSERT_TRUE(h.Start());
	EXPECT_EQ(g_module->LateRegistrationCount(), 0u);

	src.mMetricGen = 0;	// reset: three behind
	EXPECT_EQ(g_module->LateRegistrationCount(), 0u);
	g_module->CheckLateRegistrations();
	EXPECT_EQ(g_module->LateWarningCount(), 0u);

	src.mMetricGen = 5;	// two ahead of the collection
	EXPECT_EQ(g_module->LateRegistrationCount(), 2u);

	src.mMetricGen = 1;	// behind again, but the command generation moves forward
	src.mCommandGen = 4;
	EXPECT_EQ(g_module->LateRegistrationCount(), 3u);
}

TEST(DiaValueSchemaModuleEdge, LateRegistrationCountWithBothGenerationsBackwardsIsZero)
{
	FakeCodeSideSource src;
	src.AddMetric("m.a");
	src.AddCommand("c.a", "Game", "d", nullptr, 0, true);
	Harness h("{}", src);
	ASSERT_TRUE(h.Start());
	src.mMetricGen = 0;
	src.mCommandGen = 0;
	EXPECT_EQ(g_module->LateRegistrationCount(), 0u);
	g_module->CheckLateRegistrations();
	EXPECT_EQ(g_module->LateWarningCount(), 0u);
}

TEST(DiaValueSchemaModuleEdge, CheckLateRegistrationsNeverWarnsWhenCodeSideDisabled)
{
	FakeCodeSideSource src;
	Harness h(R"({"code_side":false})", src);
	ASSERT_TRUE(h.Start());
	src.AddMetric("late.one");
	src.AddCommand("late.cmd", "Game", "d", nullptr, 0, true);
	for (int i = 0; i < 5; ++i) { g_module->CheckLateRegistrations(); }
	EXPECT_EQ(g_module->LateRegistrationCount(), 0u);
	EXPECT_EQ(g_module->LateWarningCount(), 0u);
}

TEST(DiaValueSchemaModuleEdge, LateWarningStateResetsOnRestart)
{
	FakeCodeSideSource src;
	Harness h("{}", src);
	ASSERT_TRUE(h.Start());
	src.AddMetric("late.one");
	g_module->CheckLateRegistrations();
	EXPECT_EQ(g_module->LateWarningCount(), 1u);
	g_module->CycleStop();
	g_module->CycleStart();	// re-collects: the late metric is now part of the schema
	EXPECT_EQ(g_module->LateRegistrationCount(), 0u);
	EXPECT_NE(g_module->GetService()->Find(StringCRC("dia.metric/late.one")), nullptr);
	src.AddMetric("late.two");
	g_module->CheckLateRegistrations();
	EXPECT_EQ(g_module->LateWarningCount(), 2u);	// warns again: the last-warned count was reset to 0
}
