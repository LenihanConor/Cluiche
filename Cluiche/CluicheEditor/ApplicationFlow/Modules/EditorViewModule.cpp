#include "EditorViewModule.h"
#include "EditorModelModule.h"
#include "EditorViewControllerModule.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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
			, mWindow(nullptr)
			, mUISystem(nullptr)
		{
		}

		EditorViewModule::~EditorViewModule()
		{
		}

		void EditorViewModule::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
		{
			AddDependancy(buildDependencies->GetModule(EditorModelModule::kTypeId));
			AddDependancy(buildDependencies->GetModule(EditorViewControllerModule::kTypeId));
		}

		Dia::Application::StateObject::OpertionResponse EditorViewModule::DoStart(const Dia::Application::StateObject::IStartData*)
		{
			EditorModelModule* modelModule = GetModule<EditorModelModule>();
			EditorViewControllerModule* controllerModule = GetModule<EditorViewControllerModule>();

			Dia::Editor::EditorModel* model = &modelModule->GetModel();
			Dia::Editor::EditorViewController* controller = &controllerModule->GetController();

			Dia::Window::IWindow::Settings::Dimensions dims(1280, 720);
			Dia::Window::IWindow::Settings::Style style;
			Dia::Core::Containers::String64 title("Cluiche Editor");
			Dia::Window::IWindow::Settings settings(title, dims, style);

			Dia::Window::WindowCloseCallback onClose = [model]() { model->RequestClose(); };
			mWindow = Dia::Window::CreateNativeWindow(settings, onClose);

			Dia::UICEF::EditorUISystemConfig uiConfig;
			uiConfig.subprocessPath = "CluicheEditor.exe";
			uiConfig.assetBasePath = "";
			uiConfig.windowedRendering = true;
			mUISystem = Dia::UICEF::CreateEditorUISystem(mWindow, uiConfig);

			Dia::Maths::Vector2D size = mWindow->GetSize();
			mUISystem->CreatePage("dia://ui/index.html",
				static_cast<int>(size.X()), static_cast<int>(size.Y()));

			mView.Initialize(mUISystem, controller);

			Dia::Window::IWindow* win = mWindow;
			Dia::Window::SetNativeResizeCallback(mWindow, [win](int w, int h) {
				HWND parent = static_cast<HWND>(win->GetSystemHandle());
				HWND child = FindWindowExW(parent, nullptr, nullptr, nullptr);
				if (child)
					MoveWindow(child, 0, 0, w, h, TRUE);
			});

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
