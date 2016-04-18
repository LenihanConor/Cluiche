#pragma once

#include <DiaApplication/ApplicationModule.h>
#include <DiaCore/Time/TimeServer.h>
#include <DiaInput/InputSourceManager.h>
#include <DiaInput/ConsoleGamepadManager.h>
#include <DiaCore/Frame/FrameStream.h>
#include <DiaSFML/RenderWindowFactory.h>
#include <DiaSFML/RenderWindow.h>
#include <DiaUIAwesomium/AwesomiumUISystem.h>

#include "SimThreadStruct.h"

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
		bool mRunning;
		Dia::UI::Awesomium::UISystem* mAwesomiumUISystem;
		Dia::Graphics::ICanvas* canvas;
		Dia::Core::FrameStream<Dia::Graphics::FrameData> mSimToRenderFrameStream;

	private:
		virtual StateObject::OpertionResponse DoStart(const IStartData* startData) override;
		virtual void DoUpdate() override;
		virtual void DoStop() override;
		
		Dia::Core::TimeServer mTimeServer;
		
		Dia::Input::InputSourceManager mInputSourceManager;
		Dia::Core::FrameStream<Dia::Input::EventData> mInputToSimFrameStream;
		
		SimThreadStruct mSimThreadStruct;

		std::thread* mSimThread;

		Dia::Input::ConsoleGamepadManager mGamepadManager;
		Dia::SFML::RenderWindowFactory mWindowFactory;

		Dia::SFML::RenderWindow* renderWindow; // TODO CLEAN UP

		// Abstract Interfaces
		Dia::Window::IWindow* window;
		
	};
}
