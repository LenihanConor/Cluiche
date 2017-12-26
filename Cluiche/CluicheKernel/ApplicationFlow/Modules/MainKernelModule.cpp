#include "CluicheKernel/ApplicationFlow/Modules/MainKernelModule.h"

#include <DiaUI/IUISystem.h>
#include <DiaCore/FilePath/PathStoreConfig.h>
#include <DiaCore/Type/TypeFacade.h>
#include <DiaCore/FilePath/SerializedFileLoad.h>
#include <DiaCore/Core/Assert.h>
#include <DiaSFML/RenderWindow.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/FilePath/PathStore.h>
#include <DiaGraphics/Interface/ICanvas.h>
#include <DiaInput/IInputSource.h>

namespace Cluiche
{
	namespace Kernel
	{
		const Dia::Core::StringCRC MainKernelModule::kUniqueId("MainKernelModule");

		MainKernelModule::MainKernelModule(Dia::Application::ProcessingUnit* associatedProcessingUnit)
			: Dia::Application::Module(associatedProcessingUnit, kUniqueId, Dia::Application::Module::RunningEnum::kUpdate)
			, mRunning(true)
			, mTimeServer(30.0f, Dia::Core::TimeAbsolute::Zero())	// With only one time server everything in the main loop will increment at its frequency
		{}

		bool MainKernelModule::FlaggedToStopUpdating()const
		{
			return !mRunning;
		}

		Dia::Core::FrameStream<Dia::Graphics::FrameData>& MainKernelModule::GetSimToRenderFrameStream()
		{
			return mSimToRenderFrameStream;
		}

		const  Dia::Core::FrameStream<Dia::Graphics::FrameData>& MainKernelModule::GetSimToRenderFrameStream()const
		{
			return mSimToRenderFrameStream;
		}

		Dia::Core::FrameStream<Dia::Input::EventData>& MainKernelModule::GetInputToSimFrameStream()
		{
			return mInputToSimFrameStream;
		}

		const Dia::Core::FrameStream<Dia::Input::EventData>& MainKernelModule::GetInputToSimFrameStream()const
		{
			return mInputToSimFrameStream;
		}

		Dia::Input::EventData& MainKernelModule::GetInputEventData()
		{
			return mInputEventData;
		}

		const Dia::Input::EventData& MainKernelModule::GetInputEventData()const
		{
			return mInputEventData;
		}

		Dia::Graphics::ICanvas* MainKernelModule::GetCanvas()
		{
			return mCanvas;
		}

		const Dia::Graphics::ICanvas* MainKernelModule::GetCanvas()const
		{
			return mCanvas;
		}

		Dia::Window::IWindow* MainKernelModule::GetWindow()
		{
			return mWindow;
		}

		const Dia::Window::IWindow* MainKernelModule::GetWindow()const
		{
			return mWindow;
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
			Dia::Window::IWindow::Settings windowSetting("Cluiche Application", Dia::Window::IWindow::Settings::Dimensions(900, 700), Dia::Window::IWindow::Settings::Style());
			Dia::Graphics::ICanvas::Settings canvasSettings(Dia::Graphics::ICanvas::Settings::VSyncEnum::kEnable, 0, 0, 2, 0);

			Dia::SFML::RenderWindow* renderWindow = static_cast<Dia::SFML::RenderWindow*>(mWindowFactory.Create(windowSetting, canvasSettings));

			// Abstract Interfaces
			mWindow = renderWindow;
			mCanvas = renderWindow;

			
			// We are using SFML for keyboard and mouse support
			renderWindow->ListenForInputSources(Dia::Core::BitArray8(Dia::SFML::InputSource::ESources::kSystem | Dia::SFML::InputSource::ESources::kKeyboard | Dia::SFML::InputSource::ESources::kMouse));	// We are getting mouse and keyboard only from SFML
			mInputSourceManager.AddInputSource(renderWindow);

			// We are using a dia specific source for the gamepad
			mInputSourceManager.AddInputSource(&mGamepadManager); // Getting gamepads from the DIA	

			mCanvas->SetActiveContext(false); // Need to disable active context before starting rendering thread

			return StateObject::OpertionResponse::kImmediate;
		}

		void MainKernelModule::DoUpdate()
		{
			// Input thread :)
			
			mInputEventData.RemoveAll();

			// Input (Control) part		
			mInputSourceManager.StartFrame();
			mInputSourceManager.Update(mInputEventData);
			mInputSourceManager.EndFrame();

			if (mInputEventData.Size() > 0)
			{
				mInputToSimFrameStream.InsertCopyOfDataToStream(mInputEventData, mTimeServer.GetTime());
			}

			{
				const Dia::Input::EventData* pEventData = &mInputEventData;

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
					default:
						break;
					}
				}
			}

			mTimeServer.Tick();
		}

		void MainKernelModule::DoStop()
		{
			mWindowFactory.Destroy(mWindow);
		}
	}
}