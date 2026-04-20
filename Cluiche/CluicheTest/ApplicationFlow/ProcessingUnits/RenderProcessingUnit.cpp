#include "ApplicationFlow/ProcessingUnits/RenderProcessingUnit.h"

#include <DiaGraphics/Interface/ICanvas.h>

namespace Cluiche
{
	const Dia::Core::StringCRC RenderProcessingUnit::kTypeId("RenderProcessingUnit");

	RenderProcessingUnit::RenderProcessingUnit(const Dia::Core::StringCRC& instanceId, float hz)
		: Dia::Application::ProcessingUnit(instanceId, hz)
		, mRunning(nullptr)
		, mFrameStream(nullptr)
		, mpCanvas(nullptr)
	{}

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

#include <DiaApplication/TypeRegistry/RegistrationMacros.h>
namespace { using _RenderProcessingUnit = Cluiche::RenderProcessingUnit; }
DIA_REGISTER_PROCESSING_UNIT(_RenderProcessingUnit) {
	float hz = config.get("frequency_hz", 60.0f).asFloat();
	return new Cluiche::RenderProcessingUnit(instanceId, hz);
}