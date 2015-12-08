#pragma once

#include <DiaCore/Timer/TimeThreadLimiter.h>
#include <DiaCore/Time/TimeServer.h>
#include <DiaCore/Frame/FrameStream.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Interface/ICanvas.h>


#include <DiaUIAwesomium/AwesomiumUISystem.h>
#include  <DiaWindow/Interface/IWindow.h>

struct RenderThreadStruct
{
public:
	RenderThreadStruct(const bool& running,
						Dia::Core::FrameStream<Dia::Graphics::FrameData>& frameStream,
						Dia::Graphics::ICanvas* pCanvas);

	void operator()();
	
	void Run();

private:
	// Shared resources
	const bool& mRunning;
	Dia::Core::FrameStream<Dia::Graphics::FrameData>& mFrameStream;
	Dia::Graphics::ICanvas* mpCanvas;

	// Local resources
	Dia::Core::TimeThreadLimiter mThreadLimiter;
};