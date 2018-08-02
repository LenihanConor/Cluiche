#include "ApplicationFlow/ProcessingUnits/SimProcessingUnit.h"

#include "CluicheKernel/ApplicationFlow/Modules/MainUIModule.h"

#include <DiaGraphics/Interface/ICanvas.h>
#include <DiaUI/IUISystem.h>

namespace Cluiche
{
	Dia::Maths::Vector2D dynamicCirclePos(0.0f, 0.0f);
	Dia::Graphics::RGBA dynamicCircleColour = Dia::Graphics::RGBA::Red;

	const Dia::Core::StringCRC SimProcessingUnit::kUniqueId("SimProcessingUnit");

	SimProcessingUnit::SimProcessingUnit()
		: Dia::Application::ProcessingUnit(kUniqueId, 30.0f)
		, mRunning(nullptr)
		, mSimToRenderFrameStream(nullptr)
		, mBootPhase(this)
		, mBootStrapPhase(this)
		, mSimTimeServerModule(this, 30.0f, Dia::Core::TimeAbsolute::Zero())
		, mSimUIProxyModule(this, &mRenderFrameBuffer)
		, mSimInputFrameStreamModule(this)
	{
		// Setup Phase Transitions
		SetInitialPhase(&mBootPhase);
		AddPhaseTransiton(&mBootPhase, &mBootStrapPhase);

		// We call this from here to build all dependancies. We dont need to do this here, intead 
		// we could of done it in the code below if there was dynamic adding of modules or phases
		// but for this cas case this is a nicer cleaner solution.
		Initialize();
	}

	void SimProcessingUnit::PostPhaseStart(const Dia::Application::StateObject::IStartData* startData)
	{
		DIA_ASSERT(startData != nullptr, "Starting the render PU needs to created with some start data");

		const StartData* simStartData = static_cast<const StartData*>(startData);
		mRunning = simStartData->mRunning;
		mSimToRenderFrameStream = simStartData->mFrameStream;

		GetModule<Cluiche::Sim::UIProxyModule>()->Initialize(simStartData->mMainUIModule);
		GetModule<Cluiche::Sim::InputFrameStreamModule>()->Initialize(simStartData->mInputToSimFrameStream);
	}

	void SimProcessingUnit::PrePhaseUpdate()
	{
		mRenderFrameBuffer.Clear();
	}

	void SimProcessingUnit::PostPhaseUpdate()
	{
		// TODO MOVE TO THE PHASE
		// All going to Sim thread
		mSimInputFrameStreamModule.GetStream()->GarbageCollectAllFramesOlderThan(mSimTimeServerModule.GetTimeServer().GetTime());

		mRenderFrameBuffer.RequestDraw(Dia::Graphics::DebugFrameDataCircle2D(Dia::Maths::Vector2D(100.0f, 100.0f), 75.0f));
		mRenderFrameBuffer.RequestDraw(Dia::Graphics::DebugFrameDataCircle2D(dynamicCirclePos, 25.0f, dynamicCircleColour));
		mRenderFrameBuffer.RequestDraw(Dia::Graphics::DebugFrameDataLine2D(Dia::Maths::Vector2D(100.0f, 100.0f), dynamicCirclePos));

		mSimToRenderFrameStream->InsertCopyOfDataToStream(mRenderFrameBuffer, mSimTimeServerModule.GetTimeServer().GetTime());

		mSimTimeServerModule.Tick();
	}

	bool SimProcessingUnit::FlaggedToStopUpdating()const
	{
		bool isFlaggeToStop = !(*mRunning);
		return isFlaggeToStop;
	}
}