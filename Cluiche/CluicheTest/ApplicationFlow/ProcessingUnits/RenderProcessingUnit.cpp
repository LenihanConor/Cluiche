#include "ApplicationFlow/ProcessingUnits/RenderProcessingUnit.h"

#include <DiaGraphics/Interface/ICanvas.h>
#include <DiaLogger/Logger.h>
#include <DiaLogger/DiaLog.h>

namespace Cluiche
{
	const Dia::Core::StringCRC RenderProcessingUnit::kTypeId("RenderProcessingUnit");

	RenderProcessingUnit::RenderProcessingUnit(const Dia::Core::StringCRC& instanceId, float hz)
		: Dia::Application::ProcessingUnit(instanceId, hz)
		, mThreadBufferRegistered(false)
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

	void RenderProcessingUnit::PrePhaseUpdate()
	{
		if (!mThreadBufferRegistered)
		{
			Dia::Logger::Logger::Instance().RegisterThreadBuffer();
			DIA_LOG_INFO("Application", "Render thread registered for logging");
			mThreadBufferRegistered = true;
		}
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
		bool isFlaggedToStop = !(*mRunning);
		if (isFlaggedToStop && mThreadBufferRegistered)
		{
			Dia::Logger::Logger::Instance().UnregisterThreadBuffer();
			const_cast<RenderProcessingUnit*>(this)->mThreadBufferRegistered = false;
		}
		return isFlaggedToStop;
	}
}

#include <DiaApplication/TypeRegistry/RegistrationMacros.h>
namespace { using _RenderProcessingUnit = Cluiche::RenderProcessingUnit; }
DIA_REGISTER_PROCESSING_UNIT(_RenderProcessingUnit) {
	float hz = config.get("frequency_hz", 60.0f).asFloat();
	return new Cluiche::RenderProcessingUnit(instanceId, hz);
}