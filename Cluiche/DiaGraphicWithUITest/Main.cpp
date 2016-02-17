#include <DiaSFML/RenderWindowFactory.h>
#include <DiaSFML/RenderWindow.h>

#include <DiaCore/Time/TimeServer.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Timer/TimeThreadLimiter.h>
#include <DiaCore/Frame/FrameStream.h>
#include <DiaCore/FilePath/PathStore.h>

#include <DiaGraphics/Interface/ICanvas.h>

#include <DiaInput/ConsoleGamepadManager.h>
#include <DiaInput/InputSourceManager.h>
#include <DiaInput/IInputSource.h>

#include <DiaUI/Page.h>

#include <thread>
#include <mutex>
#include <iostream>

#include "SimThreadStruct.h"
#include "RenderThreadStruct.h"

#include <DiaUI/IUISystem.h>
#include <DiaCore/FilePath/PathStoreConfig.h>
#include <DiaCore/Containers/Strings/StringReader.h>
#include <DiaCore/Containers/Strings/StringWriter.h>
#include <DiaCore/Type/TypeFacade.h>




#include <iostream>
#include <fstream>
#include <string>




class LaunchUIPage: public Dia::UI::Page
{
public:
	void DoSomething(const Dia::UI::BoundMethodArgs&)
	{
		int x = 0;
		x++;
	}

	LaunchUIPage()
		: Dia::UI::Page(Dia::Core::FilePath("root", "DiaGraphicWithUITest/", "bootscreen.html")) 
	{
		BindMethod(Dia::UI::BoundMethod("backgroundGrey", Dia::UI::BoundMethod::MethodPtr(this, &LaunchUIPage::DoSomething)));
	}
}; 

#include <windows.h>
#include <string>
#include <iostream>
#include <sstream>
#include "DiaCore/Strings/stringutils.h"

using namespace std;;

std::string ExePath() {
	char buffer[MAX_PATH];
	GetModuleFileName(NULL, buffer, MAX_PATH);
	string::size_type pos = string(buffer).find_last_of("\\/");
	return string(buffer).substr(0, pos);
}

int main(int argc, const char* argv[])
{
	Dia::Core::PathStoreConfig pathStoreConfig;

	{char bufferMemory[32 * 1024];
	Dia::Core::Containers::StringWriter bufferSerial(&bufferMemory[0], 32 * 1024);

	Dia::Core::Types::GetTypeFacade().JsonSerializer().Serialize(pathStoreConfig, bufferSerial);

	int x = 0;
	x++;
	}
//	DIA_ASSERT(0, "NEED TO GET DYNAMIC ARRAYS UP AND RUNNING :(");
//	DIA_ASSERT(0, "NEED TO GET FILE LOAD :(");

	//Setup paths
	std::string dir = ExePath();
	char bufferMemory[32 * 1024];
	std::string line;

	std::ifstream f(dir + "\\pathStoreConfig.cfg");
	
	char getdata[10000];
	if (f.is_open())
	{
		f.read(getdata, sizeof getdata);
		if (f.eof())
		{
			// got the whole file...
			size_t bytes_really_read = f.gcount();

		}
		else if (f.fail())
		{
			// some other error...
		}
		else
		{
			// getdata must be full, but the file is larger...

		}
	}
	Dia::Core::Containers::StringReader bufferDeserial(&getdata[0]);

	Dia::Core::Types::GetTypeFacade().JsonSerializer().Deserialize(pathStoreConfig, bufferDeserial);


//	Dia::Core::PathStore::RegisterPathRootToStore("root", "file:///C:/Users/Conor/Documents/GitHub/Cluiche/Cluiche/");

	// Setup the rendering windoow
	Dia::SFML::RenderWindowFactory windowFactory;
	
	Dia::Window::IWindow::Settings windowSetting("GraphicsTestWithUI", Dia::Window::IWindow::Settings::Dimensions(900, 700), Dia::Window::IWindow::Settings::Style());
	Dia::Graphics::ICanvas::Settings canvasSettings(Dia::Graphics::ICanvas::Settings::VSyncEnum::kEnable, 0, 0, 2, 0);
	
	Dia::SFML::RenderWindow* renderWindow = static_cast<Dia::SFML::RenderWindow*>(windowFactory.Create(windowSetting, canvasSettings));
	
	// Abstract Interfaces
	Dia::Window::IWindow* window = renderWindow;
	Dia::Graphics::ICanvas* canvas = renderWindow;

	// Set up generic input sources
	Dia::Input::InputSourceManager inputSourceManager;
	Dia::Input::ConsoleGamepadManager gamepadManager;

	// Setup UI
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

	SimThreadStruct simThreadStruct(running, awesomiumUISystem, inputToSimFrameStream, simToRenderFrameStream);
	std::thread simThread(std::ref(simThreadStruct));

	RenderThreadStruct renderThreadStruct(running, simToRenderFrameStream, canvas);
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
	//	std::cout << "Main: Wait " << threadLimiter.RemainingTime().AsIntInMilliseconds() << " ms\n";
		threadLimiter.SleepThread();
	}

	simThread.join();
	renderThread.join();

	windowFactory.Destroy(window);

	return 0;
}
