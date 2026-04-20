#include "EditorViewModule.h"

#include <DiaWindow/Win32Window.h>
#include <DiaUICEF/CEFUISystem.h>
#include <DiaCore/Core/Assert.h>
#include <DiaCore/Strings/String64.h>

namespace Cluiche
{
	namespace Editor
	{
		const Dia::Core::StringCRC EditorViewModule::kTypeId("EditorViewModule");

		EditorViewModule::EditorViewModule(Dia::Application::ProcessingUnit* pu)
			: Dia::Application::Module(pu, kTypeId, RunningEnum::kUpdate)
			, mModel(nullptr)
			, mWindow(nullptr)
			, mCEFSystem(nullptr)
		{
		}

		EditorViewModule::~EditorViewModule()
		{
		}

		Dia::Application::StateObject::OpertionResponse EditorViewModule::DoStart(const Dia::Application::StateObject::IStartData*)
		{
			// Create the native window
			mWindow = new Dia::Window::Win32Window();

			if (mModel)
			{
				mWindow->SetCloseCallback([this]()
				{
					mModel->RequestClose();
				});
			}

			Dia::Window::IWindow::Settings::Dimensions dims(1280, 720);
			Dia::Window::IWindow::Settings::Style style;
			Dia::Core::Containers::String64 title("Cluiche Editor");
			Dia::Window::IWindow::Settings settings(title, dims, style);
			mWindow->Initialize(settings);

			// Create CEF UI system
			mCEFSystem = new Dia::UICEF::CEFUISystem(mWindow);
			mCEFSystem->SetSubprocessPath("CluicheEditor.exe");
			mCEFSystem->SetWindowedRendering(true);
			mCEFSystem->SetAssetBasePath("UI/");
			mCEFSystem->Initialize();

			// Load main React shell
			Dia::Maths::Vector2D size = mWindow->GetSize();
			mCEFSystem->CreatePage("dia://editor/index.html",
				static_cast<int>(size.X()), static_cast<int>(size.Y()));

			mView.Initialize(mCEFSystem);

			return Dia::Application::StateObject::OpertionResponse::kImmediate;
		}

		void EditorViewModule::DoUpdate()
		{
			if (mWindow)
				mWindow->PumpMessages();

			if (mCEFSystem)
				mCEFSystem->Update();
		}

		void EditorViewModule::DoStop()
		{
			mView.Shutdown();

			if (mCEFSystem)
			{
				mCEFSystem->Shutdown();
				delete mCEFSystem;
				mCEFSystem = nullptr;
			}

			if (mWindow)
			{
				mWindow->Close();
				delete mWindow;
				mWindow = nullptr;
			}
		}
	}
}
