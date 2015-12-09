#include "RenderThreadStruct.h"

#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Timer/TimeThreadLimiter.h>
#include <DiaCore/Frame/FrameStream.h>

#include <iostream>

RenderThreadStruct::RenderThreadStruct(const bool& running,
										Dia::Core::FrameStream<Dia::Graphics::FrameData>& frameStream,
										Dia::Graphics::ICanvas* pCanvas)
	: mRunning(running)
	, mFrameStream(frameStream)
	, mThreadLimiter(60.0)
	, mpCanvas(pCanvas)
{
	
}

void RenderThreadStruct::operator()()
{
	Run();
}

void RenderThreadStruct::Run()
{
	// Render (View) Part
	while (mRunning)
	{
		mThreadLimiter.Start();

		Dia::Core::TimeAbsolute timeStampOfRenderFrameBuffer = Dia::Core::TimeAbsolute::Zero();
		const Dia::Graphics::FrameData* pRenderFrameBuffer = mFrameStream.FetchLatestData(timeStampOfRenderFrameBuffer);

		if (pRenderFrameBuffer == nullptr)
		{
			continue;
		}	

		mpCanvas->StartFrame(*pRenderFrameBuffer);
		mpCanvas->ProcessFrame(*pRenderFrameBuffer);
		mpCanvas->EndFrame(*pRenderFrameBuffer);

		mFrameStream.GarbageCollectAllFramesOlderThan(timeStampOfRenderFrameBuffer);

		mThreadLimiter.Stop();
//		std::cout << "RENDER: Wait " << mThreadLimiter.RemainingTime().AsIntInMilliseconds() << " ms\n";
		mThreadLimiter.SleepThread();
	}
}