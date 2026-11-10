#include <gtest/gtest.h>
#include <DiaValueSchema/ValueSchemaModule.h>
#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaValueSchema/Testing/FakeCodeSideSource.h>
#include <DiaValueSchema/Testing/SchemaFixtureBuilder.h>
#include <DiaValueSchema/Testing/SchemaTempDirectory.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/FilePath/PathStore.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaEntity/Testing/TestDescribedComponent.h>
#include <DiaEntity/ComponentTypeDesc.h>
#include <DiaCore/Reflect/FieldAttributes.h>
#include <DiaObservation/Health/IHealthReporter.h>
#include <memory>
#include <string>

// Feature 4 task 6: ValueSchemaModule runs CodeSideCollector before Publish, exposes the report,
// gauges, and the late-registration hook (AC 4, 6; Resolved Decision 2).

using namespace Dia::ApplicationFlow;
using namespace Dia::ValueSchema;
using Dia::Core::StringCRC;
using Dia::Core::PathStore;
using Dia::ValueSchema::Testing::FakeCodeSideSource;
using Dia::ValueSchema::Testing::SchemaFixtureBuilder;
using Dia::ValueSchema::Testing::SchemaTempDirectory;

namespace
{
	constexpr float kDt = 1.0f / 60.0f;

	// Every ServiceStream needs a consumer to start (mirrors TestValueSchemaModule.cpp).
	struct CsConsumerModule : Module
	{
		static const StringCRC kTypeId;
		explicit CsConsumerModule(const StringCRC& id) : Module(id) {}
		ServiceStreamReader<ValueSchemaService> mReader{ this, StringCRC(ValueSchemaModule::kServiceStreamId) };
		void OnConnectStreams(Application& app) override { mReader.Connect(app); }
		StartResult DoStart() override { return StartResult::kReady; }
		void DoUpdate(float) override {}
		StopResult DoStop() override { return StopResult::kDone; }
	};
	const StringCRC CsConsumerModule::kTypeId("CsConsumerModule");

	// Exposes the protected lifecycle so a test can run a second stop/start cycle on a connected module.
	struct TestVsc : ValueSchemaModule
	{
		using ValueSchemaModule::ValueSchemaModule;
		void CycleStop() { DoStop(); }
		void CycleStart() { DoStart(); }
	};

	TestVsc*				g_module = nullptr;
	FakeCodeSideSource*		g_pendingSource = nullptr;
	const Dia::Entity::ComponentTypeDesc* const*	g_pendingComponents = nullptr;
	unsigned int									g_pendingComponentCount = 0;
	Dia::Reflect::FieldAttributeRegistry*			g_pendingAttrs = nullptr;

	Module* CreateVsc(const StringCRC& id)
	{
		auto* m = new TestVsc(id);
		if (g_pendingSource) { m->SetCodeSideSource(g_pendingSource); }
		// Component fields: none unless a test opts in (keeps the exact-count assertions above independent of the
		// ECS components linked into this binary).
		m->SetComponentDescriptors(g_pendingComponents, g_pendingComponentCount);
		if (g_pendingAttrs) { m->SetAttributeRegistry(g_pendingAttrs); }
		g_module = m;
		return m;
	}

	ModuleDeclaration MakeModule(const StringCRC& typeId, const char* instanceId, const char* role, const char* configJson = "")
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
		mod.configJson = configJson;
		return mod;
	}

	struct Harness
	{
		TypeRegistry					mReg;
		ApplicationManifestV3			mManifest;
		std::unique_ptr<Application>	mApp;
		std::string						mConfig;

		Harness(FakeCodeSideSource* source, const std::string& configJson)
			: mConfig(configJson)
		{
			g_module = nullptr;
			g_pendingSource = source;
			mReg.Register(ValueSchemaModule::kTypeId, CreateVsc);
			mReg.Register(CsConsumerModule::kTypeId, [](const StringCRC& id) -> Module* { return new CsConsumerModule(id); });
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
			pu.modules.Add(MakeModule(ValueSchemaModule::kTypeId, "valueSchema", "provides", mConfig.c_str()));
			pu.modules.Add(MakeModule(CsConsumerModule::kTypeId, "consumer", "consumes"));
			m.processingUnits.Add(pu);
			mApp.reset(new Application(mManifest, mReg));
		}
		bool Start()
		{
			if (!mApp->Start()) return false;
			for (int i = 0; i < 10; ++i) mApp->Update(kDt);
			return g_module != nullptr;
		}
		~Harness()
		{
			g_pendingSource = nullptr;
			g_pendingComponents = nullptr;
			g_pendingComponentCount = 0;
			g_pendingAttrs = nullptr;
			if (!mApp) return;
			mApp->RequestShutdown();
			for (int guard = 0; guard < 1000 && mApp->Update(kDt); ++guard) {}
			mApp.reset();
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

	const Dia::API::ValueParam kParams[] = {
		{ "count", "int", true, "How many" },
		{ "name", "string", false, "Which one" },
	};

	double GaugeValue(const char* name)
	{
		auto* g = Dia::Observation::Metric::MetricRegistry::Instance().FindGauge(StringCRC(name));
		return g ? g->Value() : -1.0;
	}
}

TEST(DiaValueSchemaCodeSideModule, EntriesVisibleThroughServiceAfterStart)
{
	FakeCodeSideSource src;
	src.AddMetric("frame.ms", "Frame", "Frame time", "ms");
	src.AddMetric("bare.metric");
	src.AddCommand("spawn", "Game", "Spawn thing", kParams, 2);

	Harness h(&src, "{}");
	ASSERT_TRUE(h.Start());
	const ValueSchemaService* svc = g_module->GetService();
	ASSERT_NE(svc, nullptr);
	EXPECT_NE(svc->Find(StringCRC("dia.metric/frame.ms")), nullptr);
	EXPECT_NE(svc->Find(StringCRC("dia.metric/bare.metric")), nullptr);
	EXPECT_NE(svc->Find(StringCRC("dia.command/spawn")), nullptr);
	EXPECT_NE(svc->Find(StringCRC("dia.command/spawn.count")), nullptr);
	EXPECT_NE(svc->Find(StringCRC("dia.command/spawn.name")), nullptr);
}

TEST(DiaValueSchemaCodeSideModule, ReportCountsAndGauges)
{
	FakeCodeSideSource src;
	src.AddMetric("m.described", "L", "desc", "u");
	src.AddMetric("m.bare");
	src.AddCommand("cmd", "Game", "d", kParams, 2);

	Harness h(&src, "{}");
	ASSERT_TRUE(h.Start());
	const CodeSideReport& r = g_module->GetCodeSideReport();
	EXPECT_EQ(r.mMetricsCollected, 2u);
	EXPECT_EQ(r.mMetricsDescribed, 1u);
	EXPECT_EQ(r.mMetricsUndescribed, 1u);
	EXPECT_EQ(r.mCommandsCollected, 1u);
	EXPECT_EQ(r.mArgsCollected, 2u);
	EXPECT_EQ(r.mArgsDescribed, 2u);

	EXPECT_DOUBLE_EQ(GaugeValue("valueschema.metrics_described"), 1.0);
	EXPECT_DOUBLE_EQ(GaugeValue("valueschema.metrics_undescribed"), 1.0);
	EXPECT_DOUBLE_EQ(GaugeValue("valueschema.commands_described"), 1.0);
	EXPECT_DOUBLE_EQ(GaugeValue("valueschema.args_described"), 2.0);
}

TEST(DiaValueSchemaCodeSideModule, ZeroMetricsSummaryDoesNotCrash)
{
	FakeCodeSideSource src;
	Harness h(&src, "{}");
	ASSERT_TRUE(h.Start());
	ASSERT_NE(g_module->GetService(), nullptr);
	EXPECT_EQ(g_module->GetService()->Count(), 0u);
	EXPECT_EQ(g_module->GetCodeSideReport().mMetricsCollected, 0u);
	EXPECT_EQ(g_module->LateRegistrationCount(), 0u);
}

TEST(DiaValueSchemaCodeSideModule, LateRegistrationCountedNotInServiceWarnsOncePerChange)
{
	FakeCodeSideSource src;
	src.AddMetric("early");
	Harness h(&src, "{}");
	ASSERT_TRUE(h.Start());
	EXPECT_EQ(g_module->LateRegistrationCount(), 0u);
	g_module->CheckLateRegistrations();
	EXPECT_EQ(g_module->LateWarningCount(), 0u);

	src.AddMetric("late.one");
	EXPECT_EQ(g_module->LateRegistrationCount(), 1u);
	EXPECT_EQ(g_module->GetService()->Find(StringCRC("dia.metric/late.one")), nullptr);
	g_module->CheckLateRegistrations();
	EXPECT_EQ(g_module->LateWarningCount(), 1u);
	g_module->CheckLateRegistrations();
	EXPECT_EQ(g_module->LateWarningCount(), 1u);

	src.AddCommand("late.cmd", "Game", "d", nullptr, 0, true);
	EXPECT_EQ(g_module->LateRegistrationCount(), 2u);
	EXPECT_EQ(g_module->GetService()->Find(StringCRC("dia.command/late.cmd")), nullptr);
	g_module->CheckLateRegistrations();
	EXPECT_EQ(g_module->LateWarningCount(), 2u);
	g_module->CheckLateRegistrations();
	EXPECT_EQ(g_module->LateWarningCount(), 2u);
}

TEST(DiaValueSchemaCodeSideModule, DisabledByConfigCollectsNothing)
{
	FakeCodeSideSource src;
	src.AddMetric("m");
	src.AddCommand("c", "Game", "d", kParams, 2);
	Harness h(&src, R"({"code_side":false})");
	ASSERT_TRUE(h.Start());
	EXPECT_EQ(g_module->GetService()->Count(), 0u);
	EXPECT_EQ(g_module->GetCodeSideReport().mMetricsCollected, 0u);
	EXPECT_EQ(g_module->LateRegistrationCount(), 0u);
	src.AddMetric("later");
	EXPECT_EQ(g_module->LateRegistrationCount(), 0u);
}

TEST(DiaValueSchemaCodeSideModule, SidecarAndCodeSideEntriesCoexist)
{
	SchemaTempDirectory dir("CodeSideCoexist");
	dir.Write("a.schema.json", SchemaFixtureBuilder().For("file.f").Entry("x").Min(0.0).Max(2.0).Build());
	ScopedAlias alias("vsc_codeside_root", dir.Path());

	FakeCodeSideSource src;
	src.AddMetric("m.one");
	Harness h(&src, R"({"schema_roots":["vsc_codeside_root"]})");
	ASSERT_TRUE(h.Start());
	const ValueSchemaService* svc = g_module->GetService();
	ASSERT_NE(svc, nullptr);
	EXPECT_NE(svc->Find(StringCRC("file.f/x")), nullptr);
	EXPECT_NE(svc->Find(StringCRC("dia.metric/m.one")), nullptr);
	EXPECT_EQ(svc->Count(), 2u);
}

TEST(DiaValueSchemaCodeSideModule, StopStartRecollectsAgainstFreshRegistry)
{
	FakeCodeSideSource src;
	src.AddMetric("first");
	Harness h(&src, "{}");
	ASSERT_TRUE(h.Start());
	ASSERT_NE(g_module->GetService()->Find(StringCRC("dia.metric/first")), nullptr);

	// Same module object, second start cycle driven directly.
	g_module->CycleStop();
	src.AddMetric("second");
	EXPECT_EQ(g_module->LateRegistrationCount(), 1u);
	g_module->CycleStart();
	ASSERT_NE(g_module->GetService(), nullptr);
	EXPECT_NE(g_module->GetService()->Find(StringCRC("dia.metric/first")), nullptr);
	EXPECT_NE(g_module->GetService()->Find(StringCRC("dia.metric/second")), nullptr);
	EXPECT_EQ(g_module->GetCodeSideReport().mMetricsCollected, 2u);
	EXPECT_EQ(g_module->LateRegistrationCount(), 0u);
}

// --- Feature 7 tasks 5+7: component field wiring ----------------------------------------------------------------
namespace
{
	const Dia::Entity::ComponentTypeDesc* g_fixtureDescs[1];

	void UseFixtureComponent()
	{
		g_fixtureDescs[0] = &DiaEntityTesting::TestDescribedComponent::GetDesc();
		g_pendingComponents = g_fixtureDescs;
		g_pendingComponentCount = 1;
	}

	std::unique_ptr<Dia::Reflect::FieldAttributeRegistry> CopyOfAttrRegistry()
	{
		return std::unique_ptr<Dia::Reflect::FieldAttributeRegistry>(
			new Dia::Reflect::FieldAttributeRegistry(Dia::Reflect::FieldAttributeRegistry::Instance()));
	}
}

TEST(DiaValueSchemaComponentFieldModule, FieldsVisibleThroughServiceAndReportExposed)
{
	UseFixtureComponent();
	FakeCodeSideSource src;
	Harness h(&src, "{}");
	ASSERT_TRUE(h.Start());
	const ValueSchemaService* svc = g_module->GetService();
	ASSERT_NE(svc, nullptr);
	EXPECT_NE(svc->Find(StringCRC("test-described-component/speed")), nullptr);

	const ComponentFieldReport& r = g_module->GetComponentFieldReport();
	EXPECT_EQ(r.mCollected, 6u);
	EXPECT_EQ(r.mDescribed, 3u);	// speed, hitPoints, nested
	EXPECT_EQ(r.mUndescribed, 1u);	// armor (ranged, no describe)
	ASSERT_EQ(r.mUndescribedFields.Size(), 1u);
	EXPECT_STREQ(r.mUndescribedFields[0].mName.AsCStr(), "test-described-component/armor");
}

TEST(DiaValueSchemaComponentFieldModule, GaugesRegisteredWithMatchingValues)
{
	UseFixtureComponent();
	std::unique_ptr<Dia::Reflect::FieldAttributeRegistry> attrs = CopyOfAttrRegistry();
	g_pendingAttrs = attrs.get();
	FakeCodeSideSource src;
	Harness h(&src, "{}");
	ASSERT_TRUE(h.Start());
	EXPECT_DOUBLE_EQ(GaugeValue("valueschema.component_fields_described"), 3.0);
	EXPECT_DOUBLE_EQ(GaugeValue("valueschema.component_fields_undescribed"), 1.0);
	EXPECT_DOUBLE_EQ(GaugeValue("valueschema.attr_registry_entries"), static_cast<double>(attrs->EntryCount()));
	EXPECT_DOUBLE_EQ(GaugeValue("valueschema.attr_registry_dropped"), 0.0);
}

TEST(DiaValueSchemaComponentFieldModule, HealthNotDegradedWithoutDrops)
{
	UseFixtureComponent();
	std::unique_ptr<Dia::Reflect::FieldAttributeRegistry> attrs = CopyOfAttrRegistry();
	g_pendingAttrs = attrs.get();
	FakeCodeSideSource src;
	Harness h(&src, "{}");
	ASSERT_TRUE(h.Start());
	EXPECT_EQ(g_module->GetHealthReporter().Report().status, Dia::Observation::Health::HealthStatus::kOK);
}

TEST(DiaValueSchemaComponentFieldModule, DroppedRegistrationsDegradeHealthAndAreCounted)
{
	UseFixtureComponent();
	std::unique_ptr<Dia::Reflect::FieldAttributeRegistry> attrs = CopyOfAttrRegistry();
	static Dia::Reflect::RequiredAttribute attr;
	for (unsigned int i = 0; i <= Dia::Reflect::FieldAttributeList::kMaxAttrsPerField; ++i) { attrs->Register(7u, 7u, &attr); }
	ASSERT_EQ(attrs->DroppedCount(), 1u);
	g_pendingAttrs = attrs.get();
	FakeCodeSideSource src;
	Harness h(&src, "{}");
	ASSERT_TRUE(h.Start());
	EXPECT_EQ(g_module->GetHealthReporter().Report().status, Dia::Observation::Health::HealthStatus::kDegraded);
	EXPECT_EQ(g_module->GetComponentAuditReport().mDroppedCount, 1u);
	EXPECT_DOUBLE_EQ(GaugeValue("valueschema.attr_registry_dropped"), 1.0);
}

TEST(DiaValueSchemaComponentFieldModule, RepeatedStartDoesNotRewarnSamePair)
{
	UseFixtureComponent();
	std::unique_ptr<Dia::Reflect::FieldAttributeRegistry> attrs = CopyOfAttrRegistry();
	static Dia::Reflect::DescribeAttribute ghost;
	ghost.label = "Ghost";
	ghost.description = "No such field";
	attrs->Register(StringCRC("TestDescribedComponent").Value(), StringCRC("noSuchField").Value(), &ghost);
	g_pendingAttrs = attrs.get();
	FakeCodeSideSource src;
	Harness h(&src, "{}");
	ASSERT_TRUE(h.Start());
	EXPECT_EQ(g_module->GetComponentAuditReport().mUnknownFieldDescribes, 1u);
	EXPECT_EQ(g_module->GetComponentAuditReport().mNewWarnings, 1u);

	g_module->CycleStop();
	g_module->CycleStart();
	EXPECT_EQ(g_module->GetComponentAuditReport().mUnknownFieldDescribes, 1u);
	EXPECT_EQ(g_module->GetComponentAuditReport().mNewWarnings, 0u);
}

TEST(DiaValueSchemaComponentFieldModule, DisabledByCodeSideConfigCollectsNoComponents)
{
	UseFixtureComponent();
	FakeCodeSideSource src;
	Harness h(&src, R"({"code_side":false})");
	ASSERT_TRUE(h.Start());
	EXPECT_EQ(g_module->GetComponentFieldReport().mCollected, 0u);
	EXPECT_EQ(g_module->GetService()->Find(StringCRC("test-described-component/speed")), nullptr);
}
