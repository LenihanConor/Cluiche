#include "EditorViewModule.h"
#include "EditorModelModule.h"
#include "EditorViewControllerModule.h"
#include "SplashScreenModule.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/IApplicationControl.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaWindow/NativeWindow.h>
#include <DiaUICEF/EditorUISystemFactory.h>
#include <DiaUI/IUISystem.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaObservation/Log/Logger.h>
#include <DiaCore/Core/Assert.h>
#include <DiaCore/Strings/String64.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Cluiche
{
	namespace Editor
	{
		const Dia::Core::StringCRC EditorViewModule::kTypeId("EditorViewModule");

		EditorViewModule::EditorViewModule(const Dia::Core::StringCRC& instanceId)
			: Dia::ApplicationFlow::Module(instanceId)
			, mWindow(nullptr)
			, mUISystem(nullptr)
			, mRestoreMaximized(false)
			, mModelRef(this, EditorModelModule::kTypeId)
			, mControllerRef(this, EditorViewControllerModule::kTypeId)
			, mSplashRef(this, SplashScreenModule::kTypeId)
		{
		}

		EditorViewModule::~EditorViewModule()
		{
		}

		Dia::ApplicationFlow::StartResult EditorViewModule::DoStart()
		{
			EditorModelModule* modelModule = mModelRef.Get();
			EditorViewControllerModule* controllerModule = mControllerRef.Get();

			Dia::Editor::EditorModel* model = (modelModule != nullptr) ? &modelModule->GetModel() : nullptr;
			Dia::Editor::EditorViewController* controller = (controllerModule != nullptr) ? &controllerModule->GetController() : nullptr;

			const WindowState& ws = modelModule->GetWindowState();
			unsigned int startWidth = ws.hasStoredState ? static_cast<unsigned int>(ws.width) : 1280u;
			unsigned int startHeight = ws.hasStoredState ? static_cast<unsigned int>(ws.height) : 720u;

			Dia::Window::IWindow::Settings::Dimensions dims(startWidth, startHeight);
			Dia::Window::IWindow::Settings::Style style;
			Dia::Core::Containers::String64 title("Cluiche Editor");
			Dia::Window::IWindow::Settings settings(title, dims, style);

			Dia::Window::WindowCloseCallback onClose = [model]()
			{
				if (model != nullptr)
					model->RequestClose();
			};
			mWindow = Dia::Window::CreateNativeWindow(settings, onClose);

			if (ws.hasStoredState)
			{
				HWND hwnd = static_cast<HWND>(mWindow->GetSystemHandle());
				WINDOWPLACEMENT wp = {};
				wp.length = sizeof(WINDOWPLACEMENT);
				GetWindowPlacement(hwnd, &wp);
				wp.rcNormalPosition.left   = ws.x;
				wp.rcNormalPosition.top    = ws.y;
				wp.rcNormalPosition.right  = ws.x + ws.width;
				wp.rcNormalPosition.bottom = ws.y + ws.height;
				wp.showCmd = SW_HIDE;
				SetWindowPlacement(hwnd, &wp);
				mRestoreMaximized = ws.maximized;
			}
			else
			{
				// Keep editor window hidden until the React shell signals it is ready
				ShowWindow(static_cast<HWND>(mWindow->GetSystemHandle()), SW_HIDE);
			}

			Dia::UICEF::EditorUISystemConfig uiConfig;
			uiConfig.subprocessPath    = "CluicheEditor.exe";
			uiConfig.assetBasePath     = "";
			uiConfig.windowedRendering = true;
			mUISystem = Dia::UICEF::CreateEditorUISystem(mWindow, uiConfig);

			Dia::Maths::Vector2D size = mWindow->GetSize();
			mUISystem->CreatePage("dia://ui/index.html",
				static_cast<int>(size.X()), static_cast<int>(size.Y()));

			mView.Initialize(mUISystem, controller);

			// Register console sink now that the view/bridge is ready
			Dia::Editor::WebUIBridge* bridge = mView.GetWebUIBridge();
			mConsoleSink.SetBridge(bridge);
			bridge->RegisterEventHandler(Dia::Core::StringCRC("console_ready"),
				[this](const Json::Value&) { mConsoleSink.NotifyConsoleReady(); });
			Dia::Observation::Log::Logger::Instance().RegisterSink(&mConsoleSink);

			// When the React shell fires "shell_ready" dismiss the native splash
			// and reveal the editor window.
			Dia::Window::IWindow* win = mWindow;
			SplashScreenModule* splashModule = mSplashRef.Get();
			bool restoreMax = mRestoreMaximized;
			mView.GetWebUIBridge()->RegisterEventHandler(
				Dia::Core::StringCRC("shell_ready"),
				[splashModule, win, restoreMax](const Json::Value&)
				{
					if (splashModule)
						splashModule->Dismiss();
					ShowWindow(static_cast<HWND>(win->GetSystemHandle()),
						restoreMax ? SW_SHOWMAXIMIZED : SW_SHOW);
				});

			Dia::Window::SetNativeResizeCallback(mWindow, [win](int w, int h)
			{
				HWND parent = static_cast<HWND>(win->GetSystemHandle());
				HWND child = FindWindowExW(parent, nullptr, nullptr, nullptr);
				if (child)
					MoveWindow(child, 0, 0, w, h, TRUE);
			});

			return Dia::ApplicationFlow::StartResult::kReady;
		}

		void EditorViewModule::DoUpdate(float /*deltaTime*/)
		{
			if (mWindow)
				Dia::Window::PumpNativeMessages(mWindow);

			if (mUISystem)
				mUISystem->Update();

			// Detect close request and trigger application shutdown
			EditorModelModule* modelModule = mModelRef.Get();
			if (modelModule != nullptr && modelModule->GetModel().IsCloseRequested())
			{
				if (auto* app = GetApplication())
					app->RequestShutdown();
			}
		}

		Dia::ApplicationFlow::StopResult EditorViewModule::DoStop()
		{
			// Persist window geometry before tearing down.
			if (mWindow)
			{
				HWND hwnd = static_cast<HWND>(mWindow->GetSystemHandle());
				bool maximized = (IsZoomed(hwnd) != 0);

				WINDOWPLACEMENT wp = {};
				wp.length = sizeof(WINDOWPLACEMENT);
				GetWindowPlacement(hwnd, &wp);
				RECT rc = wp.rcNormalPosition;

				EditorModelModule* modelModule = mModelRef.Get();
				if (modelModule)
					modelModule->SaveWindowState(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, maximized);
			}

			// Unregister console sink
			Dia::Observation::Log::Logger::Instance().UnregisterSink(&mConsoleSink);
			mConsoleSink.SetBridge(nullptr);

			mView.SaveLayoutToDisk();
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

			return Dia::ApplicationFlow::StopResult::kDone;
		}
	}
}

namespace { using EditorViewModule_ = Cluiche::Editor::EditorViewModule; }
DIA_MODULE(EditorViewModule_);
DIA_DESCRIBE(EditorViewModule_::kTypeId, "Renders the editor UI and manages panel layout via the web-based view layer.");
