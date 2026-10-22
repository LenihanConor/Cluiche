#include <gtest/gtest.h>
#include <DiaValueSchema/ValueSchemaQuery.h>
#include <DiaValueSchema/ValueSchemaModule.h>
#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaValueSchema/ValueSchemaLoader.h>
#include <DiaValueSchema/IValueSchemaContributor.h>
#include <DiaValueSchema/Testing/SchemaFixtureBuilder.h>
#include <DiaValueSchema/Testing/ValueInfoBuilder.h>
#include <DiaValueSchema/Testing/FakeCodeSideSource.h>
#include <DiaDebugServer/QueryRegistry.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Counter.h>
#include <memory>
#include <string>

// Feature 3 task 3: valueschema.get query handler + schema Version() (AC 8).

using namespace Dia::ValueSchema;
using namespace Dia::ApplicationFlow;
using Dia::Core::StringCRC;
using Dia::ValueSchema::Testing::SchemaFixtureBuilder;
using Dia::ValueSchema::Testing::MakeValueInfo;

namespace
{
	constexpr float kDt = 1.0f / 60.0f;

	uint64_t CounterValue(const char* name)
	{
		auto* c = Dia::Observation::Metric::MetricRegistry::Instance().FindCounter(StringCRC(name));
		return c ? c->Value() : 0u;
	}

	// Loads sidecar text into a registry; entries are registered in file order.
	void LoadInto(ValueSchemaRegistry& registry, const std::string& text, const char* file)
	{
		LoadResult r = ValueSchemaLoader::LoadFromText(text.c_str(), file);
		ASSERT_TRUE(r.mOk) << file;
		for (unsigned int i = 0; i < r.mEntries.Size(); ++i) { registry.Register(r.mEntries[i]); }
	}

	struct QueryFixture : ::testing::Test
	{
		void SetUp() override
		{
			SchemaFixtureBuilder eco;
			eco.For("cow.ecology_params")
				.Entry("grazing_rate", "parameter", "float").Label("Grazing rate").Description("How fast herds graze").Unit("1/day").Hint("Raise for faster depletion")
					.Min(0.0).Max(10.0).Default(Json::Value(2.5)).Affects("cow.ecology_params/regrowth").Related("cow.ecology_params/regrowth")
				.Entry("grazing_mode", "parameter", "enum").Label("Mode").Description("Grazing mode")
					.EnumValueEntry("fast", "Fast", "Quick").EnumValueEntry("slow", "Slow", "Slow one")
				.Entry("regrowth", "parameter", "float").Label("Regrowth").Description("Regrowth");
			SchemaFixtureBuilder wx;
			wx.For("cow.weather")
				.Entry("wind", "parameter", "float").Label("Wind").Description("Wind speed");

			LoadInto(mRegistry, eco.Build(), "eco.schema.json");
			LoadInto(mRegistry, wx.Build(), "wx.schema.json");
			mRegistry.Publish();
			mService.reset(new ValueSchemaService(mRegistry));
			mHandler.reset(new ValueSchemaQueryHandler(*mService));
		}

		Json::Value Query(const char* prefix)
		{
			Json::Value args(Json::objectValue);
			if (prefix) { args["prefix"] = prefix; }
			return mHandler->Handle(args);
		}

		ValueSchemaRegistry							mRegistry;
		std::unique_ptr<ValueSchemaService>			mService;
		std::unique_ptr<ValueSchemaQueryHandler>	mHandler;
	};
}

TEST_F(QueryFixture, OmittedPrefixReturnsAllEntriesAndVersion)
{
	const Json::Value r = Query(nullptr);
	ASSERT_TRUE(r["entries"].isArray());
	EXPECT_EQ(r["entries"].size(), 4u);
	EXPECT_EQ(r["version"].asUInt(), mService->Version());
	EXPECT_GT(mService->Version(), 0u);
}

TEST_F(QueryFixture, FormatPrefixReturnsOnlyThatFormat)
{
	const Json::Value r = Query("cow.ecology_params/");
	ASSERT_EQ(r["entries"].size(), 3u);
	for (Json::ArrayIndex i = 0; i < r["entries"].size(); ++i) { EXPECT_EQ(r["entries"][i]["for"].asString(), "cow.ecology_params"); }
}

TEST_F(QueryFixture, PartialKeyPrefixWorks)
{
	const Json::Value r = Query("cow.ecology_params/grazing");
	ASSERT_EQ(r["entries"].size(), 2u);
	EXPECT_EQ(r["entries"][0]["key"].asString(), "grazing_rate");
	EXPECT_EQ(r["entries"][1]["key"].asString(), "grazing_mode");
}

TEST_F(QueryFixture, UnknownPrefixReturnsEmptyEntriesAndVersion)
{
	const Json::Value r = Query("nope/");
	ASSERT_TRUE(r["entries"].isArray());
	EXPECT_EQ(r["entries"].size(), 0u);
	EXPECT_EQ(r["version"].asUInt(), mService->Version());
}

TEST_F(QueryFixture, EntryJsonUsesSidecarFieldNames)
{
	const Json::Value r = Query("cow.ecology_params/grazing_rate");
	ASSERT_EQ(r["entries"].size(), 1u);
	const Json::Value& e = r["entries"][0];
	EXPECT_EQ(e["for"].asString(), "cow.ecology_params");
	EXPECT_EQ(e["key"].asString(), "grazing_rate");
	EXPECT_EQ(e["kind"].asString(), "parameter");
	EXPECT_EQ(e["label"].asString(), "Grazing rate");
	EXPECT_EQ(e["description"].asString(), "How fast herds graze");
	EXPECT_EQ(e["hint"].asString(), "Raise for faster depletion");
	EXPECT_EQ(e["unit"].asString(), "1/day");
	EXPECT_EQ(e["type"].asString(), "float");
	EXPECT_DOUBLE_EQ(e["min"].asDouble(), 0.0);
	EXPECT_DOUBLE_EQ(e["max"].asDouble(), 10.0);
	EXPECT_DOUBLE_EQ(e["default"].asDouble(), 2.5);
	ASSERT_TRUE(e["affects"].isArray());
	EXPECT_EQ(e["affects"][0].asString(), "cow.ecology_params/regrowth");
	ASSERT_TRUE(e["related"].isArray());
	EXPECT_EQ(e["related"][0].asString(), "cow.ecology_params/regrowth");
}

TEST_F(QueryFixture, AbsentOptionalFieldsAreOmitted)
{
	const Json::Value r = Query("cow.weather/wind");
	ASSERT_EQ(r["entries"].size(), 1u);
	const Json::Value& e = r["entries"][0];
	for (const char* absent : { "hint", "unit", "min", "max", "default", "affects", "related", "source", "values", "ui" })
	{
		EXPECT_FALSE(e.isMember(absent)) << absent;
	}
}

TEST_F(QueryFixture, EnumValuesAreEmittedWithIdLabelDescription)
{
	const Json::Value r = Query("cow.ecology_params/grazing_mode");
	ASSERT_EQ(r["entries"].size(), 1u);
	const Json::Value& values = r["entries"][0]["values"];
	ASSERT_TRUE(values.isArray());
	ASSERT_EQ(values.size(), 2u);
	EXPECT_EQ(values[0]["id"].asString(), "fast");
	EXPECT_EQ(values[0]["label"].asString(), "Fast");
	EXPECT_EQ(values[0]["description"].asString(), "Quick");
}

TEST_F(QueryFixture, VersionIsStableAcrossRepeatedQueries)
{
	const unsigned int a = Query(nullptr)["version"].asUInt();
	const unsigned int b = Query("cow.weather/")["version"].asUInt();
	EXPECT_EQ(a, b);
}

TEST_F(QueryFixture, MetricsCountServedAndUnresolvedQueries)
{
	const uint64_t servedBefore = CounterValue("valueschema.queries_served");
	const uint64_t unresolvedBefore = CounterValue("valueschema.query_unresolved");
	Query("cow.weather/");
	Query("nope/");
	Query(nullptr);
	EXPECT_EQ(CounterValue("valueschema.queries_served") - servedBefore, 3u);
	EXPECT_EQ(CounterValue("valueschema.query_unresolved") - unresolvedBefore, 1u);
}

TEST(ValueSchemaVersionCounter, DistinctRegistriesGetIncreasingVersions)
{
	ValueSchemaRegistry a;
	ValueSchemaRegistry b;
	EXPECT_EQ(a.Version(), 0u);	// unpublished
	a.Publish();
	b.Publish();
	EXPECT_GT(a.Version(), 0u);
	EXPECT_GT(b.Version(), a.Version());
	const unsigned int stable = a.Version();
	a.Publish();	// publishing twice must not renumber
	EXPECT_EQ(a.Version(), stable);
}

// --- Module: registration with a QueryRegistry and version change on restart ----------------------------------------
namespace
{
	struct QueryContributor : IValueSchemaContributor
	{
		void ContributeEntries(ValueSchemaRegistry& registry) override
		{
			registry.Register(MakeValueInfo("code.metrics", "frame_ms", ValueKind::Metric, "<code>"));
		}
	};

	struct QueryConsumer : Module
	{
		static const StringCRC kTypeId;
		explicit QueryConsumer(const StringCRC& id) : Module(id) {}
		ServiceStreamReader<ValueSchemaService> mReader{ this, StringCRC(ValueSchemaModule::kServiceStreamId) };
		void OnConnectStreams(Application& app) override { mReader.Connect(app); }
		StartResult DoStart() override { return StartResult::kReady; }
		void DoUpdate(float) override {}
		StopResult DoStop() override { return StopResult::kDone; }
	};
	const StringCRC QueryConsumer::kTypeId("QueryConsumer");

	struct TestVsc : ValueSchemaModule
	{
		using ValueSchemaModule::ValueSchemaModule;
		void CycleStop() { DoStop(); }
		void CycleStart() { DoStart(); }
	};

	TestVsc*										g_module = nullptr;
	Dia::ValueSchema::Testing::FakeCodeSideSource	g_emptySource;
	QueryContributor								g_contributor;
	Dia::DebugServer::QueryRegistry*				g_queryRegistry = nullptr;

	Module* CreateVsc(const StringCRC& id)
	{
		auto* m = new TestVsc(id);
		m->SetCodeSideSource(&g_emptySource);
		m->SetComponentDescriptors(nullptr, 0);	// exact-count tests: no ECS component fields
		m->AddContributor(&g_contributor);
		m->SetQueryRegistry(g_queryRegistry);
		g_module = m;
		return m;
	}

	Module* CreateConsumer(const StringCRC& id) { return new QueryConsumer(id); }

	struct ModuleHarness
	{
		TypeRegistry					mReg;
		ApplicationManifestV3			mManifest;
		std::unique_ptr<Application>	mApp;

		ModuleHarness()
		{
			g_module = nullptr;
			mReg.Register(ValueSchemaModule::kTypeId, CreateVsc);
			mReg.Register(QueryConsumer::kTypeId, CreateConsumer);
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
			ModuleDeclaration vsc;
			vsc.instanceId = StringCRC("valueSchema");
			vsc.typeId = ValueSchemaModule::kTypeId;
			vsc.startTimeoutMs = 10000.0f;
			vsc.stopTimeoutMs = 5000.0f;
			vsc.stages.Add(StringCRC("Boot"));
			ChannelBinding ch;
			ch.id = StringCRC(ValueSchemaModule::kServiceStreamId);
			ch.role = StringCRC("provides");
			vsc.channels.Add(ch);
			vsc.configJson = "{}";
			pu.modules.Add(vsc);
			ModuleDeclaration consumer;
			consumer.instanceId = StringCRC("consumer");
			consumer.typeId = QueryConsumer::kTypeId;
			consumer.startTimeoutMs = 10000.0f;
			consumer.stopTimeoutMs = 5000.0f;
			consumer.stages.Add(StringCRC("Boot"));
			ChannelBinding cch;
			cch.id = StringCRC(ValueSchemaModule::kServiceStreamId);
			cch.role = StringCRC("consumes");
			consumer.channels.Add(cch);
			pu.modules.Add(consumer);
			m.processingUnits.Add(pu);
			mApp.reset(new Application(mManifest, mReg));
		}
		bool Start()
		{
			if (!mApp->Start()) { return false; }
			for (int i = 0; i < 10; ++i) { mApp->Update(kDt); }
			return g_module != nullptr;
		}
		~ModuleHarness()
		{
			if (!mApp) { return; }
			mApp->RequestShutdown();
			for (int guard = 0; guard < 1000 && mApp->Update(kDt); ++guard) {}
			mApp.reset();
		}
	};
}

TEST(ValueSchemaQueryModule, VersionChangesAfterRestartAndQueryFollowsLifecycle)
{
	Dia::DebugServer::QueryRegistry queries;
	g_queryRegistry = &queries;
	{
		ModuleHarness h;
		ASSERT_TRUE(h.Start());
		const StringCRC name("valueschema.get");
		ASSERT_TRUE(queries.Has(name));

		const Json::Value first = queries.Execute(name, Json::Value(Json::objectValue));
		const Json::Value again = queries.Execute(name, Json::Value(Json::objectValue));
		EXPECT_EQ(first["version"].asUInt(), again["version"].asUInt());
		EXPECT_EQ(first["entries"].size(), 1u);
		EXPECT_EQ(first["entries"][0]["key"].asString(), "frame_ms");

		g_module->CycleStop();
		EXPECT_FALSE(queries.Has(name));
		g_module->CycleStart();
		ASSERT_TRUE(queries.Has(name));
		const Json::Value afterRestart = queries.Execute(name, Json::Value(Json::objectValue));
		EXPECT_NE(afterRestart["version"].asUInt(), first["version"].asUInt());
		EXPECT_EQ(afterRestart["version"].asUInt(), g_module->GetService()->Version());
	}
	EXPECT_FALSE(queries.Has(StringCRC("valueschema.get")));	// shutdown unregisters
	g_queryRegistry = nullptr;
}
