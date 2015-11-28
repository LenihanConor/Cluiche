#include "RenderThreadStruct.h"

#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Timer/TimeThreadLimiter.h>
#include <DiaCore/Frame/FrameStream.h>
#include <DiaUI/UIDataBuffer.h>

#include <iostream>

RenderThreadStruct::RenderThreadStruct(const bool& running,
										const Dia::Core::TimeServer& timeServer,
										Dia::Core::FrameStream<Dia::Graphics::FrameData>& frameStream,
										Dia::Graphics::ICanvas* pCanvas)
	: mRunning(running)
	, mTimeServer(timeServer)
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
		const Dia::Graphics::FrameData* pRenderFrameBuffer = mFrameStream.FetchDataClosestToTime(mTimeServer.GetTime(), timeStampOfRenderFrameBuffer);

		if (pRenderFrameBuffer == nullptr)
		{
			continue;
		}

		// UI
	//	awesomiumUISystem.Update();

		

		mpCanvas->StartFrame(*pRenderFrameBuffer);
		mpCanvas->ProcessFrame(*pRenderFrameBuffer);
		mpCanvas->EndFrame(*pRenderFrameBuffer);

		mFrameStream.GarbageCollectAllFramesOlderThan(timeStampOfRenderFrameBuffer);

		mThreadLimiter.Stop();
//		std::cout << "RENDER: Wait " << mThreadLimiter.RemainingTime().AsIntInMilliseconds() << " ms\n";
		mThreadLimiter.SleepThread();
	}
}