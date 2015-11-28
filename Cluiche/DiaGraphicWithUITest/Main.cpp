#include <DiaSFML/RenderWindowFactory.h>
#include <DiaSFML/RenderWindow.h>
#include <DiaCore/Time/TimeServer.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Timer/TimeThreadLimiter.h>
#include <DiaCore/Frame/FrameStream.h>
#include <DiaInput/IInputSource.h>
#include <DiaGraphics/Interface/ICanvas.h>

#include <DiaInput/ConsoleGamepadManager.h>
#include <DiaInput/InputSourceManager.h>

#include <thread>
#include <mutex>
#include <iostream>

#include "SimThreadStruct.h"
#include "RenderThreadStruct.h"

#include <DiaUI/IUISystem.h>
class LaunchUIPage: public Dia::UI::Page
{
public:
	LaunchUIPage()
		: Dia::UI::Page("file:///Z:/GitHub/Cluiche/Cluiche/WebixTest/app.html")
	{}

	void BindMethods(Dia::UI::IUISystem* parentSystem)
	{
		DIA_ASSERT(parentSystem, "parentSystem can not be NULL");

		parentSystem->BindMethod()
	}
}



// Bound to app.sayHello() in JavaScript
void BackgroundGrey(WebView* caller,
	const JSArray& args) {
	backColour = sf::Color(211, 211, 211);
}

#define CREATE_AWESOMIUM_BOUND_METHOD(_methodName, _ptrToMethod)\
	void _methodName(WebView* caller, const JSArray& args)\
	{\
		_ptrToMethod();\
	}
	

#define CREATE_AWESOMIUM_BOUND_METHOD_WITH_ARGS(_methodName, _ptrToMethod)\
	void _methodName(WebView* caller, const JSArray& args)\
	{\
		_ptrToMethod();\
	}

void DoSomething()
{

}

CREATE_AWESOMIUM_BOUND_METHOD(DoSomething)

void DoSomething(int args)
{

}



int main(int argc, const char* argv[])
{
	Dia::SFML::RenderWindowFactory windowFactory;
	
	Dia::Window::IWindow::Settings windowSetting("GraphicsTestWithUI", Dia::Window::IWindow::Settings::Dimensions(800, 600), Dia::Window::IWindow::Settings::Style());
	Dia::Graphics::ICanvas::Settings canvasSettings(Dia::Graphics::ICanvas::Settings::VSyncEnum::kEnable, 0, 0, 2, 0);
	
	Dia::SFML::RenderWindow* renderWindow = static_cast<Dia::SFML::RenderWindow*>(windowFactory.Create(windowSetting, canvasSettings));
	
	// Abstract Interfaces
	Dia::Window::IWindow* window = renderWindow;
	Dia::Graphics::ICanvas* canvas = renderWindow;

	// Set up generic input sources
	Dia::Input::InputSourceManager inputSourceManager;
	Dia::Input::ConsoleGamepadManager gamepadManager;

	LaunchUIPage launchUIPage;
	Dia::UI::Awesomium::UISystem awesomiumUISystem(window);
	awesomiumUISystem.Initialize();
	awesomiumUISystem.LoadPage(launchUIPage);

	// We are using SFML for keyboard and mouse support
	renderWindow->ListenForInputSources(Dia::Core::BitArray8(Dia::SFML::InputSource::ESources::kSystem | Dia::SFML::InputSource::ESources::kKeyboard | Dia::SFML::InputSource::ESources::kMouse));	// We are getting mouse and keyboard only from SFML
	inputSourceManager.AddInputSource(renderWindow);

	// We are using a dia specific source for the gamepad
	inputSourceManager.AddInputSource(&gamepadManager); // Getting gamepads from the DIA
	
	Dia::Core::TimeServer timeServer(30.0f, Dia::Core::TimeAbsolute::Zero());	// With only one time server everything in the main loop will increment at its frequency

	canvas->SetActiveContext(false); // Need to disable active context before starting rendering thread

	bool running = true;
	Dia::Core::FrameStream<Dia::Input::EventData> inputToSimFrameStream;
	Dia::Core::FrameStream<Dia::Graphics::FrameData> simToRenderFrameStream;

	SimThreadStruct simThreadStruct(running, timeServer, awesomiumUISystem, inputToSimFrameStream, simToRenderFrameStream);
	std::thread simThread(std::ref(simThreadStruct));

	RenderThreadStruct renderThreadStruct(running, timeServer, simToRenderFrameStream, canvas);
	std::thread renderThread(std::ref(renderThreadStruct));

	// run the main loop
	// Main thread
	Dia::Core::TimeThreadLimiter threadLimiter(1.0f/timeServer.GetStep().AsFloatInSeconds());

	
	while (running)
	{
		threadLimiter.Start();
		
		awesomiumUISystem.Update();

		// Input thread :)
		{
			Dia::Input::EventData eventData;

			// Input (Control) part		
			inputSourceManager.StartFrame();
			inputSourceManager.Update(eventData);
			inputSourceManager.EndFrame();
			
			if (eventData.Size() > 0)
			{
				inputToSimFrameStream.InsertCopyOfDataToStream(eventData, timeServer.GetTime());
			}

			//for (unsigned int i = 0; i < eventData.Size(); i++)
			{
				const Dia::Input::EventData* pEventData = &eventData;

				// This is where an application will handle any user input
				for (unsigned int j = 0; j < pEventData->Size(); j++)
				{
					Dia::Input::Event event = (*pEventData)[j];
					switch (event.type)
					{
						break;
					case Dia::Input::Event::EType::kMouseMoved:
					{
						awesomiumUISystem.InjectMouseMove(event.mouseMove.x, event.mouseMove.y);
					}
					break;
					case Dia::Input::Event::EType::kMouseButtonPressed:
					{
						awesomiumUISystem.InjectMouseClick(event.mouseButton.AsMouseButton(), event.mouseButton.x, event.mouseButton.y);
					}
					break;
					default:
						break;
					}
				}
			}
		}

		timeServer.Tick();

		threadLimiter.Stop();
		std::cout << "Main: Wait " << threadLimiter.RemainingTime().AsIntInMilliseconds() << " ms\n";
		threadLimiter.SleepThread();
	}

	simThread.join();
	renderThread.join();

	windowFactory.Destroy(window);

	return 0;
}
