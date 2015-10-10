////////////////////////////////////////////////////////////////////////////////
// Filename: AwesomiumUISystem.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaUI/IUISystem.h>

namespace Dia
{
	namespace UI
	{
		namespace Awesomium
		{
			class UISystem : public IUISystem
			{
			public:
				UISystem() {};
				virtual ~UISystem() {};

				virtual void LoadScreen() {};
				virtual void OnLoadedScreen() {};
				virtual void IsLoadingScreen() {};
				virtual void UnloadScreen() {};

				virtual void Update() {};

				virtual void FetchTexture() {};

			};
		}
	}
}