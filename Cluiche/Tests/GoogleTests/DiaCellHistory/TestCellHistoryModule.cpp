#include "HistoryTestRig.h"
#include <DiaCellHistory/CellHistoryModule.h>
#include <DiaCellLayers/CellLayersModule.h>
#include <DiaCellLayers/Testing/FakeMirrorRegistrar.h>
#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaStreams/IStreamConnector.h>

#include <memory>
#include <vector>

// CellHistoryModule wiring without an Application: the module is driven through a thin subclass that exposes the
// protected lifecycle, with a fake IStreamConnector standing in for the Application's stream store.

using namespace Dia::CellHistory;
using namespace HistoryRig;
using Dia::ApplicationFlow::IStreamStore;
using Dia::CellLayers::Testing::LogCapture;
using Dia::Core::StringCRC;

namespace
{
	class FakeConnector : public Dia::ApplicationFlow::IStreamConnector
	{
	public:
		IStreamStore* RegisterOrFindStreamStore(Dia::Core::UniquePtr<IStreamStore> newStore) override
		{
			for (auto& s : mStores) { if (s->GetId() == newStore->GetId()) { return s.Get(); } }
			IStreamStore* raw = newStore.Get();
			mStores.push_back(std::move(newStore));
			return raw;
		}
		// Stands in for Application::CommitReadyServiceStreams.
		void CommitReady()
		{
			for (auto& s : mStores) { if (s->IsRegistered() && !s->IsCommitted()) { s->Commit(); } }
		}
	private:
		std::vector<Dia::Core::UniquePtr<IStreamStore>> mStores;
	};

	struct TestModule : CellHistoryModule
	{
		explicit TestModule(const char* id = "history") : CellHistoryModule(StringCRC(id)) {}
		using CellHistoryModule::OnConfigure;
		void Start() { ASSERT_EQ(DoStart(), Dia::ApplicationFlow::StartResult::kReady); }
		void Update() { DoUpdate(0.016f); }
		void Stop() { DoStop(); }
	};

	struct ModuleRig : Rig
	{
		FakeConnector connector;
		TestModule module;
		Dia::ApplicationFlow::ServiceStreamReader<CellHistoryRecorder> reader{ nullptr, StringCRC(CellHistoryModule::kServiceStreamId) };

		void Wire()
		{
			grid.AddFloatLayer("temp", 40.0f);
			ASSERT_TRUE(schema.Load({ FloatEntry("temp", 0, 100) }));
			RegisterWithKeys(grid, registry);
			module.SetRegistry(&registry);
			module.SetSchema(schema.service.get());
			module.ConnectStreams(connector);
			reader.Connect(connector);
		}
		void Run() { module.Start(); module.Update(); connector.CommitReady(); }
	};
}

TEST(CellHistoryModule, NoRecorderUntilRegistryAvailable)
{
	FakeConnector connector;
	TestModule module;
	module.ConnectStreams(connector);
	Dia::ApplicationFlow::ServiceStreamReader<CellHistoryRecorder> reader{ nullptr, StringCRC(CellHistoryModule::kServiceStreamId) };
	reader.Connect(connector);

	module.Start();
	module.Update();
	connector.CommitReady();
	EXPECT_EQ(module.GetRecorder(), nullptr);
	EXPECT_EQ(FindCellHistoryRecorder(reader), nullptr);
}

TEST(CellHistoryModule, CreatesRecorderOnceRegistryIsAvailable)
{
	ModuleRig r;
	r.Wire();
	r.module.Start();
	EXPECT_EQ(r.module.GetRecorder(), nullptr);	// the stores commit after DoStart, so nothing is resolved yet
	r.module.Update();
	ASSERT_NE(r.module.GetRecorder(), nullptr);
	CellHistoryRecorder* first = r.module.GetRecorder();
	r.module.Update();
	EXPECT_EQ(r.module.GetRecorder(), first);	// created once
}

TEST(CellHistoryModule, ResolvesRegistryFromTheStream)
{
	FakeConnector connector;
	Dia::CellLayers::CellLayerRegistry registry;
	Dia::ApplicationFlow::ServiceStreamWriter<Dia::CellLayers::CellLayerRegistry> provider{ nullptr, StringCRC(CellHistoryModule::kRegistryStreamId) };
	TestModule module;
	module.ConnectStreams(connector);
	provider.Connect(connector);

	module.Start();
	module.Update();
	EXPECT_EQ(module.GetRecorder(), nullptr);	// registered nothing yet
	provider.Register(registry);
	module.Update();
	EXPECT_EQ(module.GetRecorder(), nullptr);	// registered but not committed
	connector.CommitReady();
	module.Update();
	EXPECT_NE(module.GetRecorder(), nullptr);
}

TEST(CellHistoryModule, DefaultConfigGivesDefaultBudgets)
{
	ModuleRig r;
	r.Wire();
	r.Run();
	CellHistoryRecorder::HistoryStatus st;
	r.module.GetRecorder()->BuildStatus(st);
	EXPECT_EQ(st.mCellBudget, 64ull * 1048576ull);
	EXPECT_EQ(st.mAggregateBudget, 64ull * 1048576ull);
	EXPECT_EQ(st.mEventBudget, 16ull * 1048576ull);
}

TEST(CellHistoryModule, ConfigAppliedBudgetsAndTrack)
{
	ModuleRig r;
	r.Wire();
	r.module.OnConfigure("{\"history\":{\"track\":[\"temp\"],\"cellBudgetMB\":8,\"aggregateBudgetMB\":4,\"eventBudgetMB\":2}}");
	r.Run();
	CellHistoryRecorder* rec = r.module.GetRecorder();
	SampleInfo info; info.tick = 1;
	rec->Sample(info);
	CellHistoryRecorder::HistoryStatus st;
	rec->BuildStatus(st);
	EXPECT_EQ(st.mCellBudget, 8ull * 1048576ull);
	EXPECT_EQ(st.mAggregateBudget, 4ull * 1048576ull);
	EXPECT_EQ(st.mEventBudget, 2ull * 1048576ull);
	EXPECT_TRUE(rec->IsTracked(StringCRC("temp")));
}

TEST(CellHistoryModule, BareHistoryBlockConfigAlsoAccepted)
{
	ModuleRig r;
	r.Wire();
	r.module.OnConfigure("{\"eventBudgetMB\":3}");
	EXPECT_EQ(r.module.GetConfig().mEventBudgetMB, 3u);
}

TEST(CellHistoryModule, InvalidConfigLogsErrorAndUsesDefaults)
{
	LogCapture log("HIS");
	ModuleRig r;
	r.Wire();
	r.module.OnConfigure("{\"history\":{\"cellBudgetMB\":\"lots\"}}");
	EXPECT_GE(log.Count(LogCapture::Level::kError, "config"), 1u);
	EXPECT_EQ(r.module.GetConfig().mCellBudgetMB, 64u);

	log.Clear();
	r.module.OnConfigure("{not json");
	EXPECT_GE(log.Count(LogCapture::Level::kError, "config"), 1u);
	EXPECT_EQ(r.module.GetConfig().mCellBudgetMB, 64u);

	r.Run();	// still works with defaults
	EXPECT_NE(r.module.GetRecorder(), nullptr);
}

TEST(CellHistoryModule, EmptyConfigIsDefaultsWithoutError)
{
	LogCapture log("HIS");
	ModuleRig r;
	r.module.OnConfigure("");
	r.module.OnConfigure(nullptr);
	EXPECT_EQ(log.Count(LogCapture::Level::kError), 0u);
}

TEST(CellHistoryModule, RunIdSeedIsUsedAndDeterministic)
{
	ModuleRig r;
	r.Wire();
	r.module.SetRunIdSeed(42);
	r.Run();
	CellHistoryRecorder expected(r.registry, r.schema.service.get(), HistoryConfig(), 42);
	EXPECT_EQ(r.module.GetRecorder()->RunId(), expected.RunId());
}

TEST(CellHistoryModule, StreamPublishedAfterStartAndWithdrawnOnStop)
{
	ModuleRig r;
	r.Wire();
	r.Run();
	EXPECT_EQ(FindCellHistoryRecorder(r.reader), r.module.GetRecorder());
	EXPECT_NE(FindCellHistoryRecorder(r.reader), nullptr);

	r.module.Stop();
	EXPECT_EQ(FindCellHistoryRecorder(r.reader), nullptr);
}

TEST(CellHistoryModule, RestartBuildsAFreshRecorder)
{
	ModuleRig r;
	r.Wire();
	r.Run();
	r.module.Stop();
	r.Run();	// a second Start/Update/commit cycle must not trip the single-registration assert
	EXPECT_NE(FindCellHistoryRecorder(r.reader), nullptr);
	EXPECT_EQ(r.module.GetRecorder()->SampleCount(), 0u);
}

TEST(CellHistoryModule, DoesNotSampleItself)
{
	ModuleRig r;
	r.Wire();
	r.Run();
	for (int i = 0; i < 5; ++i) { r.module.Update(); }
	EXPECT_EQ(r.module.GetRecorder()->SampleCount(), 0u);
}

#ifdef DIA_DEBUG

#include <DiaCellLayers/Testing/FakeMirrorRegistrar.h>

TEST(CellHistoryModule, CommandsRegisteredOnceAndUnregisteredOnStop)
{
	Dia::CellLayers::Testing::FakeMirrorRegistrar registrar;
	ModuleRig r;
	r.Wire();
	r.module.SetRegistrar(&registrar);
	r.Run();
	r.module.Update();
	r.module.Update();
	EXPECT_TRUE(r.module.CommandsRegistered());
	EXPECT_EQ(registrar.Count(), 3u);
	EXPECT_EQ(registrar.RegisterCount(), 3u);	// registered once, not per update
	EXPECT_TRUE(registrar.Has("cells.history.status"));

	r.module.Stop();
	EXPECT_FALSE(r.module.CommandsRegistered());
	EXPECT_EQ(registrar.Count(), 0u);
}

TEST(CellHistoryModule, NoRegistrarMeansNoCommandsAndNoCrash)
{
	ModuleRig r;
	r.Wire();
	r.Run();
	EXPECT_FALSE(r.module.CommandsRegistered());
	r.module.Stop();
}

TEST(CellHistoryModule, CommandsServedByTheModulesRecorder)
{
	Dia::CellLayers::Testing::FakeMirrorRegistrar registrar;
	ModuleRig r;
	r.Wire();
	r.module.SetRegistrar(&registrar);
	r.Run();
	SampleInfo info; info.tick = 7;
	r.module.GetRecorder()->Sample(info);
	const Json::Value status = registrar.Execute("cells.history.status", Json::Value(Json::objectValue));
	ASSERT_TRUE(status.isObject());
	EXPECT_EQ(status["sampleCount"].asUInt(), 1u);
}

TEST(CellHistoryModule, RestartReregistersCommands)
{
	Dia::CellLayers::Testing::FakeMirrorRegistrar registrar;
	ModuleRig r;
	r.Wire();
	r.module.SetRegistrar(&registrar);
	r.Run();
	r.module.Stop();
	r.Run();
	EXPECT_EQ(registrar.Count(), 3u);
}

#endif // DIA_DEBUG
