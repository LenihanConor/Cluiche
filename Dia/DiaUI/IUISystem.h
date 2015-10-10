//////////////////
#pragma once


namespace Dia
{
	namespace UI
	{
		class IUIRendering
		{
		public:
			virtual void FetchTexture() = 0;
		};

		class IUIInput
		{
		public:
		};

		class IUISystem: public IUIRendering, public IUIInput
		{
		public:
			IUISystem() {};
			virtual ~IUISystem() {};

			virtual void LoadScreen() = 0;
			virtual void OnLoadedScreen() = 0;
			virtual void IsLoadingScreen() = 0;
			virtual void UnloadScreen() = 0;	

			virtual void Update() = 0;
		};
	}
}