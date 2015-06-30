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

Dia::Maths::Vector2D dynamicCirclePos(0.0f, 0.0f);
Dia::Graphics::RGBA dynamicCircleColour = Dia::Graphics::RGBA::Red;

struct SimThreadStruct
{
public:

	SimThreadStruct(bool& running,
					const Dia::Core::TimeServer& timeServer,
					Dia::Core::FrameStream<Dia::Input::EventData>& inputToSimFrameStream,
					Dia::Core::FrameStream<Dia::Graphics::FrameData>& SimToRenderFrameStream)
		: mRunning(running)
		, mTimeServer(timeServer)
		, mInputToSimFrameStream(inputToSimFrameStream)
		, mSimToRenderFrameStream(SimToRenderFrameStream)
		, mThreadLimiter(30.0) 
	{}

	void operator()()
	{
		Run();
	}

	void Run()
	{
		// This is where the simulation (Model) would go...

		// This below is just a test
		while (mRunning)
		{
			mThreadLimiter.Start();

			// All going to Sim thread

			Dia::Core::Containers::DynamicArrayC<const Dia::Input::EventData*, 32> inputEventPtrBuffer;
			mInputToSimFrameStream.FetchAllDataUpToTime(mTimeServer.GetTime(), inputEventPtrBuffer);

			for (unsigned int i = 0; i < inputEventPtrBuffer.Size(); i++)
			{
				const Dia::Input::EventData* pEventData = inputEventPtrBuffer[i];

				// This is where an application will handle any user input
				for (unsigned int j = 0; j < pEventData->Size(); j++)
				{
					Dia::Input::Event event = (*pEventData)[j];
					switch (event.type)
					{
					case Dia::Input::Event::EType::kClosed:
						// end the program
						mRunning = false;
						break;
					case Dia::Input::Event::EType::kConsoleGamepadAnalogStickMove:
						dynamicCirclePos += Dia::Maths::Vector2D(event.consoleGamepadMoveEvent.x, -event.consoleGamepadMoveEvent.y);
						break;
					case Dia::Input::Event::EType::kConsoleGamepadButtonPressed:
						switch (event.consoleGamepadButtonEvent.button)
						{
						case Dia::Input::ConsoleGamepad::EButtonID::A:
							dynamicCircleColour = Dia::Graphics::RGBA::Green;
							break;
						case Dia::Input::ConsoleGamepad::EButtonID::B:
							dynamicCircleColour = Dia::Graphics::RGBA::Red;
							break;
						case Dia::Input::ConsoleGamepad::EButtonID::X:
							dynamicCircleColour = Dia::Graphics::RGBA::Blue;
							break;
						case Dia::Input::ConsoleGamepad::EButtonID::Y:
							dynamicCircleColour = Dia::Graphics::RGBA::Yellow;
							break;
						}

						break;
					default:
						break;
					}
				}
			}

			mInputToSimFrameStream.GarbageCollectAllFramesOlderThan(mTimeServer.GetTime());

			Dia::Graphics::FrameData renderFrameBuffer;

			renderFrameBuffer.RequestDraw(Dia::Graphics::DebugFrameDataCircle2D(Dia::Maths::Vector2D(100.0f, 100.0f), 75.0f));
			renderFrameBuffer.RequestDraw(Dia::Graphics::DebugFrameDataCircle2D(dynamicCirclePos, 25.0f, dynamicCircleColour));
			renderFrameBuffer.RequestDraw(Dia::Graphics::DebugFrameDataLine2D(Dia::Maths::Vector2D(100.0f, 100.0f), dynamicCirclePos));

			mSimToRenderFrameStream.InsertCopyOfDataToStream(renderFrameBuffer, mTimeServer.GetTime());

			mThreadLimiter.Stop();
			std::cout << "SIM: Wait " << mThreadLimiter.RemainingTime().AsIntInMilliseconds() << " ms\n";
			mThreadLimiter.SleepThread();
		}
	}

private:
	// Shared resources
	bool& mRunning;
	const Dia::Core::TimeServer& mTimeServer;
	Dia::Core::FrameStream<Dia::Input::EventData>& mInputToSimFrameStream;
	Dia::Core::FrameStream<Dia::Graphics::FrameData>& mSimToRenderFrameStream;
	
	// Local resources
	Dia::Core::TimeThreadLimiter mThreadLimiter;
};

struct RenderThreadStruct
{
public:
	RenderThreadStruct(const bool& running, 
						const Dia::Core::TimeServer& timeServer, 
						Dia::Core::FrameStream<Dia::Graphics::FrameData>& frameStream, 
						Dia::Graphics::ICanvas* pCanvas)
		: mRunning(running)
		, mTimeServer(timeServer)
		, mFrameStream(frameStream)
		, mThreadLimiter(60.0)
		, mpCanvas(pCanvas)
	{}

	void operator()()
	{
		Run();
	}

	void Run()
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
					
			mpCanvas->StartFrame(*pRenderFrameBuffer);
			mpCanvas->ProcessFrame(*pRenderFrameBuffer);
			mpCanvas->EndFrame(*pRenderFrameBuffer);
			
			mFrameStream.GarbageCollectAllFramesOlderThan(timeStampOfRenderFrameBuffer);

			mThreadLimiter.Stop();
			std::cout << "RENDER: Wait " << mThreadLimiter.RemainingTime().AsIntInMilliseconds() << " ms\n";
			mThreadLimiter.SleepThread();
		}
	}

private:
	// Shared resources
	const bool& mRunning; 
	const Dia::Core::TimeServer& mTimeServer;
	Dia::Core::FrameStream<Dia::Graphics::FrameData>& mFrameStream;
	Dia::Graphics::ICanvas* mpCanvas;

	// Local resources
	Dia::Core::TimeThreadLimiter mThreadLimiter;
};

int main(int argc, const char* argv[])
{
	Dia::SFML::RenderWindowFactory windowFactory;
	
	Dia::Window::IWindow::Settings windowSetting("GraphicsTest", Dia::Window::IWindow::Settings::Dimensions(800, 600), Dia::Window::IWindow::Settings::Style());
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

	RenderThreadStruct renderThreadStruct(running, timeServer, simToRenderFrameStream, canvas);
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
		std::cout << "Main: Wait " << threadLimiter.RemainingTime().AsIntInMilliseconds() << " ms\n";
		threadLimiter.SleepThread();
	}

	simThread.join();
	renderThread.join();

	windowFactory.Destroy(window);

	return 0;
}
