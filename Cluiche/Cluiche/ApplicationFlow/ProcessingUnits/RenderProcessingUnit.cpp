#include "ApplicationFlow/ProcessingUnits/RenderProcessingUnit.h"

#include <DiaGraphics/Interface/ICanvas.h>

namespace Cluiche
{
	const Dia::Core::StringCRC RenderProcessingUnit::kUniqueId("RenderProcessingUnit");

	RenderProcessingUnit::RenderProcessingUnit()
		: Dia::Application::ProcessingUnit(kUniqueId, 60.0f)
		, mRunningPhase(this)
		, mpCanvas(nullptr)
	{
		// Setup Phase Transitions
		SetInitialPhase(&mRunningPhase);

		// We call this from here to build all dependancies. We dont need to do this here, intead 
		// we could of done it in the code below if there was dynamic adding of modules or phases
		// but for this cas case this is a nicer cleaner solution.
		Initialize();
	}

	void RenderProcessingUnit::PostPhaseStart(const Dia::Application::StateObject::IStartData* startData)
	{
		DIA_ASSERT(startData != nullptr, "Starting the render PU needs to created with some start data");

		const StartData* renderStartData = static_cast<const StartData*>(startData);
		mRunning = renderStartData->mRunning;
		mFrameStream = renderStartData->mFrameStream;
		mpCanvas = renderStartData->mCanvas;
	}

	void RenderProcessingUnit::PostPhaseUpdate()
	{
		Dia::Core::TimeAbsolute timeStampOfRenderFrameBuffer = Dia::Core::TimeAbsolute::Zero();
		const Dia::Graphics::FrameData* pRenderFrameBuffer = mFrameStream->FetchLatestData(timeStampOfRenderFrameBuffer);

		if (pRenderFrameBuffer != nullptr)
		{
			mpCanvas->StartFrame(*pRenderFrameBuffer);
			mpCanvas->ProcessFrame(*pRenderFrameBuffer);
			mpCanvas->EndFrame(*pRenderFrameBuffer);

			mFrameStream->GarbageCollectAllFramesOlderThan(timeStampOfRenderFrameBuffer);
		}
	}

	bool RenderProcessingUnit::FlaggedToStopUpdating()const
	{
		bool isFlaggeToStop = !(*mRunning);
		return isFlaggeToStop;
	}
}