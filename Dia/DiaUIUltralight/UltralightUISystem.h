////////////////////////////////////////////////////////////////////////////////
// Filename: UltralightUISystem.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaUI/IUISystem.h>
#include "DiaUIUltralight/UIHandler.h"

#include <mutex>

namespace Dia
{
	namespace Window
	{
		class IWindow;
	}

	namespace UI
	{
		class UIDataBuffer;

		namespace Ultralight
		{
			class UISystemImpl;

			class UISystem : public IUISystem
			{
			public:
				UISystem(const Window::IWindow* windowContext);
				virtual ~UISystem();

				virtual void Initialize() override;
				virtual void Shutdown() override;

				virtual void LoadPage(Page& newPage) override;
				virtual void UnloadPage() override;
				virtual bool IsPageLoaded() const override;

				virtual void Update() override;

				virtual void FetchUIDataBuffer(UIDataBuffer& outBuffer) const override;

				virtual IPage* CreatePage(const char* url, int width, int height) override;
				virtual void DestroyPage(IPage* page) override;
				virtual int GetPageCount() const override;

				// Input
				virtual void InjectMouseMove(int x, int y) override;
				virtual void InjectMouseDown(Dia::Input::EMouseButton button, int x, int y) override;
				virtual void InjectMouseUp(Dia::Input::EMouseButton button, int x, int y) override;
				virtual void InjectMouseClick(Dia::Input::EMouseButton button, int x, int y) override;
				virtual void InjectMouseWheel(int scroll_vert, int scroll_horz) override;

				UIHandler* GetUIHandler();

			private:
				bool mIsPageLoaded;
				mutable std::mutex mSystemMutex;
				UISystemImpl* mUISystemImpl;
				UIHandler mUIHandler;
			};
		}
	}
}
