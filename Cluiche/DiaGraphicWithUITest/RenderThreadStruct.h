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
						const Dia::Core::TimeServer& timeServer,
						Dia::Core::FrameStream<Dia::Graphics::FrameData>& frameStream,
						Dia::Graphics::ICanvas* pCanvas,
						const Dia::Window::IWindow* windowContext);

	void operator()();
	
	void Run();

private:
	// Shared resources
	const bool& mRunning;
	const Dia::Core::TimeServer& mTimeServer;
	Dia::Core::FrameStream<Dia::Graphics::FrameData>& mFrameStream;
	Dia::Graphics::ICanvas* mpCanvas;

	// Local resources
	Dia::Core::TimeThreadLimiter mThreadLimiter;
	Dia::UI::Awesomium::UISystem awesomiumUISystem;
};