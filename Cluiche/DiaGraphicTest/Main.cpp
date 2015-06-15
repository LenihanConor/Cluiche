#include <DiaSFML/RenderWindowFactory.h>
#include <DiaSFML/RenderWindow.h>
#include <DiaCore/Time/TimeServer.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaInput/IInputSource.h>
#include <DiaGraphics/Interface/ICanvas.h>

#include <DiaInput/ConsoleGamepadManager.h>
#include <DiaInput/InputSourceManager.h>


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
	
	Dia::Maths::Vector2D dynamicCirclePos(0.0f, 0.0f);
	Dia::Graphics::RGBA dynamicCircleColour = Dia::Graphics::RGBA::Red;

	Dia::Core::TimeServer timeServer(30.0f, Dia::Core::TimeAbsolute::Zero());	// With only one time server everything in the main loop will increment at its frequency
	//FrameManager<GrphicsFrameData> graphicsFrameManager;

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

		{
			// Dia::Graphics::FrameData& simulationRenderData = frameManager.FetchNewFrame(timeServer);
			 

			// This is where the simulation (Model) would go...
			// In good architetcure you would only do simualtion events here and have a seperate step to interpret that into game specific rendering

			// This below is just a test
			simulationRenderData.RequestDraw(Dia::Graphics::DebugFrameDataCircle2D(Dia::Maths::Vector2D(100.0f, 100.0f), 75.0f));
			simulationRenderData.RequestDraw(Dia::Graphics::DebugFrameDataCircle2D(dynamicCirclePos, 25.0f, dynamicCircleColour));
			simulationRenderData.RequestDraw(Dia::Graphics::DebugFrameDataLine2D(Dia::Maths::Vector2D(100.0f, 100.0f), dynamicCirclePos));

			// Render (View) Part
			// In good architecture this is completley generic.
			canvas->StartFrame(simulationRenderData);

			canvas->ProcessFrame(simulationRenderData);

			canvas->EndFrame(simulationRenderData);
		}

		timeServer.Tick();// Move us onto the next frame
	}

	windowFactory.Destroy(window);

	return 0;
}