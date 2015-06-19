#include <DiaSFML/RenderWindowFactory.h>
#include <DiaSFML/RenderWindow.h>
#include <DiaCore/Time/TimeServer.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaInput/IInputSource.h>
#include <DiaGraphics/Interface/ICanvas.h>

#include <DiaInput/ConsoleGamepadManager.h>
#include <DiaInput/InputSourceManager.h>

#include <thread>

Dia::Maths::Vector2D dynamicCirclePos(0.0f, 0.0f);
Dia::Graphics::RGBA dynamicCircleColour = Dia::Graphics::RGBA::Red;

struct SimThreadStruct
{
public:

	SimThreadStruct(Dia::Graphics::FrameData& renderFrameBuffer)
		: mRenderFrameBuffer(renderFrameBuffer)
	{}

	void operator()()
	{
		Run();
	}

	void Run()
	{
		// This is where the simulation (Model) would go...
		// In good architetcure you would only do simualtion events here and have a seperate step to interpret that into game specific rendering

		// This below is just a test
		mRenderFrameBuffer.RequestDraw(Dia::Graphics::DebugFrameDataCircle2D(Dia::Maths::Vector2D(100.0f, 100.0f), 75.0f));
		mRenderFrameBuffer.RequestDraw(Dia::Graphics::DebugFrameDataCircle2D(dynamicCirclePos, 25.0f, dynamicCircleColour));
		mRenderFrameBuffer.RequestDraw(Dia::Graphics::DebugFrameDataLine2D(Dia::Maths::Vector2D(100.0f, 100.0f), dynamicCirclePos));
	}

private:
	Dia::Graphics::FrameData& mRenderFrameBuffer;
};

struct RenderThreadStruct
{
public:
	RenderThreadStruct(Dia::Graphics::ICanvas* pCanvas, Dia::Graphics::FrameData& renderFrameBuffer)
		: mpCanvas(pCanvas)
		, mRenderFrameBuffer(renderFrameBuffer)
	{}

	void operator()()
	{
		Run();
	}

	void Run()
	{
		// Render (View) Part
		// In good architecture this is completley generic.
		mpCanvas->StartFrame(mRenderFrameBuffer);

		mpCanvas->ProcessFrame(mRenderFrameBuffer);

		mpCanvas->EndFrame(mRenderFrameBuffer);
	}

private:
	Dia::Graphics::ICanvas* mpCanvas;
	Dia::Graphics::FrameData& mRenderFrameBuffer;
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
	
	Dia::Core::TimeServer timeServer(30.0f, Dia::Core::TimeAbsolute::Zero());	// With only one time server everything in the main loop will increment at its frequency
	//FrameManager<GraphicsFrameData> frameManager;

	// run the main loop
	bool running = true;
	while (running)
	{

		{
			inputSourceManager.StartFrame();
			
	//		Register

			Dia::Input::EventStream eventStream; // Scope will wipe this for next turn

			// Input (Control) part		
			inputSourceManager.Update(eventStream);


			/// Hmmm... this probably belongs in simulation as it is the interpetation of the buttons
			// This is where an application will handle any user input
			for (unsigned int i = 0; i < eventStream.Size(); i++)
			{
				Dia::Input::Event event = eventStream[i];
				switch (event.type)
				{
				case Dia::Input::Event::EType::kClosed:
					// end the program
					running = false;
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

			inputSourceManager.EndFrame();
		}


	//	std::thread sim(Sim);


		Dia::Graphics::FrameData simulationRenderFrame;

		SimThreadStruct simThreadStruct(simulationRenderFrame);
		RenderThreadStruct renderThreadStruct(canvas, simulationRenderFrame);

		std::thread simThread(simThreadStruct);
		simThread.join();
		
	//	renderThreadStruct.Run();
		std::thread renderThread(renderThreadStruct);
		renderThread.join();

		timeServer.Tick();// Move us onto the next frame
	}

	windowFactory.Destroy(window);

	return 0;
}
