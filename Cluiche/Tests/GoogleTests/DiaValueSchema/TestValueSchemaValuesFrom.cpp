#include <gtest/gtest.h>
#include <DiaValueSchema/ValueSchemaModule.h>
#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaValueSchema/Testing/SchemaFixtureBuilder.h>
#include <DiaValueSchema/Testing/SchemaTempDirectory.h>
#include <DiaValueSchema/Testing/FakeCodeSideSource.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/FilePath/PathStore.h>
#include <memory>
#include <string>

// valuesFrom resolution in ValueSchemaModule (Biome Registry plan task 12): the module fills an enum entry's values from a
// data file located by its `format`, in record order, before publishing.

using namespace Dia::ApplicationFlow;
using namespace Dia::ValueSchema;
using Dia::Core::StringCRC;
using Dia::Core::PathStore;
using Dia::ValueSchema::Testing::SchemaFixtureBuilder;
using Dia::ValueSchema::Testing::SchemaTempDirectory;

namespace
{
	constexpr float kDt = 1.0f / 60.0f;
	ValueSchemaModule* g_vfModule = nullptr;
	Dia::ValueSchema::Testing::FakeCodeSideSource g_vfEmptyCodeSide;

	// The manifest needs a stream consumer next to the provider.
	struct VF_ConsumerModule : Module
	{
		static const StringCRC kTypeId;
		explicit VF_ConsumerModule(const StringCRC& id) : Module(id) {}
		ServiceStreamReader<ValueSchemaService> mReader{ this, StringCRC(ValueSchemaModule::kServiceStreamId) };
		void OnConnectStreams(Application& app) override { mReader.Connect(app); }
		StartResult DoStart() override { return StartResult::kReady; }
		void DoUpdate(float) override {}
		StopResult DoStop() override { return StopResult::kDone; }
	};
	const StringCRC VF_ConsumerModule::kTypeId("VF_ConsumerModule");
	Module* CreateVfConsumer(const StringCRC& id) { return new VF_ConsumerModule(id); }

	Module* CreateVfModule(const StringCRC& id)
	{
		auto* m = new ValueSchemaModule(id);
		m->SetCodeSideSource(&g_vfEmptyCodeSide);
		m->SetComponentDescriptors(nullptr, 0);
		g_vfModule = m;
		return m;
	}

	struct VfHarness
	{
		TypeRegistry					mReg;
		ApplicationManifestV3			mManifest;
		std::unique_ptr<Application>	mApp;

		explicit VfHarness(const std::string& configJson)
		{
			g_vfModule = nullptr;
			mReg.Register(ValueSchemaModule::kTypeId, CreateVfModule);
			mReg.Register(VF_ConsumerModule::kTypeId, CreateVfConsumer);
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
			ModuleDeclaration cons;
			cons.instanceId = StringCRC("consumer");
			cons.typeId = VF_ConsumerModule::kTypeId;
			cons.startTimeoutMs = 10000.0f;
			cons.stopTimeoutMs = 5000.0f;
			cons.stages.Add(StringCRC("Boot"));
			ChannelBinding cch;
			cch.id = StringCRC(ValueSchemaModule::kServiceStreamId);
			cch.role = StringCRC("consumes");
			cons.channels.Add(cch);
			ModuleDeclaration mod;
			mod.instanceId = StringCRC("valueSchema");
			mod.typeId = ValueSchemaModule::kTypeId;
			mod.startTimeoutMs = 10000.0f;
			mod.stopTimeoutMs = 5000.0f;
			mod.stages.Add(StringCRC("Boot"));
			ChannelBinding ch;
			ch.id = StringCRC(ValueSchemaModule::kServiceStreamId);
			ch.role = StringCRC("provides");
			mod.channels.Add(ch);
			mod.configJson = configJson.c_str();
			pu.modules.Add(mod);
			pu.modules.Add(cons);
			m.processingUnits.Add(pu);
			mApp.reset(new Application(mManifest, mReg));
		}
		bool Start()
		{
			if (!mApp->Start()) return false;
			for (int i = 0; i < 10; ++i) mApp->Update(kDt);
			return g_vfModule != nullptr;
		}
		~VfHarness()
		{
			if (!mApp) return;
			mApp->RequestShutdown();
			for (int guard = 0; guard < 1000 && mApp->Update(kDt); ++guard) {}
			mApp.reset();
		}
	};

	struct VfAlias
	{
		StringCRC mAlias;
		VfAlias(const char* alias, const std::string& path) : mAlias(alias)
		{
			PathStore::RegisterToStore(mAlias, Dia::Core::Path::String(path.c_str()));
		}
		~VfAlias() { PathStore::UnregisterFromStore(mAlias); }
	};

	const char* const kThreeBiomes = R"({ "format": "cow.biomes", "version": 1, "biomes": [
	  { "id": "grassland", "name": "Grassland", "colour": "#88cc44", "description": "Open plains" },
	  { "id": "forest",    "name": "Forest",    "colour": "#226611", "description": "Dense trees" },
	  { "id": "tundra",    "name": "Tundra",    "colour": "#ddeeff", "description": "Frozen ground" } ] })";

	std::string DataSidecar() { return SchemaFixtureBuilder().For("cow.biomes").Entry("dummy").Build(); }

	// A stage sidecar with one enum entry `biomeIndex` and the given valuesFrom object text.
	std::string StageSidecar(const char* valuesFromJson)
	{
		return std::string(R"({ "format": "dia.value_schema", "version": 1, "for": "vf.stage", "entries": [
		  { "key": "biomeIndex", "kind": "field", "label": "Biome", "description": "d", "type": "enum", "valuesFrom": )")
			+ valuesFromJson + " } ] })";
	}

	const char* const kFullVf = R"({ "format": "cow.biomes", "records": "biomes", "id": "id", "label": "name", "colour": "colour", "description": "description" })";

	// Writes the data sidecar + data + stage sidecar into dir; returns nothing, the caller starts the module.
	void WriteAll(const SchemaTempDirectory& dir, const char* data, const char* valuesFromJson)
	{
		dir.Write("world/biomes.schema.json", DataSidecar());
		if (data != nullptr) { dir.Write("world/biomes.json", data); }
		dir.Write("stage/stage.schema.json", StageSidecar(valuesFromJson));
	}

	std::string RootsFor(const char* alias) { return std::string("{\"schema_roots\":[\"") + alias + "\"]}"; }

	const ValueInfo* BiomeEntry()
	{
		const ValueSchemaService* svc = g_vfModule->GetService();
		return svc ? svc->Find(StringCRC("vf.stage/biomeIndex")) : nullptr;
	}

	bool IsFailing()
	{
		const auto r = g_vfModule->GetHealthReporter().Report();
		return r.status == Dia::Observation::Health::HealthStatus::kFailing
			&& r.reason == StringCRC("valueschema.values_from_failed");
	}
}

TEST(ValueSchemaValuesFrom, ThreeRecordsResolveInOrderWithLabelsColoursDescriptions)
{
	SchemaTempDirectory dir("VfThree");
	WriteAll(dir, kThreeBiomes, kFullVf);
	VfAlias alias("vf_three", dir.Path());
	VfHarness h(RootsFor("vf_three"));
	ASSERT_TRUE(h.Start());

	const ValueInfo* e = BiomeEntry();
	ASSERT_NE(e, nullptr);
	ASSERT_EQ(e->mEnumValues.Size(), 3u);
	EXPECT_STREQ(e->mEnumValues[0].mId.AsCStr(), "grassland");
	EXPECT_STREQ(e->mEnumValues[1].mId.AsCStr(), "forest");
	EXPECT_STREQ(e->mEnumValues[2].mId.AsCStr(), "tundra");
	EXPECT_STREQ(e->mEnumValues[0].mLabel.AsCStr(), "Grassland");
	EXPECT_STREQ(e->mEnumValues[2].mLabel.AsCStr(), "Tundra");
	EXPECT_STREQ(e->mEnumValues[1].mColour.AsCStr(), "#226611");
	EXPECT_TRUE(e->mEnumValues[1].mHasColour);
	EXPECT_STREQ(e->mEnumValues[0].mDescription.AsCStr(), "Open plains");
	EXPECT_EQ(g_vfModule->GetHealthReporter().Report().status, Dia::Observation::Health::HealthStatus::kOK);
}

TEST(ValueSchemaValuesFrom, DataFileMissingFailsAndPublishesNothingForTheEntry)
{
	SchemaTempDirectory dir("VfNoFile");
	WriteAll(dir, nullptr, kFullVf);
	VfAlias alias("vf_nofile", dir.Path());
	VfHarness h(RootsFor("vf_nofile"));
	ASSERT_TRUE(h.Start());
	EXPECT_TRUE(IsFailing());
	EXPECT_EQ(BiomeEntry(), nullptr);
}

TEST(ValueSchemaValuesFrom, RecordsKeyMissingFails)
{
	SchemaTempDirectory dir("VfNoRecords");
	WriteAll(dir, R"({ "format": "cow.biomes", "version": 1 })", kFullVf);
	VfAlias alias("vf_norec", dir.Path());
	VfHarness h(RootsFor("vf_norec"));
	ASSERT_TRUE(h.Start());
	EXPECT_TRUE(IsFailing());
	EXPECT_EQ(BiomeEntry(), nullptr);
}

TEST(ValueSchemaValuesFrom, RecordsNotAnArrayFails)
{
	SchemaTempDirectory dir("VfRecordsObj");
	WriteAll(dir, R"({ "format": "cow.biomes", "version": 1, "biomes": { "id": "x" } })", kFullVf);
	VfAlias alias("vf_recobj", dir.Path());
	VfHarness h(RootsFor("vf_recobj"));
	ASSERT_TRUE(h.Start());
	EXPECT_TRUE(IsFailing());
	EXPECT_EQ(BiomeEntry(), nullptr);
}

TEST(ValueSchemaValuesFrom, RecordMissingIdFails)
{
	SchemaTempDirectory dir("VfNoId");
	WriteAll(dir, R"({ "format": "cow.biomes", "version": 1, "biomes": [
	  { "id": "a", "name": "A", "colour": "#111111", "description": "d" },
	  { "name": "B", "colour": "#222222", "description": "d" } ] })", kFullVf);
	VfAlias alias("vf_noid", dir.Path());
	VfHarness h(RootsFor("vf_noid"));
	ASSERT_TRUE(h.Start());
	EXPECT_TRUE(IsFailing());
	EXPECT_EQ(BiomeEntry(), nullptr);
}

TEST(ValueSchemaValuesFrom, RecordMissingConfiguredLabelFails)
{
	SchemaTempDirectory dir("VfNoLabel");
	WriteAll(dir, R"({ "format": "cow.biomes", "version": 1, "biomes": [
	  { "id": "a", "name": "A", "colour": "#111111", "description": "d" },
	  { "id": "b", "colour": "#222222", "description": "d" } ] })", kFullVf);
	VfAlias alias("vf_nolabel", dir.Path());
	VfHarness h(RootsFor("vf_nolabel"));
	ASSERT_TRUE(h.Start());
	EXPECT_TRUE(IsFailing());
	EXPECT_EQ(BiomeEntry(), nullptr);
}

TEST(ValueSchemaValuesFrom, OtherEntriesStillPublishWhenOneFails)
{
	SchemaTempDirectory dir("VfOthers");
	WriteAll(dir, nullptr, kFullVf);
	dir.Write("other/o.schema.json", SchemaFixtureBuilder().For("vf.other").Entry("rate").Build());
	VfAlias alias("vf_others", dir.Path());
	VfHarness h(RootsFor("vf_others"));
	ASSERT_TRUE(h.Start());
	EXPECT_TRUE(IsFailing());
	EXPECT_EQ(BiomeEntry(), nullptr);
	EXPECT_NE(g_vfModule->GetService()->Find(StringCRC("vf.other/rate")), nullptr);
}

TEST(ValueSchemaValuesFrom, OptionalSubkeysOmittedLeaveLabelDescriptionEmptyAndColourDefaulted)
{
	SchemaTempDirectory dir("VfOptional");
	WriteAll(dir, kThreeBiomes, R"({ "format": "cow.biomes", "records": "biomes", "id": "id" })");
	VfAlias alias("vf_opt", dir.Path());
	VfHarness h(RootsFor("vf_opt"));
	ASSERT_TRUE(h.Start());

	const ValueInfo* e = BiomeEntry();
	ASSERT_NE(e, nullptr);
	ASSERT_EQ(e->mEnumValues.Size(), 3u);
	EXPECT_STREQ(e->mEnumValues[1].mId.AsCStr(), "forest");
	EXPECT_STREQ(e->mEnumValues[1].mLabel.AsCStr(), "");
	EXPECT_STREQ(e->mEnumValues[1].mDescription.AsCStr(), "");
	EXPECT_FALSE(e->mEnumValues[1].mHasColour);	// categorical default colour, like a literal value without "colour"
	EXPECT_GT(e->mEnumValues[1].mColour.Length(), 0u);
	EXPECT_EQ(g_vfModule->GetHealthReporter().Report().status, Dia::Observation::Health::HealthStatus::kOK);
}

TEST(ValueSchemaValuesFrom, EmptyRecordsResolveToZeroValuesWithoutError)
{
	SchemaTempDirectory dir("VfEmpty");
	WriteAll(dir, R"({ "format": "cow.biomes", "version": 1, "biomes": [] })", kFullVf);
	VfAlias alias("vf_empty", dir.Path());
	VfHarness h(RootsFor("vf_empty"));
	ASSERT_TRUE(h.Start());

	const ValueInfo* e = BiomeEntry();
	ASSERT_NE(e, nullptr);
	EXPECT_EQ(e->mEnumValues.Size(), 0u);
	EXPECT_EQ(g_vfModule->GetHealthReporter().Report().status, Dia::Observation::Health::HealthStatus::kOK);
}
