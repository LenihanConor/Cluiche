#include <gtest/gtest.h>
#include <DiaValueSchema/RampSetLoader.h>
#include <DiaValueSchema/RampRegistry.h>
#include <DiaValueSchema/ValueSchemaModule.h>
#include <DiaValueSchema/ValueSchemaService.h>
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
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

// Feature 5 task 3: RampSetLoader (no Application) and ValueSchemaModule ramp loading / FindRamp / health / metrics.
// Health decision: only a whole-file failure degrades health; a skipped single ramp is logged and counted in
// rampsRejected but leaves health OK.

using namespace Dia::ApplicationFlow;
using namespace Dia::ValueSchema;
using Dia::Core::StringCRC;
using Dia::Core::PathStore;
using Dia::ValueSchema::Testing::SchemaTempDirectory;
namespace fs = std::filesystem;

namespace
{
	constexpr float kDt = 1.0f / 60.0f;

	std::string RampJson(const char* id, const char* c0, const char* c1)
	{
		return std::string(R"({"id":")") + id + R"(","kind":"continuous","stops":[{"t":0.0,"colour":")" + c0 +
			R"("},{"t":1.0,"colour":")" + c1 + R"("}]})";
	}
	std::string RampFile(const std::string& ramps)
	{
		return std::string(R"({"format":"dia.ramps","version":1,"ramps":[)") + ramps + "]}";
	}

	std::string ReadShippedRamps()
	{
		std::error_code ec;
		fs::path dir = fs::absolute(fs::path(__FILE__), ec).parent_path();
		while (!dir.empty())
		{
			const fs::path candidate = dir / "Dia" / "DiaValueSchema" / "Data" / "ramps.json";
			if (fs::exists(candidate, ec))
			{
				std::ifstream in(candidate, std::ios::binary);
				std::stringstream ss;
				ss << in.rdbuf();
				return ss.str();
			}
			const fs::path parent = dir.parent_path();
			if (parent == dir) { break; }
			dir = parent;
		}
		return std::string();
	}

	// ---- Module harness (mirrors TestValueSchemaModule.cpp) ----
	struct RampConsumerModule : Module
	{
		static const StringCRC kTypeId;
		explicit RampConsumerModule(const StringCRC& id) : Module(id) {}
		ServiceStreamReader<ValueSchemaService> mReader{ this, StringCRC(ValueSchemaModule::kServiceStreamId) };
		bool			mSawGreen = false;
		bool			mSawUnknown = false;
		unsigned int	mRampCount = 0;
		void OnConnectStreams(Application& app) override { mReader.Connect(app); }
		StartResult DoStart() override { Observe(); return StartResult::kReady; }
		void DoUpdate(float) override { Observe(); }
		StopResult DoStop() override { return StopResult::kDone; }
		void Observe()
		{
			if (!mReader.IsAvailable()) { return; }
			const ValueSchemaService& s = mReader.Get();
			mSawGreen = s.FindRamp(StringCRC("sequential.green")) != nullptr;
			mSawUnknown = s.FindRamp(StringCRC("no.such.ramp")) != nullptr;
			mRampCount = s.RampCount();
		}
	};
	const StringCRC RampConsumerModule::kTypeId("RampConsumerModule");

	ValueSchemaModule*		g_module = nullptr;
	RampConsumerModule*		g_consumer = nullptr;
	Module* CreateVsc(const StringCRC& id) { auto* m = new ValueSchemaModule(id); g_module = m; return m; }
	Module* CreateConsumer(const StringCRC& id) { auto* m = new RampConsumerModule(id); g_consumer = m; return m; }

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

		explicit Harness(const std::string& configJson)
		{
			g_module = nullptr;
			g_consumer = nullptr;
			mReg.Register(ValueSchemaModule::kTypeId, CreateVsc);
			mReg.Register(RampConsumerModule::kTypeId, CreateConsumer);
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
			vsc.configJson = configJson.c_str();
			pu.modules.Add(vsc);
			pu.modules.Add(MakeModule(RampConsumerModule::kTypeId, "consumer", "consumes"));
			m.processingUnits.Add(pu);
			mApp.reset(new Application(mManifest, mReg));
		}
		bool Start()
		{
			if (!mApp->Start()) return false;
			for (int i = 0; i < 10; ++i) mApp->Update(kDt);
			return g_module != nullptr && g_consumer != nullptr;
		}
		~Harness()
		{
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

	std::string Config(const char* schemaAlias, const char* rampAlias)
	{
		std::string s = "{";
		if (schemaAlias) { s += std::string("\"schema_roots\":[\"") + schemaAlias + "\"]"; }
		if (rampAlias) { s += std::string(schemaAlias ? "," : "") + "\"ramp_roots\":[\"" + rampAlias + "\"]"; }
		return s + "}";
	}
}

// ---- RampSetLoader ----

TEST(DiaValueSchemaRampSetLoader, GameRampClashingWithDiaRejectedDiaColoursPreserved)
{
	SchemaTempDirectory dia("RampSetDia"), game("RampSetGame");
	dia.Write("ramps.json", RampFile(RampJson("shared.id", "#112233", "#445566")));
	game.Write("g.ramps.json", RampFile(RampJson("shared.id", "#aaaaaa", "#bbbbbb") + "," + RampJson("game.only", "#000000", "#ffffff")));

	RampSetLoader loader;
	loader.AddGameRoot(game.Path().c_str());	// order of Add must not matter: Dia always registers first
	loader.AddDiaRoot(dia.Path().c_str());
	RampRegistry reg;
	const RampLoadStats stats = loader.Load(reg);
	reg.Publish();

	EXPECT_EQ(stats.mFilesFound, 2u);
	EXPECT_EQ(stats.mFilesLoaded, 2u);
	EXPECT_EQ(stats.mFilesFailed, 0u);
	EXPECT_EQ(stats.mRampsLoaded, 2u);
	EXPECT_EQ(stats.mRampsRejected, 1u);
	const RampData* r = reg.Find(StringCRC("shared.id"));
	ASSERT_NE(r, nullptr);
	EXPECT_EQ(r->mStops[0].mRgba8, 0x112233FFu);
	EXPECT_EQ(r->mStops[1].mRgba8, 0x445566FFu);
	EXPECT_NE(reg.Find(StringCRC("game.only")), nullptr);
}

TEST(DiaValueSchemaRampSetLoader, TwoGameFilesSameIdSortedFirstWins)
{
	SchemaTempDirectory game("RampSetSorted");
	game.Write("b/b.ramps.json", RampFile(RampJson("dup", "#222222", "#333333")));
	game.Write("a/a.ramps.json", RampFile(RampJson("dup", "#010101", "#020202")));

	RampSetLoader loader;
	loader.AddGameRoot(game.Path().c_str());
	RampRegistry reg;
	const RampLoadStats stats = loader.Load(reg);
	EXPECT_EQ(stats.mRampsLoaded, 1u);
	EXPECT_EQ(stats.mRampsRejected, 1u);
	EXPECT_EQ(reg.Find(StringCRC("dup"))->mStops[0].mRgba8, 0x010101FFu);
}

TEST(DiaValueSchemaRampSetLoader, BadFileFailsOnlyItself)
{
	SchemaTempDirectory game("RampSetBad");
	game.Write("a.ramps.json", "{ broken");
	game.Write("b.ramps.json", RampFile(RampJson("ok", "#000000", "#ffffff")));
	game.Write("c.schema.json", "ignored");

	RampSetLoader loader;
	loader.AddGameRoot(game.Path().c_str());
	RampRegistry reg;
	const RampLoadStats stats = loader.Load(reg);
	EXPECT_EQ(stats.mFilesFound, 2u);
	EXPECT_EQ(stats.mFilesFailed, 1u);
	EXPECT_EQ(stats.mFilesLoaded, 1u);
	EXPECT_NE(reg.Find(StringCRC("ok")), nullptr);
}

TEST(DiaValueSchemaRampSetLoader, DiaRootOnlyLoadsFileNamedRampsJson)
{
	SchemaTempDirectory dia("RampSetDiaName");
	dia.Write("other.ramps.json", RampFile(RampJson("not.dia", "#000000", "#ffffff")));
	RampSetLoader loader;
	loader.AddDiaRoot(dia.Path().c_str());
	RampRegistry reg;
	const RampLoadStats stats = loader.Load(reg);
	EXPECT_EQ(stats.mFilesFound, 0u);
	EXPECT_EQ(reg.Count(), 0u);
}

TEST(DiaValueSchemaRampSetLoader, BadRampInsideFileCountsRejectedNotFailed)
{
	SchemaTempDirectory game("RampSetOneBad");
	game.Write("a.ramps.json", RampFile(R"({"id":"bad","kind":"continuous","stops":[]},)" + RampJson("good", "#000000", "#ffffff")));
	RampSetLoader loader;
	loader.AddGameRoot(game.Path().c_str());
	RampRegistry reg;
	const RampLoadStats stats = loader.Load(reg);
	EXPECT_EQ(stats.mFilesFailed, 0u);
	EXPECT_EQ(stats.mFilesLoaded, 1u);
	EXPECT_EQ(stats.mRampsLoaded, 1u);
	EXPECT_EQ(stats.mRampsRejected, 1u);
}

// ---- ValueSchemaService ----

TEST(DiaValueSchemaRampService, FindRampWithoutRegistryIsNull)
{
	ValueSchemaRegistry schemas;
	schemas.Publish();
	ValueSchemaService svc(schemas);
	EXPECT_EQ(svc.FindRamp(StringCRC("x")), nullptr);
	EXPECT_EQ(svc.RampCount(), 0u);
}

// ---- Module ----

TEST(DiaValueSchemaRampModule, RealShippedRampsLoadedThroughRampRoots)
{
	const std::string shipped = ReadShippedRamps();
	ASSERT_FALSE(shipped.empty());
	SchemaTempDirectory dir("RampModuleDia");
	dir.Write("ramps.json", shipped);
	ScopedAlias alias("vsc_ramp_dia_data", dir.Path());

	Harness h(Config(nullptr, "vsc_ramp_dia_data"));
	ASSERT_TRUE(h.Start());

	const ValueSchemaService* svc = g_module->GetService();
	ASSERT_NE(svc, nullptr);
	EXPECT_EQ(svc->RampCount(), 8u);
	for (const char* id : { "sequential.green", "sequential.blue", "sequential.heat", "sequential.grey", "sequential.purple",
		"sequential.terrain", "diverging.blue_red", "categorical.12" })
	{
		EXPECT_NE(svc->FindRamp(StringCRC(id)), nullptr) << id;
	}
	EXPECT_EQ(svc->FindRamp(StringCRC("no.such.ramp")), nullptr);
	EXPECT_TRUE(g_consumer->mSawGreen);
	EXPECT_FALSE(g_consumer->mSawUnknown);
	EXPECT_EQ(g_consumer->mRampCount, 8u);

	auto* loaded = Dia::Observation::Metric::MetricRegistry::Instance().FindGauge(StringCRC("valueschema.ramps_loaded"));
	auto* failed = Dia::Observation::Metric::MetricRegistry::Instance().FindGauge(StringCRC("valueschema.ramp_files_failed"));
	ASSERT_NE(loaded, nullptr);
	ASSERT_NE(failed, nullptr);
	EXPECT_DOUBLE_EQ(loaded->Value(), 8.0);
	EXPECT_DOUBLE_EQ(failed->Value(), 0.0);
	EXPECT_EQ(g_module->GetHealthReporter().Report().status, Dia::Observation::Health::HealthStatus::kOK);
}

TEST(DiaValueSchemaRampModule, NoRampRootsMeansNoDiaRampsAndNoError)
{
	Harness h("{}");
	ASSERT_TRUE(h.Start());
	ASSERT_NE(g_module->GetService(), nullptr);
	EXPECT_EQ(g_module->GetService()->RampCount(), 0u);
	EXPECT_EQ(g_module->GetService()->FindRamp(StringCRC("sequential.green")), nullptr);
	EXPECT_FALSE(g_consumer->mSawGreen);
	EXPECT_EQ(g_module->GetHealthReporter().Report().status, Dia::Observation::Health::HealthStatus::kOK);
}

TEST(DiaValueSchemaRampModule, GameRampsFoundUnderSchemaRootsAndBadFileDegradesHealth)
{
	SchemaTempDirectory dia("RampModuleDia2"), game("RampModuleGame");
	dia.Write("ramps.json", RampFile(RampJson("sequential.green", "#112233", "#445566")));
	game.Write("a.ramps.json", "{ broken");
	game.Write("sub/b.ramps.json", RampFile(RampJson("sequential.green", "#aaaaaa", "#bbbbbb") + "," + RampJson("game.ramp", "#000000", "#ffffff")));
	ScopedAlias a1("vsc_ramp_dia2", dia.Path());
	ScopedAlias a2("vsc_ramp_game", game.Path());

	Harness h(Config("vsc_ramp_game", "vsc_ramp_dia2"));
	ASSERT_TRUE(h.Start());

	const ValueSchemaService* svc = g_module->GetService();
	ASSERT_NE(svc, nullptr);
	EXPECT_EQ(svc->RampCount(), 2u);
	ASSERT_NE(svc->FindRamp(StringCRC("sequential.green")), nullptr);
	EXPECT_EQ(svc->FindRamp(StringCRC("sequential.green"))->mStops[0].mRgba8, 0x112233FFu);	// Dia wins
	EXPECT_NE(svc->FindRamp(StringCRC("game.ramp")), nullptr);

	EXPECT_EQ(g_module->GetHealthReporter().Report().status, Dia::Observation::Health::HealthStatus::kDegraded);
	auto* failed = Dia::Observation::Metric::MetricRegistry::Instance().FindGauge(StringCRC("valueschema.ramp_files_failed"));
	ASSERT_NE(failed, nullptr);
	EXPECT_DOUBLE_EQ(failed->Value(), 1.0);
}
