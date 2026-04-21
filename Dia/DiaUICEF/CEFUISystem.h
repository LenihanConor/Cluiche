////////////////////////////////////////////////////////////////////////////////
// Filename: CEFUISystem.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaUI/IUISystem.h>

#include <mutex>

namespace Dia
{
	namespace Window
	{
		class IWindow;
	}

	namespace UICEF
	{
		class CEFUISystemImpl;

		class CEFUISystem : public UI::IUISystem
		{
		public:
			CEFUISystem(const Window::IWindow* windowContext);
			virtual ~CEFUISystem();

			virtual void Initialize() override;
			virtual void Shutdown() override;

			virtual void LoadPage(UI::Page& newPage) override;
			virtual void UnloadPage() override;
			virtual bool IsPageLoaded() const override;

			virtual void Update() override;

			virtual void FetchUIDataBuffer(UI::UIDataBuffer& outBuffer) const override;

			virtual UI::IPage* CreatePage(const char* url, int width, int height) override;
			virtual void DestroyPage(UI::IPage* page) override;
			virtual int GetPageCount() const override;

			// Input
			virtual void InjectMouseMove(int x, int y) override;
			virtual void InjectMouseDown(Dia::Input::EMouseButton button, int x, int y) override;
			virtual void InjectMouseUp(Dia::Input::EMouseButton button, int x, int y) override;
			virtual void InjectMouseClick(Dia::Input::EMouseButton button, int x, int y) override;
			virtual void InjectMouseWheel(int scroll_vert, int scroll_horz) override;

			// JS bridge
			virtual void RegisterJSHandler(const char* name, JSHandler handler) override;
			virtual void CallJSFunction(const char* functionName, const char* argsJson) override;

			// CEF-specific configuration (call before Initialize)
			void SetRemoteDebuggingPort(int port);
			void SetCachePath(const char* path);
			void SetAssetBasePath(const char* path);
			void SetSubprocessPath(const char* path);
			void SetWindowedRendering(bool windowed);

		private:
			mutable std::mutex mSystemMutex;
			CEFUISystemImpl* mImpl;
		};
	}
}
