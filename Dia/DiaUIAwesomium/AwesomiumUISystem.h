////////////////////////////////////////////////////////////////////////////////
// Filename: AwesomiumUISystem.h
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

	namespace UI
	{
		class UIDataBuffer;

		namespace Awesomium
		{
			class UISystemImpl;

			class UISystem : public IUISystem
			{
			public:
				UISystem(const Window::IWindow* windowContext);
				virtual ~UISystem();

				virtual void Initialize() override;

				virtual void LoadPage(Page& newPage) override;
				virtual void OnLoadedPage() override {};
				virtual void IsLoadingPage() override {};
				virtual void UnloadPage() override {};

				virtual void Update() override;

				virtual void FetchUIDataBuffer(UIDataBuffer& outBuffer)const override;

				//Input
				virtual void InjectMouseMove(int x, int y)override;
				virtual void InjectMouseDown(Dia::Input::EMouseButton button, int x, int y)override;
				virtual void InjectMouseUp(Dia::Input::EMouseButton button, int x, int y)override;
				virtual void InjectMouseClick(Dia::Input::EMouseButton button, int x, int y)override;
				virtual void InjectMouseWheel(int scroll_vert, int scroll_horz)override;
			private:
				mutable std::mutex mSystemMutex;	// Mutex to the system to allow multithreading
				UISystemImpl* mUISystemImpl; // Using the impl pattern here to not spread the awesomium includes further afield
			};
		}
	}
}