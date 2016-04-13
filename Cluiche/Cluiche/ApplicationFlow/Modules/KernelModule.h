////////////////////////////////////////////////////////////////////////////////
// Filename: MainProcessingUnit.h
//
// This is the main processing unit for Cluiche. All other processing 
//			units stem from this.
//
////////////////////////////////////////////////////////////////////////////////


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
	class KernelModule : public Dia::Application::Module
	{
	public:
		static const Dia::Core::StringCRC kUniqueId;

		KernelModule(Dia::Application::ProcessingUnit* associatedProcessingUnit);
	
	private:
		virtual StateObject::OpertionResponse DoStart() override;
		virtual void DoUpdate() override;
		virtual void DoStop() override;

		bool mRunning;
		Dia::Core::TimeThreadLimiter mThreadLimiter;
		Dia::Core::TimeServer mTimeServer;
		Dia::UI::Awesomium::UISystem* mAwesomiumUISystem;
		Dia::Input::InputSourceManager mInputSourceManager;
		Dia::Core::FrameStream<Dia::Input::EventData> mInputToSimFrameStream;
		Dia::Core::FrameStream<Dia::Graphics::FrameData> mSimToRenderFrameStream;

		SimThreadStruct mSimThreadStruct;
		RenderThreadStruct mRenderThreadStruct;

		std::thread* mSimThread;
		std::thread* mRenderThread;

		Dia::Input::ConsoleGamepadManager mGamepadManager;
		Dia::SFML::RenderWindowFactory mWindowFactory;

		Dia::SFML::RenderWindow* renderWindow;

		// Abstract Interfaces
		Dia::Window::IWindow* window;
		Dia::Graphics::ICanvas* canvas;
	};
}
