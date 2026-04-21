#include "EditorViewModule.h"

#include <DiaWindow/NativeWindow.h>
#include <DiaUICEF/EditorUISystemFactory.h>
#include <DiaUI/IUISystem.h>
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
			, mUISystem(nullptr)
		{
		}

		EditorViewModule::~EditorViewModule()
		{
		}

		Dia::Application::StateObject::OpertionResponse EditorViewModule::DoStart(const Dia::Application::StateObject::IStartData*)
		{
			Dia::Window::IWindow::Settings::Dimensions dims(1280, 720);
			Dia::Window::IWindow::Settings::Style style;
			Dia::Core::Containers::String64 title("Cluiche Editor");
			Dia::Window::IWindow::Settings settings(title, dims, style);

			Dia::Editor::EditorModel* model = mModel;
			Dia::Window::WindowCloseCallback onClose;
			if (model)
				onClose = [model]() { model->RequestClose(); };

			mWindow = Dia::Window::CreateNativeWindow(settings, onClose);

			Dia::UICEF::EditorUISystemConfig uiConfig;
			uiConfig.subprocessPath = "CluicheEditor.exe";
			uiConfig.assetBasePath = "";
			uiConfig.windowedRendering = true;
			mUISystem = Dia::UICEF::CreateEditorUISystem(mWindow, uiConfig);

			// Load main React shell. URL host "ui" maps to the UI/ folder
			// deployed next to the exe (Windows fopen is case-insensitive).
			Dia::Maths::Vector2D size = mWindow->GetSize();
			mUISystem->CreatePage("dia://ui/index.html",
				static_cast<int>(size.X()), static_cast<int>(size.Y()));

			mView.Initialize(mUISystem);

			return Dia::Application::StateObject::OpertionResponse::kImmediate;
		}

		void EditorViewModule::DoUpdate()
		{
			if (mWindow)
				Dia::Window::PumpNativeMessages(mWindow);

			if (mUISystem)
				mUISystem->Update();
		}

		void EditorViewModule::DoStop()
		{
			mView.Shutdown();

			if (mUISystem)
			{
				Dia::UICEF::DestroyEditorUISystem(mUISystem);
				mUISystem = nullptr;
			}

			if (mWindow)
			{
				mWindow->Close();
				Dia::Window::DestroyNativeWindow(mWindow);
				mWindow = nullptr;
			}
		}
	}
}
