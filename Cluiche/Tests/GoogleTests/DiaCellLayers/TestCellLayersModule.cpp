#include <gtest/gtest.h>
#include <DiaCellLayers/CellLayersModule.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaStreams/ServiceStreamWriter.h>
#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaCore/CRC/StringCRC.h>
#include <memory>

// CellLayersModule lifecycle through a live single-PU Application (pattern of TestValueSchemaModule):
// start publishes the registry, a stream consumer gets the same registry, stop withdraws it.

using namespace Dia::ApplicationFlow;
using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;
using Dia::Graphics::CellGridDesc;

namespace
{
	constexpr float kDt = 1.0f / 60.0f;

	struct CLM_ConsumerModule : Module
	{
		static const StringCRC kTypeId;
		explicit CLM_ConsumerModule(const StringCRC& id) : Module(id) {}

		ServiceStreamReader<CellLayerRegistry> mReader{ this, StringCRC(CellLayersModule::kServiceStreamId) };
		CellLayerRegistry*	mSeen = nullptr;
		bool				mRegisteredOk = false;
		CellGridDesc		mGrid;

		void OnConnectStreams(Application& app) override { mReader.Connect(app); }
		StartResult DoStart() override { return StartResult::kReady; }
		// The store is committed after every provider's DoStart, so a same-PU consumer resolves it on its first update.
		void DoUpdate(float) override
		{
			if (mSeen) { return; }
			mSeen = FindCellLayerRegistry(mReader);
			if (mSeen) { mRegisteredOk = mSeen->Register(MakeLayer("consumer.layer", &mGrid)) == CellRegisterResult::Ok; }
		}
		StopResult DoStop() override { return StopResult::kDone; }
	};
	const StringCRC CLM_ConsumerModule::kTypeId("CLM_ConsumerModule");

	// Stands in for ValueSchemaModule: publishes a fixture-built ValueSchemaService on the "ValueSchemaService" stream.
	Dia::ValueSchema::ValueSchemaService* g_schemaToPublish = nullptr;
	struct CLM_SchemaProviderModule : Module
	{
		static const StringCRC kTypeId;
		explicit CLM_SchemaProviderModule(const StringCRC& id) : Module(id) {}
		ServiceStreamWriter<Dia::ValueSchema::ValueSchemaService> mWriter{ this, StringCRC(CellLayersModule::kSchemaStreamId) };
		void OnConnectStreams(Application& app) override { mWriter.Connect(app); }
		StartResult DoStart() override
		{
			if (!g_schemaToPublish)
			{
				static UploaderSchemaFixture sEmpty;	// no entries: no layer has ui.default_base
				if (!sEmpty.service) { sEmpty.Publish(); }
				g_schemaToPublish = sEmpty.service;
				mUsedEmpty = true;
			}
			mWriter.Register(*g_schemaToPublish);
			return StartResult::kReady;
		}
		bool mUsedEmpty = false;
		void DoUpdate(float) override {}
		StopResult DoStop() override { mWriter.Deregister(); return StopResult::kDone; }
	};
	const StringCRC CLM_SchemaProviderModule::kTypeId("CLM_SchemaProviderModule");
	Module* CreateSchemaProvider(const StringCRC& id) { return new CLM_SchemaProviderModule(id); }

	bool g_availableAfterStop = true;
	bool g_stopped = false;

	// Observes the stream from inside the provider: availability right after its own DoStart/DoStop.
	struct ObservedCellLayersModule : CellLayersModule
	{
		explicit ObservedCellLayersModule(const StringCRC& id) : CellLayersModule(id) {}
		ServiceStreamReader<CellLayerRegistry> mSelfReader{ this, StringCRC(CellLayersModule::kServiceStreamId) };
		void OnConnectStreams(Application& app) override { CellLayersModule::OnConnectStreams(app); mSelfReader.Connect(app); }
		StopResult DoStop() override
		{
			StopResult r = CellLayersModule::DoStop();
			g_availableAfterStop = mSelfReader.IsAvailable();
			g_stopped = true;
			return r;
		}
	};

	CellLayersModule*		g_module = nullptr;
	ObservedCellLayersModule* g_observed = nullptr;
	CLM_ConsumerModule*		g_consumer = nullptr;

	Module* CreateModule(const StringCRC& id)
	{
		auto* m = new ObservedCellLayersModule(id);
		m->SetCurrentPuIdFn(&FakePuIdFn);
		g_module = m;
		g_observed = m;
		return m;
	}
	Module* CreateConsumer(const StringCRC& id) { auto* m = new CLM_ConsumerModule(id); g_consumer = m; return m; }

	ModuleDeclaration MakeModule(const StringCRC& typeId, const char* instanceId, const char* role)
	{
		ModuleDeclaration mod;
		mod.instanceId = StringCRC(instanceId);
		mod.typeId = typeId;
		mod.startTimeoutMs = 10000.0f;
		mod.stopTimeoutMs = 5000.0f;
		mod.stages.Add(StringCRC("Boot"));
		ChannelBinding ch;
		ch.id = StringCRC(CellLayersModule::kServiceStreamId);
		ch.role = StringCRC(role);
		mod.channels.Add(ch);
		return mod;
	}

	ApplicationManifestV3 BuildManifest()
	{
		ApplicationManifestV3 m;
		m.version = 3;
		StageDeclaration boot;
		boot.name = StringCRC("Boot");
		m.stages.Add(boot);
		m.initialStage = StringCRC("Boot");

		StreamDeclaration svc;
		svc.id = StringCRC(CellLayersModule::kServiceStreamId);
		svc.kind = StringCRC("ServiceStream");
		svc.payloadType = StringCRC("CellLayerRegistry");
		m.streams.Add(svc);

		ProcessingUnitDeclaration pu;
		pu.instanceId = StringCRC("MainPU");
		pu.frequencyHz = 60.0f;
		pu.dedicatedThread = false;
		// The module always consumes the schema stream: the manifest must declare it and a provider must exist
		// (undeclared streams assert, providerless streams fail validation). Default provider publishes an empty schema.
		StreamDeclaration schemaStream;
		schemaStream.id = StringCRC(CellLayersModule::kSchemaStreamId);
		schemaStream.kind = StringCRC("ServiceStream");
		schemaStream.payloadType = StringCRC("ValueSchemaService");
		m.streams.Add(schemaStream);
		{
			ModuleDeclaration provider = MakeModule(CLM_SchemaProviderModule::kTypeId, "schemaProvider", "provides");
			provider.channels[0].id = StringCRC(CellLayersModule::kSchemaStreamId);
			pu.modules.Add(provider);
		}
		ModuleDeclaration cl = MakeModule(CellLayersModule::kTypeId, "cellLayers", "provides");
		ChannelBinding schemaCh;
		schemaCh.id = StringCRC(CellLayersModule::kSchemaStreamId);
		schemaCh.role = StringCRC("consumes");
		cl.channels.Add(schemaCh);
		pu.modules.Add(cl);
		pu.modules.Add(MakeModule(CLM_ConsumerModule::kTypeId, "consumer", "consumes"));
		m.processingUnits.Add(pu);
		return m;
	}

	struct Harness
	{
		TypeRegistry					mReg;
		ApplicationManifestV3			mManifest;
		std::unique_ptr<Application>	mApp;

		Harness()
		{
			g_module = nullptr;
			g_consumer = nullptr;
			FakePuId() = 1;
			mReg.Register(CellLayersModule::kTypeId, CreateModule);
			mReg.Register(CLM_ConsumerModule::kTypeId, CreateConsumer);
			mReg.Register(CLM_SchemaProviderModule::kTypeId, CreateSchemaProvider);
			mManifest = BuildManifest();
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
}

TEST(CellLayersModule, StartPublishesRegistry)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	ASSERT_NE(g_module->GetRegistry(), nullptr);
	EXPECT_TRUE(g_consumer->mReader.IsAvailable());
	EXPECT_EQ(&g_consumer->mReader.Get(), g_module->GetRegistry());
}

TEST(CellLayersModule, ConsumerGetsSameRegistryAndRegistrationIsVisibleToModule)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	ASSERT_NE(g_consumer->mSeen, nullptr) << "FindCellLayerRegistry returned null in the consumer's DoUpdate";
	EXPECT_EQ(g_consumer->mSeen, g_module->GetRegistry());
	EXPECT_TRUE(g_consumer->mRegisteredOk);
	EXPECT_EQ(g_module->GetRegistry()->LayerCount(), 1u);
	EXPECT_NE(g_module->GetRegistry()->Find(StringCRC("consumer.layer")), nullptr);
}

TEST(CellLayersModule, StopWithdrawsStream)
{
	g_stopped = false;
	g_availableAfterStop = true;
	{
		Harness h;
		ASSERT_TRUE(h.Start());
		ASSERT_TRUE(g_consumer->mReader.IsAvailable());
		h.Shutdown();	// modules are destroyed here; the provider recorded the stream state in its own DoStop
	}
	EXPECT_TRUE(g_stopped);
	EXPECT_FALSE(g_availableAfterStop) << "the stream must be withdrawn once the provider has stopped";
}

TEST(CellLayersModule, WrongProcessingUnitRegistrationRejected)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	CellGridDesc grid;
	FakePuId() = 2;	// a different PU than the one that started the module
	EXPECT_EQ(g_module->GetRegistry()->Register(MakeLayer("other.pu", &grid)), CellRegisterResult::WrongProcessingUnit);
	FakePuId() = 1;
}

TEST(CellLayersModule, FindCellLayerRegistryNullWhenNotConnected)
{
	CLM_ConsumerModule consumer(StringCRC("lonely"));
	EXPECT_EQ(FindCellLayerRegistry(consumer.mReader), nullptr);
}

namespace
{
	// Each Harness is large; run them in separate stack frames.
	struct RunResult { uint32_t layerCount = 0; bool hasExtra = false; };
	RunResult RunApp(bool registerExtra)
	{
		RunResult out;
		Harness h;
		if (!h.Start()) { return out; }
		CellGridDesc grid;
		if (registerExtra) { g_module->GetRegistry()->Register(MakeLayer("extra", &grid)); }
		out.layerCount = g_module->GetRegistry()->LayerCount();
		out.hasExtra = g_module->GetRegistry()->Find(StringCRC("extra")) != nullptr;
		return out;
	}
}

TEST(CellLayersModule, RegistrationsDoNotSurviveAnApplicationRestart)
{
	const RunResult first = RunApp(true);
	EXPECT_EQ(first.layerCount, 2u);	// consumer.layer + extra
	EXPECT_TRUE(first.hasExtra);
	const RunResult second = RunApp(false);
	EXPECT_EQ(second.layerCount, 1u);	// only the consumer's own layer
	EXPECT_FALSE(second.hasExtra);
}

TEST(CellLayersModule, ConsumerSeesNoRegistryBeforeFirstUpdate)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	EXPECT_EQ(g_module->GetRegistry()->OverlayCount(), 0u);
	EXPECT_EQ(g_module->GetRegistry()->GridCount(), 1u);
	EXPECT_TRUE(g_module->GetRegistry()->IsLive());
}

// Gap fix: a manifest-built CellLayersModule consumes the ValueSchemaService stream itself, so ui.default_base is honoured.
TEST(CellLayersModule, ManifestSchemaStreamMakesUiDefaultBaseWin)
{
	UploaderSchemaFixture fx;
	auto temp = UploaderSchemaFixture::FloatEntry("temp", 0.0, 1.0);
	auto rain = UploaderSchemaFixture::FloatEntry("rain", 0.0, 1.0);
	rain.mUi["default_base"] = true;
	fx.Add(temp);
	fx.Add(rain);
	fx.Publish();
	g_schemaToPublish = fx.service;
	{
		Harness h;
		ASSERT_TRUE(h.Start());
		CellGridDesc grid;
		CellLayerRegistry* reg = g_module->GetRegistry();
		ASSERT_NE(reg, nullptr);
		// temp registered first; the schema marks rain as the default base.
		ASSERT_EQ(reg->Register(MakeLayer("temp", &grid, "cells/temp")), CellRegisterResult::Ok);
		ASSERT_EQ(reg->Register(MakeLayer("rain", &grid, "cells/rain")), CellRegisterResult::Ok);
		ASSERT_NE(reg->DefaultBase(&grid), nullptr);
		EXPECT_EQ(reg->DefaultBase(&grid)->id, StringCRC("rain"));
	}
	g_schemaToPublish = nullptr;
}

TEST(CellLayersModule, SchemaWithoutDefaultBaseFlagFallsBackToFirstRegistered)
{
	g_schemaToPublish = nullptr;
	Harness h;
	ASSERT_TRUE(h.Start());
	CellGridDesc grid;
	CellLayerRegistry* reg = g_module->GetRegistry();
	ASSERT_EQ(reg->Register(MakeLayer("first", &grid)), CellRegisterResult::Ok);
	ASSERT_EQ(reg->Register(MakeLayer("second", &grid)), CellRegisterResult::Ok);
	EXPECT_EQ(reg->DefaultBase(&grid)->id, StringCRC("first"));
}
