#include <gtest/gtest.h>
#include <DiaValueSchema/ValueSchemaModule.h>
#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaValueSchema/IValueSchemaContributor.h>
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
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/Gauge.h>
#include <memory>
#include <string>

// Feature 1 task 6: ValueSchemaModule lifecycle through a live single-PU Application
// (same pattern as TestDiaSimTimeModule / TestServiceChannel). AC 2, 4, 9, 10, 11.

using namespace Dia::ApplicationFlow;
using namespace Dia::ValueSchema;
using Dia::Core::StringCRC;
using Dia::Core::PathStore;
using Dia::ValueSchema::Testing::SchemaFixtureBuilder;
using Dia::ValueSchema::Testing::SchemaTempDirectory;
using Dia::ValueSchema::Testing::MakeValueInfo;

namespace
{
	constexpr float kDt = 1.0f / 60.0f;

	// Consumer on the same stream: records what it sees through the ServiceStream.
	struct VSC_ConsumerModule : Module
	{
		static const StringCRC kTypeId;
		explicit VSC_ConsumerModule(const StringCRC& id) : Module(id) {}

		ServiceStreamReader<ValueSchemaService> mReader{ this, StringCRC(ValueSchemaModule::kServiceStreamId) };
		bool			mSawAvailable = false;
		unsigned int	mCount = 0;

		void OnConnectStreams(Application& app) override { mReader.Connect(app); }
		StartResult DoStart() override { Observe(); return StartResult::kReady; }
		void DoUpdate(float) override { Observe(); }
		StopResult DoStop() override { return StopResult::kDone; }

		void Observe()
		{
			if (mReader.IsAvailable()) { mSawAvailable = true; mCount = mReader.Get().Count(); }
		}
	};
	const StringCRC VSC_ConsumerModule::kTypeId("VSC_ConsumerModule");

	// Contributor: adds one code-side entry; keeps the registry to try a post-publish Register.
	struct TestContributor : IValueSchemaContributor
	{
		ValueSchemaRegistry* mRegistry = nullptr;
		int mCalls = 0;
		void ContributeEntries(ValueSchemaRegistry& registry) override
		{
			++mCalls;
			mRegistry = &registry;
			registry.Register(MakeValueInfo("code.metrics", "frame_ms", ValueKind::Metric, "<code>"));
		}
	};

	ValueSchemaModule*		g_module = nullptr;
	VSC_ConsumerModule*		g_consumer = nullptr;
	IValueSchemaContributor* g_pendingContributor = nullptr;

	Module* CreateVsc(const StringCRC& id)
	{
		auto* m = new ValueSchemaModule(id);
		if (g_pendingContributor) { m->AddContributor(g_pendingContributor); }
		g_module = m;
		return m;
	}
	Module* CreateConsumer(const StringCRC& id) { auto* m = new VSC_ConsumerModule(id); g_consumer = m; return m; }

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

	ApplicationManifestV3 BuildManifest(const char* configJson)
	{
		ApplicationManifestV3 m;
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
		vsc.configJson = configJson;
		pu.modules.Add(vsc);
		pu.modules.Add(MakeModule(VSC_ConsumerModule::kTypeId, "consumer", "consumes"));
		m.processingUnits.Add(pu);
		return m;
	}

	struct Harness
	{
		TypeRegistry					mReg;
		ApplicationManifestV3			mManifest;
		std::unique_ptr<Application>	mApp;

		explicit Harness(const std::string& configJson)
		{
			g_module = nullptr;
			g_consumer = nullptr;
			mReg.Register(ValueSchemaModule::kTypeId, CreateVsc);
			mReg.Register(VSC_ConsumerModule::kTypeId, CreateConsumer);
			mManifest = BuildManifest(configJson.c_str());
			mApp.reset(new Application(mManifest, mReg));
		}
		bool Start()
		{
			if (!mApp->Start()) return false;
			for (int i = 0; i < 10; ++i) mApp->Update(kDt);
			return g_module != nullptr && g_consumer != nullptr;
		}
		void Shutdown()
		{
			if (!mApp) return;
			mApp->RequestShutdown();
			for (int guard = 0; guard < 1000 && mApp->Update(kDt); ++guard) {}
			mApp.reset();
		}
		~Harness() { Shutdown(); }
	};

	// Registers a PathStore alias for the test's lifetime.
	struct ScopedAlias
	{
		StringCRC mAlias;
		ScopedAlias(const char* alias, const std::string& path) : mAlias(alias)
		{
			PathStore::RegisterToStore(mAlias, Dia::Core::Path::String(path.c_str()));
		}
		~ScopedAlias() { PathStore::UnregisterFromStore(mAlias); }
	};

	std::string Roots(const char* a, const char* b = nullptr)
	{
		std::string s = std::string("{\"schema_roots\":[\"") + a + "\"";
		if (b) { s += std::string(",\"") + b + "\""; }
		return s + "]}";
	}

	std::string GoodFile(const char* forFormat, const char* key)
	{
		return SchemaFixtureBuilder().For(forFormat).Entry(key).Min(0.0).Max(2.0).Build();
	}
}

TEST(DiaValueSchemaModule, ConfigReadsSchemaRoots)
{
	Harness h(Roots("vsc_alias_one", "vsc_alias_two"));
	ASSERT_TRUE(h.Start());
	ASSERT_EQ(g_module->GetSchemaRootCount(), 2u);
	EXPECT_STREQ(g_module->GetSchemaRoot(0), "vsc_alias_one");
	EXPECT_STREQ(g_module->GetSchemaRoot(1), "vsc_alias_two");
}

TEST(DiaValueSchemaModule, LoadsRootsAndPublishesServiceOnStream)
{
	SchemaTempDirectory dir("ModuleLoads");
	dir.Write("eco/params.schema.json", SchemaFixtureBuilder().For("eco.params").Entry("grazing.rate").Entry("growth.rate").Build());
	dir.Write("eco/events.schema.json", SchemaFixtureBuilder().For("eco.events").Entry("fire_started", "event").Build());
	ScopedAlias alias("vsc_mod_root", dir.Path());

	Harness h(Roots("vsc_mod_root", "vsc_unregistered_alias"));
	ASSERT_TRUE(h.Start());

	const ValueSchemaService* svc = g_module->GetService();
	ASSERT_NE(svc, nullptr);
	EXPECT_EQ(svc->Count(), 3u);
	EXPECT_NE(svc->Find(StringCRC("eco.params/grazing.rate")), nullptr);
	EXPECT_EQ(svc->ForFormat(StringCRC("eco.params")).mCount, 2u);
	EXPECT_NE(svc->FindEvent(StringCRC("fire_started")), nullptr);

	EXPECT_EQ(g_module->GetLoadStats().mRootsMissing, 1u);	// unregistered alias: warning only
	EXPECT_EQ(g_module->GetLoadStats().mFilesLoaded, 2u);

	EXPECT_TRUE(g_consumer->mSawAvailable) << "consumer must read the service through the ServiceStream";
	EXPECT_EQ(g_consumer->mCount, 3u);
}

TEST(DiaValueSchemaModule, HealthDegradedWhenFileFailsAndMetricsCount)
{
	SchemaTempDirectory dir("ModuleBadFile");
	dir.Write("a.schema.json", GoodFile("h.a", "x"));
	dir.Write("b.schema.json", "{ broken");
	dir.Write("c.schema.json", GoodFile("h.c", "y"));
	ScopedAlias alias("vsc_bad_root", dir.Path());

	Harness h(Roots("vsc_bad_root"));
	ASSERT_TRUE(h.Start());

	const auto report = g_module->GetHealthReporter().Report();
	EXPECT_EQ(report.status, Dia::Observation::Health::HealthStatus::kDegraded);
	EXPECT_EQ(g_module->GetHealthReporter().GetReporterName(), StringCRC("dia.valueschema"));
	EXPECT_EQ(g_module->GetService()->Count(), 2u);

	auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
	auto* entries = metrics.FindGauge(StringCRC("valueschema.entries_loaded"));
	auto* failed = metrics.FindGauge(StringCRC("valueschema.files_failed"));
	ASSERT_NE(entries, nullptr);
	ASSERT_NE(failed, nullptr);
	EXPECT_DOUBLE_EQ(entries->Value(), 2.0);
	EXPECT_DOUBLE_EQ(failed->Value(), 1.0);
}

TEST(DiaValueSchemaModule, HealthOkOnCleanLoad)
{
	SchemaTempDirectory dir("ModuleClean");
	dir.Write("a.schema.json", GoodFile("ok.a", "x"));
	ScopedAlias alias("vsc_clean_root", dir.Path());

	Harness h(Roots("vsc_clean_root"));
	ASSERT_TRUE(h.Start());
	EXPECT_EQ(g_module->GetHealthReporter().Report().status, Dia::Observation::Health::HealthStatus::kOK);
	auto* failed = Dia::Observation::Metric::MetricRegistry::Instance().FindGauge(StringCRC("valueschema.files_failed"));
	ASSERT_NE(failed, nullptr);
	EXPECT_DOUBLE_EQ(failed->Value(), 0.0);
}

TEST(DiaValueSchemaModule, ClamperBoundToClampCountMetric)
{
	SchemaTempDirectory dir("ModuleClamp");
	dir.Write("a.schema.json", GoodFile("cl", "rate"));
	ScopedAlias alias("vsc_clamp_root", dir.Path());

	Harness h(Roots("vsc_clamp_root"));
	ASSERT_TRUE(h.Start());
	auto* counter = Dia::Observation::Metric::MetricRegistry::Instance().FindCounter(StringCRC("valueschema.clamp.count"));
	ASSERT_NE(counter, nullptr);
	const auto before = counter->Value();

	const ValueInfo* info = g_module->GetService()->Find(StringCRC("cl/rate"));
	ASSERT_NE(info, nullptr);
	EXPECT_DOUBLE_EQ(g_module->GetClamper().Clamp(5.0, *info), 2.0);
	EXPECT_EQ(counter->Value(), before + 1);
}

TEST(DiaValueSchemaModule, ContributorEntriesRegisteredBeforePublishLaterRejected)
{
	SchemaTempDirectory dir("ModuleContributor");
	dir.Write("a.schema.json", GoodFile("file.f", "x"));
	ScopedAlias alias("vsc_contrib_root", dir.Path());

	TestContributor contributor;
	g_pendingContributor = &contributor;
	Harness h(Roots("vsc_contrib_root"));
	const bool started = h.Start();
	g_pendingContributor = nullptr;
	ASSERT_TRUE(started);

	EXPECT_EQ(contributor.mCalls, 1);
	const ValueSchemaService* svc = g_module->GetService();
	ASSERT_NE(svc, nullptr);
	EXPECT_NE(svc->Find(StringCRC("code.metrics/frame_ms")), nullptr);
	EXPECT_NE(svc->Find(StringCRC("file.f/x")), nullptr);
	EXPECT_EQ(svc->Count(), 2u);

	ASSERT_NE(contributor.mRegistry, nullptr);
	EXPECT_TRUE(contributor.mRegistry->IsPublished());
	EXPECT_EQ(contributor.mRegistry->Register(MakeValueInfo("code.metrics", "late")), RegisterResult::AlreadyPublished);
	EXPECT_EQ(svc->Count(), 2u);
}

TEST(DiaValueSchemaModule, AddContributorRejectsNull)
{
	ValueSchemaModule m(StringCRC("standalone"));
	EXPECT_FALSE(m.AddContributor(nullptr));
	TestContributor c;
	EXPECT_TRUE(m.AddContributor(&c));
	EXPECT_EQ(m.GetService(), nullptr);
}

TEST(DiaValueSchemaModule, MalformedConfigStartsEmptyAndHealthy)
{
	Harness h("not json at all");
	ASSERT_TRUE(h.Start());
	EXPECT_EQ(g_module->GetSchemaRootCount(), 0u);
	ASSERT_NE(g_module->GetService(), nullptr);
	EXPECT_EQ(g_module->GetService()->Count(), 0u);
	EXPECT_EQ(g_module->GetHealthReporter().Report().status, Dia::Observation::Health::HealthStatus::kOK);
	EXPECT_TRUE(g_consumer->mSawAvailable);
}
