////////////////////////////////////////////////////////////////////////////////
// Filename: AwesomiumUISystem.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaUI/IUISystem.h>

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

				virtual void LoadScreen() override {};
				virtual void OnLoadedScreen() override {};
				virtual void IsLoadingScreen() override {};
				virtual void UnloadScreen()override {};

				virtual void Update() override;

				virtual void FetchUIDataBuffer(UIDataBuffer& outBuffer)const override;

			private:
				UISystemImpl* mUISystemImpl; // Using the impl pattern here to not spread the awesomium includes further afield
			};
		}
	}
}