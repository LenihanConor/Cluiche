#pragma once

#include <DiaApplication/ApplicationModule.h>
#include <DiaCore/Timer/TimeThreadLimiter.h>
#include <DiaCore/Time/TimeServer.h>
#include <DiaInput/InputSourceManager.h>
#include <DiaInput/ConsoleGamepadManager.h>
#include <DiaCore/Frame/FrameStream.h>
#include <DiaSFML/RenderWindowFactory.h>
#include <DiaSFML/RenderWindow.h>

#include "SimThreadStruct.h"
#include "RenderThreadStruct.h"

namespace Dia { namespace Application { class ProcessingUnit; } }
namespace Dia { namespace UI { namespace Awesomium { class UISystem; } } }

namespace Cluiche
{
	////////////////////////////////////////////////////
	//
	// MainKernelModule: Central module for all base 
	//					systems running on the main thread
	//
	////////////////////////////////////////////////////
	class MainKernelModule : public Dia::Application::Module
	{
	public:
		static const Dia::Core::StringCRC kUniqueId;

		MainKernelModule(Dia::Application::ProcessingUnit* associatedProcessingUnit);
	
		bool FlaggedToStopUpdating()const;

		//TODO this is hack for the moment. The goal is that this will be a seperate module
		Dia::UI::Awesomium::UISystem* mAwesomiumUISystem;

	private:
		virtual StateObject::OpertionResponse DoStart() override;
		virtual void DoUpdate() override;
		virtual void DoStop() override;

		bool mRunning;
		Dia::Core::TimeThreadLimiter mThreadLimiter;
		Dia::Core::TimeServer mTimeServer;
		
		Dia::Input::InputSourceManager mInputSourceManager;
		Dia::Core::FrameStream<Dia::Input::EventData> mInputToSimFrameStream;
		Dia::Core::FrameStream<Dia::Graphics::FrameData> mSimToRenderFrameStream;

		SimThreadStruct mSimThreadStruct;
		RenderThreadStruct mRenderThreadStruct;

		std::thread* mSimThread;
		std::thread* mRenderThread;

		Dia::Input::ConsoleGamepadManager mGamepadManager;
		Dia::SFML::RenderWindowFactory mWindowFactory;

		Dia::SFML::RenderWindow* renderWindow; // TODO CLEAN UP

		// Abstract Interfaces
		Dia::Window::IWindow* window;
		Dia::Graphics::ICanvas* canvas;
	};
}
