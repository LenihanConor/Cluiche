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

	

	// We are using SFML for keyboard and mouse support
	renderWindow->ListenForInputSources(Dia::Core::BitArray8(Dia::SFML::InputSource::ESources::kSystem | Dia::SFML::InputSource::ESources::kKeyboard | Dia::SFML::InputSource::ESources::kMouse));	// We are getting mouse and keyboard only from SFML
	inputSourceManager.AddInputSource(renderWindow);
	// We are using a dia specific source for the gamepad
	inputSourceManager.AddInputSource(&gamepadManager); // Getting gamepads from the DIA
	
	Dia::Core::TimeServer timeServer(60.0f, Dia::Core::TimeAbsolute::Zero());	// With only one time server everything in the main loop will increment at its frequency

	canvas->SetActiveContext(false); // Need to disable active context before starting rendering thread

	bool running = true;
	Dia::Core::FrameStream<Dia::Input::EventData> inputToSimFrameStream;
	Dia::Core::FrameStream<Dia::Graphics::FrameData> simToRenderFrameStream;

	SimThreadStruct simThreadStruct(running, timeServer, inputToSimFrameStream, simToRenderFrameStream);
	std::thread simThread(std::ref(simThreadStruct));

	RenderThreadStruct renderThreadStruct(running, timeServer, simToRenderFrameStream, canvas, window);
	std::thread renderThread(std::ref(renderThreadStruct));

	// run the main loop
	// Main thread
	Dia::Core::TimeThreadLimiter threadLimiter(1.0f/timeServer.GetStep().AsFloatInSeconds());
	while (running)
	{
		threadLimiter.Start();

		

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
		}

		timeServer.Tick();

		threadLimiter.Stop();
//		std::cout << "Main: Wait " << threadLimiter.RemainingTime().AsIntInMilliseconds() << " ms\n";
		threadLimiter.SleepThread();
	}

	simThread.join();
	renderThread.join();

	windowFactory.Destroy(window);

	return 0;
}
