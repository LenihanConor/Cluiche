//////////////////
#pragma once

#include "DiaUI/IURendering.h"

namespace Dia
{
	namespace UI
	{
		class IUIInput
		{
		public:
		};

		class IUISystem: public IUIRendering, public IUIInput
		{
		public:
			IUISystem() {};
			virtual ~IUISystem() {};

			virtual void Initialize() = 0;

			virtual void LoadScreen() = 0;
			virtual void OnLoadedScreen() = 0;
			virtual void IsLoadingScreen() = 0;
			virtual void UnloadScreen() = 0;	

			virtual void Update() = 0;
		};
	}
}