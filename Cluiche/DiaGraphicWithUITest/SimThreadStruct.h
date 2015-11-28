#pragma once

#include <DiaCore/Timer/TimeThreadLimiter.h>
#include <DiaCore/Time/TimeServer.h>
#include <DiaCore/Frame/FrameStream.h>
#include <DiaInput/EventData.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaUI/IUISystem.h>

struct SimThreadStruct
{
public:

	SimThreadStruct(bool& running,
						const Dia::Core::TimeServer& timeServer,
						Dia::UI::IUISystem& uiSystem,
						Dia::Core::FrameStream<Dia::Input::EventData>& inputToSimFrameStream,
						Dia::Core::FrameStream<Dia::Graphics::FrameData>& SimToRenderFrameStream);

	void operator()();

	void Run();

private:
	// Shared resources
	bool& mRunning;
	const Dia::Core::TimeServer& mTimeServer;
	Dia::UI::IUISystem& mUISystem;
	Dia::Core::FrameStream<Dia::Input::EventData>& mInputToSimFrameStream;
	Dia::Core::FrameStream<Dia::Graphics::FrameData>& mSimToRenderFrameStream;

	// Local resources
	Dia::Core::TimeThreadLimiter mThreadLimiter;
};
