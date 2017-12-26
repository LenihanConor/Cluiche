#pragma once

#include <DiaApplication/ApplicationModule.h>
#include <DiaCore/Time/TimeServer.h>
#include <DiaInput/InputSourceManager.h>
#include <DiaInput/ConsoleGamepadManager.h>
#include <DiaCore/Frame/FrameStream.h>
#include <DiaUIAwesomium/AwesomiumUISystem.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaSFML/RenderWindowFactory.h>

namespace Dia { namespace Application { class ProcessingUnit; } }
namespace Dia { namespace UI { namespace Awesomium { class UISystem; } } }
namespace Dia { namespace Graphics { class ICanvas; } }

namespace Cluiche
{
	namespace Kernel
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

			Dia::Core::FrameStream<Dia::Graphics::FrameData>& GetSimToRenderFrameStream();
			const  Dia::Core::FrameStream<Dia::Graphics::FrameData>& GetSimToRenderFrameStream()const;

			Dia::Core::FrameStream<Dia::Input::EventData>& GetInputToSimFrameStream();
			const Dia::Core::FrameStream<Dia::Input::EventData>& GetInputToSimFrameStream()const;

			Dia::Input::EventData& GetInputEventData(); 
			const Dia::Input::EventData& GetInputEventData()const;


			Dia::Graphics::ICanvas* GetCanvas();
			const Dia::Graphics::ICanvas* GetCanvas()const;

			Dia::Window::IWindow* GetWindow();
			const Dia::Window::IWindow* GetWindow()const;

			Dia::Core::TimeServer& GetTimeServer() { return mTimeServer; }
			const Dia::Core::TimeServer& GetTimeServer()const { return mTimeServer; }

			//TODO this is hack for the moment. The goal is that this will be a seperate module
			bool mRunning;

		private:
			virtual StateObject::OpertionResponse DoStart(const IStartData* startData) override;
			virtual void DoUpdate() override;
			virtual void DoStop() override;

			Dia::Core::TimeServer mTimeServer;

			Dia::Input::InputSourceManager mInputSourceManager;

			Dia::Input::ConsoleGamepadManager mGamepadManager;
			Dia::SFML::RenderWindowFactory mWindowFactory;

			// Abstract Interfaces
			Dia::Window::IWindow* mWindow;
			Dia::Input::EventData mInputEventData;
			Dia::Core::FrameStream<Dia::Graphics::FrameData> mSimToRenderFrameStream;
			Dia::Core::FrameStream<Dia::Input::EventData> mInputToSimFrameStream;
			Dia::Graphics::ICanvas* mCanvas;
		};
	}
}
