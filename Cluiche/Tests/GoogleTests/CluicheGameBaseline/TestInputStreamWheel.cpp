// InputStreamModule mouse-wheel delta (worldgen-view task 6b). Real module on a SimPU, fed through the real
// "MainToSim" EventStream by a MainPU shim, exactly as KernelModule feeds it. Harness adapted from TestWorldGenCommands.cpp.
#include <gtest/gtest.h>

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/MainModule.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaStreams/EventStreamWriter.h>
#include <Modules/InputStreamModule.h>
#include <Types/MainToSimEvent.h>

#include <memory>
#include <vector>

using namespace Dia::ApplicationFlow;
using Cluiche::AppFlow::InputStreamModule;
using Cluiche::AppFlow::MainToSimEvent;
using Dia::Core::StringCRC;

namespace
{
	class WheelMainShim : public MainModule
	{
	public:
		static const StringCRC kTypeId;
		explicit WheelMainShim(const StringCRC& id) : MainModule(id) {}

		void QueueWheel(int delta)
		{
			MainToSimEvent e;
			e.kind = MainToSimEvent::Kind::kInput;
			e.input.type = Dia::Input::Event::EType::kMouseWheelMoved;
			e.input.mouseWheel.delta = delta;
			mPending.push_back(e);
		}
		void QueueMove(int x, int y)
		{
			MainToSimEvent e;
			e.kind = MainToSimEvent::Kind::kInput;
			e.input.type = Dia::Input::Event::EType::kMouseMoved;
			e.input.mouseMove.x = x;
			e.input.mouseMove.y = y;
			mPending.push_back(e);
		}

		void OnConnectStreams(Application& app) override { mOut.Connect(app); }

	protected:
		StartResult DoStart() override { return StartResult::kReady; }
		StopResult DoStop() override { return StopResult::kDone; }
		void DoUpdate(const Dia::SimTime::MainTimeContext&) override
		{
			for (const MainToSimEvent& e : mPending) (void)mOut.Send(e);
			mPending.clear();
		}

	private:
		std::vector<MainToSimEvent> mPending;
		EventStreamWriter<MainToSimEvent> mOut{ this, "MainToSim" };
	};
	const StringCRC WheelMainShim::kTypeId("WheelMainShim");

	WheelMainShim* gShim = nullptr;
	InputStreamModule* gInput = nullptr;
	Module* MakeShim(const StringCRC& id) { gShim = new WheelMainShim(id); return gShim; }
	Module* MakeInput(const StringCRC& id) { gInput = new InputStreamModule(id); return gInput; }

	ModuleDeclaration Mod(const StringCRC& typeId, const char* instance, const char* channel, const char* role)
	{
		ModuleDeclaration m;
		m.instanceId = StringCRC(instance);
		m.typeId = typeId;
		m.startTimeoutMs = 10000.0f;
		m.stopTimeoutMs = 5000.0f;
		m.stages.Add(StringCRC("Boot"));
		ChannelBinding b;
		b.id = StringCRC(channel);
		b.role = StringCRC(role);
		m.channels.Add(b);
		return m;
	}

	struct Harness
	{
		static constexpr float kDt = 0.0334f;
		TypeRegistry reg;
		ApplicationManifestV3 manifest;
		std::unique_ptr<Application> app;

		Harness()
		{
			gShim = nullptr; gInput = nullptr;
			reg.Register(WheelMainShim::kTypeId, MakeShim);
			reg.Register(InputStreamModule::kTypeId, MakeInput);

			manifest.version = 3;
			StageDeclaration boot;
			boot.name = StringCRC("Boot");
			manifest.stages.Add(boot);
			manifest.initialStage = StringCRC("Boot");

			StreamDeclaration s;
			s.id = StringCRC("MainToSim");
			s.kind = StringCRC("EventStream");
			s.fromPU = StringCRC("MainPU");
			s.toPU = StringCRC("SimPU");
			s.capacity = 64;
			manifest.streams.Add(s);

			ProcessingUnitDeclaration mainPu;
			mainPu.instanceId = StringCRC("MainPU");
			mainPu.frequencyHz = 30.0f;
			mainPu.dedicatedThread = false;
			mainPu.modules.Add(Mod(WheelMainShim::kTypeId, "shim", "MainToSim", "writes"));
			manifest.processingUnits.Add(mainPu);

			ProcessingUnitDeclaration simPu;
			simPu.instanceId = StringCRC("SimPU");
			simPu.frequencyHz = 30.0f;
			simPu.dedicatedThread = false;
			simPu.modules.Add(Mod(InputStreamModule::kTypeId, "InputStreamModule", "MainToSim", "reads"));
			manifest.processingUnits.Add(simPu);

			app.reset(new Application(manifest, reg));
		}

		bool Start()
		{
			if (!app->Start()) return false;
			for (int i = 0; i < 30; ++i)
			{
				app->Update(kDt);
				if (gInput && gInput->GetState() == ModuleState::kActive) break;
			}
			for (int i = 0; i < 4; ++i) app->Update(kDt);
			return gShim && gInput && gInput->GetState() == ModuleState::kActive;
		}

		// Runs one frame; returns the wheel delta the input module reports afterwards.
		float Step() { app->Update(kDt); return gInput->GetMouseWheelDelta(); }

		~Harness()
		{
			if (!app) return;
			app->RequestShutdown();
			for (int guard = 0; guard < 1000 && app->Update(kDt); ++guard) {}
			app.reset();
		}
	};

	// Pumps up to maxFrames until a non-zero delta is seen; returns it (0 if none).
	float PumpToWheel(Harness& h, int maxFrames = 20)
	{
		for (int i = 0; i < maxFrames; ++i)
		{
			const float d = h.Step();
			if (d != 0.0f) return d;
		}
		return 0.0f;
	}
}

TEST(InputStreamWheel, ZeroWithoutEvents)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	EXPECT_EQ(gInput->GetMouseWheelDelta(), 0.0f);
	EXPECT_EQ(h.Step(), 0.0f);
}

TEST(InputStreamWheel, DeltaVisibleForOneFrameThenZero)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	gShim->QueueWheel(3);
	EXPECT_EQ(PumpToWheel(h), 3.0f);
	EXPECT_EQ(h.Step(), 0.0f);
	EXPECT_EQ(h.Step(), 0.0f);
}

TEST(InputStreamWheel, NegativeDelta)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	gShim->QueueWheel(-2);
	EXPECT_EQ(PumpToWheel(h), -2.0f);
	EXPECT_EQ(h.Step(), 0.0f);
}

TEST(InputStreamWheel, MultipleEventsInOneFrameAccumulate)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	gShim->QueueWheel(1);
	gShim->QueueWheel(2);
	gShim->QueueWheel(-1);
	EXPECT_EQ(PumpToWheel(h), 2.0f);
	EXPECT_EQ(h.Step(), 0.0f);
}

TEST(InputStreamWheel, OtherMouseEventsDoNotAffectWheel)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	gShim->QueueMove(10, 20);
	EXPECT_EQ(PumpToWheel(h, 6), 0.0f);
	EXPECT_EQ(gInput->GetMouseX(), 10);
	EXPECT_EQ(gInput->GetMouseY(), 20);
}
