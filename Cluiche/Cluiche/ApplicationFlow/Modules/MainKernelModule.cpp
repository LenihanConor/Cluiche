#include "ApplicationFlow/Modules/MainKernelModule.h"

#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/FilePath/PathStore.h>

#include <DiaGraphics/Interface/ICanvas.h>

#include <DiaInput/IInputSource.h>

#include <DiaUI/Page.h>

#include <thread>

#include <DiaUI/IUISystem.h>
#include <DiaCore/FilePath/PathStoreConfig.h>
#include <DiaCore/Containers/Strings/StringWriter.h>
#include <DiaCore/Type/TypeFacade.h>
#include <DiaCore/FilePath/SerializedFileLoad.h>
#include <DiaCore/Core/Assert.h>

namespace Cluiche
{
	const Dia::Core::StringCRC MainKernelModule::kUniqueId("MainKernelModule");

	MainKernelModule::MainKernelModule(Dia::Application::ProcessingUnit* associatedProcessingUnit)
		: Dia::Application::Module(associatedProcessingUnit, kUniqueId, Dia::Application::Module::RunningEnum::kUpdate)
		, mRunning(true)
		, mTimeServer(30.0f, Dia::Core::TimeAbsolute::Zero())	// With only one time server everything in the main loop will increment at its frequency
		, mAwesomiumUISystem(nullptr)
	{}

	bool MainKernelModule::FlaggedToStopUpdating()const
	{
		return !mRunning;
	}

	Dia::Application::StateObject::OpertionResponse MainKernelModule::DoStart(const IStartData* startData)
	{
		//Setup paths
		Dia::Core::PathStoreConfig pathStoreConfig;

		std::string dir;
		Dia::Core::Path::ExePath(dir);

		Dia::Core::FilePath::ResoledFilePath pathStroreConfigFile("%s//pathStoreConfig.json", dir.c_str());

		Dia::Core::SerializedFileLoad fileLoad;
		if (fileLoad.LoadNow(pathStroreConfigFile, pathStoreConfig, 5096) != Dia::Core::IFileLoad::ReturnCode::kSuccess)
		{
			DIA_ASSERT(0, "Could not load pathStoreConfig at %s, ", dir.c_str());
			return StateObject::OpertionResponse::kImmediate;
		}

		Dia::Core::PathStore::RegisterToStore(pathStoreConfig);

		// Setup the rendering windoow
		Dia::Window::IWindow::Settings windowSetting("GraphicsTestWithUI", Dia::Window::IWindow::Settings::Dimensions(900, 700), Dia::Window::IWindow::Settings::Style());
		Dia::Graphics::ICanvas::Settings canvasSettings(Dia::Graphics::ICanvas::Settings::VSyncEnum::kEnable, 0, 0, 2, 0);

		mRenderWindow = static_cast<Dia::SFML::RenderWindow*>(mWindowFactory.Create(windowSetting, canvasSettings));

		// Abstract Interfaces
		mWindow = mRenderWindow;
		canvas = mRenderWindow;			

		// Setup UI
		mAwesomiumUISystem = DIA_NEW(Dia::UI::Awesomium::UISystem(mWindow));
		mAwesomiumUISystem->Initialize();

		// We are using SFML for keyboard and mouse support
		mRenderWindow->ListenForInputSources(Dia::Core::BitArray8(Dia::SFML::InputSource::ESources::kSystem | Dia::SFML::InputSource::ESources::kKeyboard | Dia::SFML::InputSource::ESources::kMouse));	// We are getting mouse and keyboard only from SFML
		mInputSourceManager.AddInputSource(mRenderWindow);

		// We are using a dia specific source for the gamepad
		mInputSourceManager.AddInputSource(&mGamepadManager); // Getting gamepads from the DIA	

		canvas->SetActiveContext(false); // Need to disable active context before starting rendering thread

		return StateObject::OpertionResponse::kImmediate;
	}

	void MainKernelModule::DoUpdate()
	{
		mAwesomiumUISystem->Update();

		// Input thread :)
		{
			Dia::Input::EventData eventData;

			// Input (Control) part		
			mInputSourceManager.StartFrame();
			mInputSourceManager.Update(eventData);
			mInputSourceManager.EndFrame();

			if (eventData.Size() > 0)
			{
				mInputToSimFrameStream.InsertCopyOfDataToStream(eventData, mTimeServer.GetTime());
			}

			{
				const Dia::Input::EventData* pEventData = &eventData;

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
					case Dia::Input::Event::EType::kMouseMoved:
					{
						mAwesomiumUISystem->InjectMouseMove(event.mouseMove.x, event.mouseMove.y);
					}
					break;
					case Dia::Input::Event::EType::kMouseButtonPressed:
					{
						mAwesomiumUISystem->InjectMouseClick(event.mouseButton.AsMouseButton(), event.mouseButton.x, event.mouseButton.y);
					}
					break;
					default:
						break;
					}
				}
			}
		}

		mTimeServer.Tick();
	}

	void MainKernelModule::DoStop()
	{
		DIA_DELETE(mAwesomiumUISystem);

		mWindowFactory.Destroy(mWindow);
	}
}